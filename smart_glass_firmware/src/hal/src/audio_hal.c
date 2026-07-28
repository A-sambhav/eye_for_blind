#include <string.h>
#include "audio_hal.h"
#include "audio_drv.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define AUDIO_HAL_RX_BUF_COUNT 2
#define AUDIO_HAL_RX_BUF_SAMPLES 480

static struct {
    audio_hal_config_t config;
    audio_hal_rx_cb_t rx_cb;
    int16_t rx_buffers[AUDIO_HAL_RX_BUF_COUNT][AUDIO_HAL_RX_BUF_SAMPLES];
    uint8_t rx_buf_write;
    int16_t playback_buf[AUDIO_HAL_RX_BUF_SAMPLES];
    bool capturing;
    bool playing;
    SemaphoreHandle_t lock;
    bool initialized;
} ah;

static void audio_rx_handler(int16_t *data, uint32_t num_samples)
{
    if (!ah.initialized || !ah.capturing) return;
    if (num_samples > AUDIO_HAL_RX_BUF_SAMPLES)
        num_samples = AUDIO_HAL_RX_BUF_SAMPLES;

    xSemaphoreTake(ah.lock, portMAX_DELAY);
    memcpy(ah.rx_buffers[ah.rx_buf_write], data, num_samples * sizeof(int16_t));
    ah.rx_buf_write = (ah.rx_buf_write + 1) % AUDIO_HAL_RX_BUF_COUNT;
    xSemaphoreGive(ah.lock);

    if (ah.rx_cb)
        ah.rx_cb(data, num_samples);
}

audio_hal_status_t audio_hal_init(const audio_hal_config_t *config)
{
    if (config == NULL) return AUDIO_HAL_ERR_INIT;
    memset(&ah, 0, sizeof(ah));
    ah.config = *config;
    if (ah.config.volume_pct == 0) ah.config.volume_pct = 80;

    ah.lock = xSemaphoreCreateMutex();
    if (ah.lock == NULL) return AUDIO_HAL_ERR_INIT;

    audio_drv_config_t drv_cfg = {
        .dir = AUDIO_DIR_DUPLEX,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .channels = 1,
        .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
    };
    if (audio_drv_init(&drv_cfg) != 0) return AUDIO_HAL_ERR_INIT;
    audio_drv_set_volume(ah.config.volume_pct);
    ah.initialized = true;
    log_info("audio_hal", "Init vol=%d mic=%d spk=%d",
             ah.config.volume_pct, ah.config.enable_mic, ah.config.enable_speaker);
    return AUDIO_HAL_OK;
}

audio_hal_status_t audio_hal_start_capture(audio_hal_rx_cb_t cb)
{
    if (!ah.initialized || cb == NULL) return AUDIO_HAL_ERR_INIT;
    ah.rx_cb = cb;
    ah.capturing = true;
    audio_drv_start_rx(audio_rx_handler);
    log_info("audio_hal", "Capture started");
    return AUDIO_HAL_OK;
}

audio_hal_status_t audio_hal_stop_capture(void)
{
    ah.capturing = false;
    audio_drv_stop_rx();
    return AUDIO_HAL_OK;
}

audio_hal_status_t audio_hal_play(const int16_t *samples, uint32_t count)
{
    if (!ah.initialized || samples == NULL || count == 0)
        return AUDIO_HAL_ERR_INIT;
    if (count > AUDIO_HAL_RX_BUF_SAMPLES) count = AUDIO_HAL_RX_BUF_SAMPLES;
    memcpy(ah.playback_buf, samples, count * sizeof(int16_t));
    ah.playing = true;
    audio_drv_start_tx(ah.playback_buf, count);
    return AUDIO_HAL_OK;
}

audio_hal_status_t audio_hal_stop_playback(void)
{
    ah.playing = false;
    audio_drv_stop_tx();
    return AUDIO_HAL_OK;
}

audio_hal_status_t audio_hal_set_volume(uint8_t pct)
{
    if (!ah.initialized) return AUDIO_HAL_ERR_INIT;
    if (pct > 100) pct = 100;
    ah.config.volume_pct = pct;
    audio_drv_set_volume(pct);
    return AUDIO_HAL_OK;
}

bool audio_hal_is_playing(void)
{
    return ah.playing;
}

audio_hal_status_t audio_hal_deinit(void)
{
    ah.capturing = false;
    ah.playing = false;
    audio_drv_deinit();
    ah.initialized = false;
    return AUDIO_HAL_OK;
}
