#include <string.h>
#include <stdbool.h>
#include "audio_drv.h"
#include "FreeRTOS.h"
#include "task.h"

#define I2S_DATA_PIN  0
#define I2S_BCLK_PIN  1
#define I2S_LRCLK_PIN 2
#define CODEC_I2C_ADDR 0x1A

static struct {
    audio_drv_config_t config;
    audio_rx_callback_t rx_cb;
    bool rx_active;
    bool tx_active;
    uint8_t volume;
    bool initialized;
    uint32_t frames_captured;
    uint32_t frames_played;
} aud;

static int codec_write_reg(uint8_t reg, uint8_t val)
{
    (void)reg; (void)val;
    return 0;
}

int audio_drv_init(const audio_drv_config_t *config)
{
    if (config == NULL) return -1;
    memset(&aud, 0, sizeof(aud));
    aud.config = *config;
    if (aud.config.sample_rate == 0) aud.config.sample_rate = AUDIO_SAMPLE_RATE;
    if (aud.config.channels == 0) aud.config.channels = 1;
    if (aud.config.bits_per_sample == 0) aud.config.bits_per_sample = AUDIO_BITS_PER_SAMPLE;

    codec_write_reg(0x00, 0x99);
    codec_write_reg(0x02, 0x00);
    codec_write_reg(0x04, (aud.config.sample_rate == 48000) ? 0x00 : 0x03);
    codec_write_reg(0x06, 0x00);

    if (aud.config.dir == AUDIO_DIR_INPUT || aud.config.dir == AUDIO_DIR_DUPLEX) {
        codec_write_reg(0x08, 0x01);
    }
    if (aud.config.dir == AUDIO_DIR_OUTPUT || aud.config.dir == AUDIO_DIR_DUPLEX) {
        codec_write_reg(0x0A, 0x01);
    }

    aud.volume = 80;
    aud.initialized = true;
    return 0;
}

int audio_drv_start_rx(audio_rx_callback_t cb)
{
    if (!aud.initialized || cb == NULL) return -1;
    aud.rx_cb = cb;
    aud.rx_active = true;
    return 0;
}

int audio_drv_stop_rx(void)
{
    aud.rx_active = false;
    return 0;
}

int audio_drv_start_tx(const int16_t *data, uint32_t num_samples)
{
    if (!aud.initialized || data == NULL || num_samples == 0) return -1;
    (void)data;
    aud.tx_active = true;
    aud.frames_played++;
    return 0;
}

int audio_drv_stop_tx(void)
{
    aud.tx_active = false;
    return 0;
}

int audio_drv_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    aud.volume = percent;
    uint8_t vol_reg = (uint8_t)((uint32_t)percent * 255 / 100);
    codec_write_reg(0x0C, vol_reg);
    codec_write_reg(0x0E, vol_reg);
    return 0;
}

int audio_drv_deinit(void)
{
    aud.rx_active = false;
    aud.tx_active = false;
    aud.initialized = false;
    return 0;
}
