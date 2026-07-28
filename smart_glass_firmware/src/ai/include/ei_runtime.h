#ifndef EI_RUNTIME_H
#define EI_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>

#define EI_ARENA_SIZE (64 * 1024)
#define EI_MAX_MODELS 4

typedef enum {
    kModelDepth,
    kModelObjectDetect,
    kModelScene,
    kModelVoice,
    kModelCount
} ei_model_id_t;

typedef enum {
    EI_OK = 0,
    EI_ERR_LOAD,
    EI_ERR_CRC,
    EI_ERR_ARENA_OOM,
    EI_ERR_TIMEOUT,
    EI_ERR_NOT_LOADED
} ei_status_t;

typedef struct {
    uint32_t flash_offset;
    uint32_t size;
    uint32_t crc32;
    uint32_t input_size;
    uint32_t output_size;
    uint32_t arena_required;
    uint32_t avg_latency_us;
    bool loaded;
} ei_model_entry_t;

typedef struct {
    uint32_t arena_size;
    bool validate_crc;
    uint32_t inference_timeout_ms;
} ei_config_t;

ei_status_t ei_runtime_init(const ei_config_t *config);
ei_status_t ei_runtime_load_model(ei_model_id_t id,
                                   uint32_t flash_offset, uint32_t size);
ei_status_t ei_runtime_run(ei_model_id_t id,
                            int8_t *input_tensor, int8_t *output_tensor);
ei_status_t ei_runtime_switch_model(ei_model_id_t id);
ei_status_t ei_runtime_get_latency(ei_model_id_t id, uint32_t *out_us);
ei_status_t ei_runtime_get_arena_usage(uint32_t *out_used,
                                        uint32_t *out_total);
ei_status_t ei_runtime_deinit(void);

#endif /* EI_RUNTIME_H */
