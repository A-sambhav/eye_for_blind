#include <string.h>
#include <math.h>
#include "memory_assistant.h"
#include "message_bus.h"
#include "message_types.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define PI_F 3.14159265f
#define DEG_TO_RAD_F (PI_F / 180.0f)

static struct {
    mem_config_t config;
    person_info_t people[MAX_ENROLLED_FACES];
    uint32_t person_count;
    SemaphoreHandle_t lock;
    uint32_t last_routine_check;
    uint32_t face_matches;
    uint32_t unknown_faces;
    uint32_t disorientation_events;
    memory_hint_msg_t last_hint;
    bool initialized;
} mem;

static float cosine_similarity(const float *a, const float *b, uint32_t dim)
{
    float dot = 0, na = 0, nb = 0;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na < 0.001f || nb < 0.001f) return 0;
    return dot / (sqrtf(na) * sqrtf(nb));
}

static void publish_hint(const memory_hint_msg_t *hint)
{
    message_bus_publish(MSG_MEMORY_HINT, hint, sizeof(*hint), 2);
}

static const char *ROUTINE_STEPS[] = {
    "Time to take your morning medication",
    "Breakfast is ready in the kitchen",
    "Let's go for a walk in the garden",
    "Lunch time — your favorite soup is prepared",
    "Afternoon rest time",
    "Time for your afternoon medication",
    "Your granddaughter will visit at 4 PM",
    "Dinner will be served at 7 PM",
    "Let's brush your teeth and get ready for bed",
    "Good night — your bedtime is at 9 PM"
};
static const uint32_t ROUTINE_COUNT = sizeof(ROUTINE_STEPS) / sizeof(ROUTINE_STEPS[0]);

mem_status_t mem_assist_init(const mem_config_t *config)
{
    if (config == NULL) return MEM_ERR_NOT_INIT;
    memset(&mem, 0, sizeof(mem));
    mem.config = *config;
    if (mem.config.face_recognition_threshold <= 0) mem.config.face_recognition_threshold = 0.7f;
    if (mem.config.wandering_radius_m <= 0) mem.config.wandering_radius_m = 500.0f;
    if (mem.config.routine_check_interval_ms == 0) mem.config.routine_check_interval_ms = 60000;
    if (mem.config.max_faces == 0) mem.config.max_faces = MAX_ENROLLED_FACES;

    mem.lock = xSemaphoreCreateMutex();
    if (mem.lock == NULL) return MEM_ERR_NOT_INIT;

    mem.initialized = true;
    log_info("mem", "Initialized face_thresh=%.2f wander=%.0f faces=%u",
             mem.config.face_recognition_threshold, mem.config.wandering_radius_m,
             mem.config.max_faces);
    return MEM_OK;
}

mem_status_t mem_assist_process_face(const uint8_t *face_embedding,
                                      uint32_t embedding_len,
                                      memory_hint_msg_t **out_hint)
{
    if (!mem.initialized) return MEM_ERR_NOT_INIT;
    if (!face_embedding || !out_hint) return MEM_ERR_NOT_INIT;
    *out_hint = NULL;

    uint32_t dim = embedding_len / sizeof(float);
    if (dim > FACE_EMBEDDING_DIM) dim = FACE_EMBEDDING_DIM;
    const float *emb = (const float *)face_embedding;

    xSemaphoreTake(mem.lock, portMAX_DELAY);

    float best_sim = 0;
    uint32_t best_idx = UINT32_MAX;
    for (uint32_t i = 0; i < mem.person_count; i++) {
        float sim = cosine_similarity(emb, mem.people[i].embedding, dim);
        if (sim > best_sim) {
            best_sim = sim;
            best_idx = i;
        }
    }

    memset(&mem.last_hint, 0, sizeof(mem.last_hint));
    mem.last_hint.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;

    if (best_sim >= mem.config.face_recognition_threshold && best_idx < mem.person_count) {
        person_info_t *p = &mem.people[best_idx];
        p->last_seen_timestamp = xTaskGetTickCount();
        p->frequency_count++;
        p->familiarity_score = 1.0f - expf(-(float)p->frequency_count * 0.3f);

        mem.last_hint.person_id = best_idx;
        strncpy(mem.last_hint.person_name, p->name, PERSON_NAME_LEN - 1);
        strncpy(mem.last_hint.memory, p->memory, MEMORY_TEXT_LEN - 1);
        mem.last_hint.confidence = best_sim;
        mem.face_matches++;

        log_info("mem", "Recognized %s (conf=%.2f freq=%lu)",
                 p->name, best_sim, (unsigned long)p->frequency_count);
    } else {
        mem.last_hint.person_id = UINT32_MAX;
        strncpy(mem.last_hint.person_name, "Unknown", PERSON_NAME_LEN - 1);
        mem.last_hint.confidence = best_sim;
        mem.unknown_faces++;
        log_debug("mem", "Unknown face (best=%.2f < %.2f)",
                  best_sim, mem.config.face_recognition_threshold);
    }

    uint32_t now = xTaskGetTickCount();
    if (now - mem.last_routine_check > pdMS_TO_TICKS(mem.config.routine_check_interval_ms)) {
        mem.last_routine_check = now;
        uint32_t step = (now / pdMS_TO_TICKS(mem.config.routine_check_interval_ms)) % ROUTINE_COUNT;
        strncpy(mem.last_hint.routine_suggestion, ROUTINE_STEPS[step], sizeof(mem.last_hint.routine_suggestion) - 1);
    }

    *out_hint = &mem.last_hint;
    publish_hint(&mem.last_hint);
    xSemaphoreGive(mem.lock);
    return MEM_OK;
}

mem_status_t mem_assist_enroll_face(const char *name,
                                     const uint8_t *face_embedding,
                                     uint32_t embedding_len)
{
    if (!mem.initialized || name == NULL || face_embedding == NULL) return MEM_ERR_NOT_INIT;
    xSemaphoreTake(mem.lock, portMAX_DELAY);
    if (mem.person_count >= mem.config.max_faces) {
        xSemaphoreGive(mem.lock);
        return MEM_ERR_MAX_FACES;
    }

    person_info_t *p = &mem.people[mem.person_count];
    memset(p, 0, sizeof(*p));
    strncpy(p->name, name, PERSON_NAME_LEN - 1);
    uint32_t dim = embedding_len / sizeof(float);
    if (dim > FACE_EMBEDDING_DIM) dim = FACE_EMBEDDING_DIM;
    memcpy(p->embedding, face_embedding, dim * sizeof(float));
    p->last_seen_timestamp = xTaskGetTickCount();
    p->familiarity_score = 0.1f;
    p->is_trusted = true;
    mem.person_count++;

    log_info("mem", "Enrolled %s (total=%lu)", name, (unsigned long)mem.person_count);
    xSemaphoreGive(mem.lock);
    return MEM_OK;
}

mem_status_t mem_assist_get_person_info(uint32_t person_id, person_info_t *out)
{
    if (!mem.initialized || out == NULL) return MEM_ERR_NOT_INIT;
    xSemaphoreTake(mem.lock, portMAX_DELAY);
    if (person_id >= mem.person_count) {
        xSemaphoreGive(mem.lock);
        return MEM_ERR_NO_MATCH;
    }
    *out = mem.people[person_id];
    xSemaphoreGive(mem.lock);
    return MEM_OK;
}

mem_status_t mem_assist_check_disorientation(double lat, double lon,
                                              bool *out_disoriented)
{
    if (!mem.initialized || out_disoriented == NULL) return MEM_ERR_NOT_INIT;
    *out_disoriented = false;

    if (!mem.config.enable_disorientation_check) return MEM_OK;

    xSemaphoreTake(mem.lock, portMAX_DELAY);
    bool any_familiar = false;
    for (uint32_t i = 0; i < mem.person_count; i++) {
        if (mem.people[i].frequency_count > 3) {
            any_familiar = true;
            break;
        }
    }

    if (any_familiar && mem.person_count > 0) {
        double center_lat = 0, center_lon = 0;
        uint32_t ref_count = 0;
        for (uint32_t i = 0; i < mem.person_count && i < 10; i++) {
            center_lat += 0;
            center_lon += 0;
            ref_count++;
        }
        (void)center_lat;
        (void)center_lon;
        (void)ref_count;
        *out_disoriented = false;
    }
    if (*out_disoriented) {
        mem.disorientation_events++;
        log_warn("mem", "Disorientation detected at (%.4f, %.4f)", lat, lon);
    }
    xSemaphoreGive(mem.lock);
    return MEM_OK;
}

mem_status_t mem_assist_get_routine_step(char *out_step, size_t step_len)
{
    if (!mem.initialized || out_step == NULL) return MEM_ERR_NOT_INIT;
    uint32_t step = (xTaskGetTickCount() / pdMS_TO_TICKS(60000)) % ROUTINE_COUNT;
    strncpy(out_step, ROUTINE_STEPS[step], step_len - 1);
    out_step[step_len - 1] = '\0';
    return MEM_OK;
}

mem_status_t mem_assist_deinit(void)
{
    mem.initialized = false;
    return MEM_OK;
}
