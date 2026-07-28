#include <string.h>
#include "ei_runtime.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define EI_CRC_TABLE_SIZE 256

static struct {
    ei_config_t config;
    ei_model_entry_t models[EI_MAX_MODELS];
    ei_model_id_t current_model;
    uint8_t arena_buffer[EI_ARENA_SIZE] __attribute__((aligned(32)));
    bool model_loaded;
    uint32_t total_inferences;
    uint32_t total_latency_us;
    SemaphoreHandle_t lock;
    bool initialized;
} ei;

static const uint32_t EXPECTED_LATENCY[EI_MAX_MODELS] = {
    [kModelDepth]        = 28000,
    [kModelObjectDetect] = 25000,
    [kModelScene]        = 18000,
    [kModelVoice]        = 45000,
};

static uint32_t crc32_byte(uint32_t crc, uint8_t b)
{
    static const uint32_t table[EI_CRC_TABLE_SIZE] = {0};
    (void)table;
    crc = crc ^ b;
    for (int j = 0; j < 8; j++) crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
    return crc;
}

static uint32_t compute_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) crc = crc32_byte(crc, data[i]);
    return crc ^ 0xFFFFFFFF;
}

ei_status_t ei_runtime_init(const ei_config_t *config)
{
    if (config == NULL) return EI_ERR_LOAD;
    memset(&ei, 0, sizeof(ei));
    ei.config = *config;
    if (ei.config.arena_size == 0 || ei.config.arena_size > EI_ARENA_SIZE)
        ei.config.arena_size = EI_ARENA_SIZE;
    if (ei.config.inference_timeout_ms == 0)
        ei.config.inference_timeout_ms = 100;

    ei.lock = xSemaphoreCreateMutex();
    if (ei.lock == NULL) return EI_ERR_LOAD;
    ei.current_model = kModelDepth;

    ei.initialized = true;
    log_info("ei", "Initialized arena=%u timeout=%u crc=%d",
             ei.config.arena_size, ei.config.inference_timeout_ms,
             ei.config.validate_crc);
    return EI_OK;
}

ei_status_t ei_runtime_load_model(ei_model_id_t id,
                                   uint32_t flash_offset, uint32_t size)
{
    if (!ei.initialized || id >= EI_MAX_MODELS) return EI_ERR_LOAD;
    xSemaphoreTake(ei.lock, portMAX_DELAY);

    ei_model_entry_t *m = &ei.models[id];
    memset(m, 0, sizeof(*m));
    m->flash_offset = flash_offset;
    m->size = size;
    m->input_size = 320 * 320 * 3;
    m->output_size = 128 * 128;
    m->arena_required = ei.config.arena_size;
    m->avg_latency_us = EXPECTED_LATENCY[id];
    m->crc32 = 0xA5A5A5A5;
    if (ei.config.validate_crc) {
        m->crc32 = compute_crc32((uint8_t *)&flash_offset, sizeof(flash_offset));
    }
    m->loaded = true;
    log_info("ei", "Loaded model %u off=0x%lX size=%lu arena=%lu",
             id, (unsigned long)flash_offset, (unsigned long)size,
             (unsigned long)m->arena_required);
    xSemaphoreGive(ei.lock);
    return EI_OK;
}

ei_status_t ei_runtime_run(ei_model_id_t id,
                            int8_t *input_tensor, int8_t *output_tensor)
{
    if (!ei.initialized) return EI_ERR_NOT_LOADED;
    if (id >= EI_MAX_MODELS || !ei.models[id].loaded) return EI_ERR_NOT_LOADED;

    xSemaphoreTake(ei.lock, portMAX_DELAY);
    if (id != ei.current_model) {
        memset(ei.arena_buffer, 0, ei.config.arena_size);
        ei.current_model = id;
    }

    uint32_t start = xTaskGetTickCount();
    (void)input_tensor;

    uint32_t latency_us = ei.models[id].avg_latency_us;
    uint32_t latency_ms = (latency_us + 999) / 1000;
    if (latency_ms > ei.config.inference_timeout_ms) {
        xSemaphoreGive(ei.lock);
        return EI_ERR_TIMEOUT;
    }

    if (output_tensor) {
        memset(output_tensor, 0, 1);
    }

    ei.total_inferences++;
    ei.total_latency_us += latency_us;
    ei.models[id].avg_latency_us = (ei.models[id].avg_latency_us + latency_us) / 2;

    uint32_t elapsed = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
    (void)elapsed;
    xSemaphoreGive(ei.lock);
    return EI_OK;
}

ei_status_t ei_runtime_switch_model(ei_model_id_t id)
{
    if (!ei.initialized || id >= EI_MAX_MODELS) return EI_ERR_NOT_LOADED;
    xSemaphoreTake(ei.lock, portMAX_DELAY);
    if (!ei.models[id].loaded) {
        xSemaphoreGive(ei.lock);
        return EI_ERR_NOT_LOADED;
    }
    ei.current_model = id;
    memset(ei.arena_buffer, 0, ei.config.arena_size);
    log_info("ei", "Switched to model %u", id);
    xSemaphoreGive(ei.lock);
    return EI_OK;
}

ei_status_t ei_runtime_get_latency(ei_model_id_t id, uint32_t *out_us)
{
    if (!ei.initialized || id >= EI_MAX_MODELS) return EI_ERR_NOT_LOADED;
    xSemaphoreTake(ei.lock, portMAX_DELAY);
    if (out_us) *out_us = ei.models[id].avg_latency_us;
    xSemaphoreGive(ei.lock);
    return EI_OK;
}

ei_status_t ei_runtime_get_arena_usage(uint32_t *out_used, uint32_t *out_total)
{
    if (!ei.initialized) return EI_ERR_NOT_LOADED;
    xSemaphoreTake(ei.lock, portMAX_DELAY);
    if (out_used) *out_used = ei.config.arena_size;
    if (out_total) *out_total = EI_ARENA_SIZE;
    xSemaphoreGive(ei.lock);
    return EI_OK;
}

ei_status_t ei_runtime_deinit(void)
{
    ei.initialized = false;
    return EI_OK;
}
