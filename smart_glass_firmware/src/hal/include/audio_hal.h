#ifndef AUDIO_HAL_H
#define AUDIO_HAL_H

#include <stdint.h>
#include <stdbool.h>

#define AUDIO_HAL_FRAME_SAMPLES 480

typedef enum {
    AUDIO_HAL_OK = 0,
    AUDIO_HAL_ERR_INIT,
    AUDIO_HAL_ERR_STREAM,
    AUDIO_HAL_ERR_UNDERRUN
} audio_hal_status_t;

typedef struct {
    bool enable_mic;
    bool enable_speaker;
    uint8_t volume_pct;
} audio_hal_config_t;

typedef void (*audio_hal_rx_cb_t)(const int16_t *samples, uint32_t count);

audio_hal_status_t audio_hal_init(const audio_hal_config_t *config);
audio_hal_status_t audio_hal_start_capture(audio_hal_rx_cb_t cb);
audio_hal_status_t audio_hal_stop_capture(void);
audio_hal_status_t audio_hal_play(const int16_t *samples, uint32_t count);
audio_hal_status_t audio_hal_stop_playback(void);
audio_hal_status_t audio_hal_set_volume(uint8_t pct);
bool audio_hal_is_playing(void);
audio_hal_status_t audio_hal_deinit(void);

#endif /* AUDIO_HAL_H */
