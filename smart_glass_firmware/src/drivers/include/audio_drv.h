#ifndef AUDIO_DRV_H
#define AUDIO_DRV_H

#include <stdint.h>

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_FRAME_SAMPLES 480  /* 30 ms @ 16 kHz */
#define AUDIO_BITS_PER_SAMPLE 16

typedef enum {
    AUDIO_DIR_INPUT,
    AUDIO_DIR_OUTPUT,
    AUDIO_DIR_DUPLEX
} audio_dir_t;

typedef struct {
    audio_dir_t dir;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
} audio_drv_config_t;

typedef void (*audio_rx_callback_t)(int16_t *data, uint32_t num_samples);

int audio_drv_init(const audio_drv_config_t *config);
int audio_drv_start_rx(audio_rx_callback_t cb);
int audio_drv_stop_rx(void);
int audio_drv_start_tx(const int16_t *data, uint32_t num_samples);
int audio_drv_stop_tx(void);
int audio_drv_set_volume(uint8_t percent);
int audio_drv_deinit(void);

#endif /* AUDIO_DRV_H */
