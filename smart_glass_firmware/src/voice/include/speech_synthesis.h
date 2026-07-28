#ifndef SPEECH_SYNTHESIS_H
#define SPEECH_SYNTHESIS_H

#include <stdint.h>
#include <stdbool.h>

#define SPEECH_MAX_TEXT_LEN 256
#define SPEECH_QUEUE_DEPTH 8

typedef struct {
    char text[SPEECH_MAX_TEXT_LEN];
    uint8_t priority;
    uint32_t duration_ms;
    uint32_t timestamp_us;
} speech_request_t;

typedef struct {
    uint8_t volume_percent;
    float speed;
    uint8_t voice_model;
    bool muted;
} speech_config_t;

typedef enum {
    SPEECH_OK = 0,
    SPEECH_ERR_INIT,
    SPEECH_ERR_TTS,
    SPEECH_ERR_QUEUE_FULL,
    SPEECH_ERR_DMA
} speech_status_t;

speech_status_t speech_init(const speech_config_t *config);
speech_status_t speech_speak(const char *text, uint8_t priority);
speech_status_t speech_interrupt(const char *text);
speech_status_t speech_set_volume(uint8_t volume_percent);
speech_status_t speech_mute(bool mute);
bool speech_is_speaking(void);
speech_status_t speech_get_queue_depth(uint8_t *out_depth);
speech_status_t speech_deinit(void);

#endif /* SPEECH_SYNTHESIS_H */
