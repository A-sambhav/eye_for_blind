#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#include <stdint.h>
#include <stdbool.h>

#define VOICE_SAMPLE_RATE 16000
#define VOICE_FRAME_MS 30
#define VOICE_FRAME_SAMPLES (VOICE_SAMPLE_RATE * VOICE_FRAME_MS / 1000)
#define VOICE_COMMAND_MAX_LEN 32

typedef enum {
    kStateWakeWord,
    kStateCommand,
    kStateIdle
} voice_state_t;

typedef struct {
    char command[VOICE_COMMAND_MAX_LEN];
    float confidence;
    uint8_t intent_id;
    uint32_t timestamp_us;
    bool wake_word_detected;
} voice_cmd_t;

typedef struct {
    float wake_threshold;
    float command_threshold;
    uint8_t vad_level;
    bool denoise_enabled;
    uint32_t audio_timeout_ms;
} voice_config_t;

typedef enum {
    VOICE_OK = 0,
    VOICE_ERR_INIT,
    VOICE_ERR_AUDIO,
    VOICE_ERR_INFER,
    VOICE_ERR_TIMEOUT
} voice_status_t;

voice_status_t voice_init(const voice_config_t *config);
voice_status_t voice_start_listening(void);
voice_status_t voice_stop_listening(void);
voice_status_t voice_set_wake_word(const char *wake_word);
voice_status_t voice_get_last_command(voice_cmd_t *out);
voice_status_t voice_deinit(void);

#endif /* VOICE_RECOGNITION_H */
