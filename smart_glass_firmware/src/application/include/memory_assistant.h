#ifndef MEMORY_ASSISTANT_H
#define MEMORY_ASSISTANT_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_ENROLLED_FACES 50
#define FACE_EMBEDDING_DIM 128
#define MEMORY_TEXT_LEN 256
#define PERSON_NAME_LEN 64

typedef struct {
    char name[PERSON_NAME_LEN];
    float embedding[FACE_EMBEDDING_DIM];
    char memory[MEMORY_TEXT_LEN];
    uint32_t last_seen_timestamp;
    uint32_t frequency_count;
    float familiarity_score;
    uint8_t relationship_type;
    bool is_trusted;
} person_info_t;

typedef struct {
    uint32_t person_id;
    char person_name[PERSON_NAME_LEN];
    char memory[MEMORY_TEXT_LEN];
    float confidence;
    uint32_t timestamp_us;
    bool disorientation_detected;
    char routine_suggestion[256];
} memory_hint_msg_t;

typedef struct {
    float face_recognition_threshold;
    bool enable_disorientation_check;
    float wandering_radius_m;
    uint32_t routine_check_interval_ms;
    uint8_t max_faces;
} mem_config_t;

typedef enum {
    MEM_OK = 0,
    MEM_ERR_NOT_INIT,
    MEM_ERR_MAX_FACES,
    MEM_ERR_NO_MATCH,
    MEM_ERR_DB
} mem_status_t;

mem_status_t mem_assist_init(const mem_config_t *config);
mem_status_t mem_assist_process_face(const uint8_t *face_embedding,
                                      uint32_t embedding_len,
                                      memory_hint_msg_t **out_hint);
mem_status_t mem_assist_enroll_face(const char *name,
                                     const uint8_t *face_embedding,
                                     uint32_t embedding_len);
mem_status_t mem_assist_get_person_info(uint32_t person_id,
                                         person_info_t *out);
mem_status_t mem_assist_check_disorientation(double lat, double lon,
                                              bool *out_disoriented);
mem_status_t mem_assist_get_routine_step(char *out_step, size_t step_len);
mem_status_t mem_assist_deinit(void);

#endif /* MEMORY_ASSISTANT_H */
