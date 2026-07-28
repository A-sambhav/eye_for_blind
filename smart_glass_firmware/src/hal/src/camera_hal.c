#include <string.h>
#include "camera_hal.h"
#include "usb_camera_drv.h"
#include "message_bus.h"
#include "message_types.h"
#include "task_manager.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define CAMERA_TASK_PERIOD_MS 33               /* 30 fps */
#define CAMERA_HALF_W (CAMERA_DEPTH_WIDTH / 2)
#define CAMERA_HALF_H (CAMERA_DEPTH_HEIGHT / 2)

static struct {
    camera_hal_config_t config;
    SemaphoreHandle_t frame_lock;
    uint8_t depth_frame[CAMERA_HALF_W * CAMERA_HALF_H];
    bool depth_frame_fresh;
    bool initialized;
} hal_state;

camera_hal_status_t camera_hal_init(const camera_hal_config_t *config)
{
    if (config == NULL) {
        return CAMERA_HAL_ERR_INIT;
    }

    memset(&hal_state, 0, sizeof(hal_state));
    hal_state.config = *config;
    hal_state.frame_lock = xSemaphoreCreateMutex();
    if (hal_state.frame_lock == NULL) {
        return CAMERA_HAL_ERR_INIT;
    }

    if (usb_camera_drv_init(config->width, config->height, config->fps) != 0) {
        return CAMERA_HAL_ERR_INIT;
    }

    hal_state.initialized = true;
    return CAMERA_HAL_OK;
}

camera_hal_status_t camera_hal_start_stream(void)
{
    if (!hal_state.initialized) {
        return CAMERA_HAL_ERR_NOT_READY;
    }
    return (usb_camera_drv_start() == 0) ? CAMERA_HAL_OK : CAMERA_HAL_ERR_TIMEOUT;
}

camera_hal_status_t camera_hal_stop_stream(void)
{
    if (!hal_state.initialized) {
        return CAMERA_HAL_ERR_NOT_READY;
    }
    return (usb_camera_drv_stop() == 0) ? CAMERA_HAL_OK : CAMERA_HAL_ERR_TIMEOUT;
}

camera_hal_status_t camera_hal_get_latest_depth_frame(uint8_t *out_buf, size_t buf_size)
{
    if (!hal_state.initialized) {
        return CAMERA_HAL_ERR_NOT_READY;
    }
    if (buf_size < sizeof(hal_state.depth_frame)) {
        return CAMERA_HAL_ERR_INIT;
    }

    camera_hal_status_t status = CAMERA_HAL_ERR_NOT_READY;
    xSemaphoreTake(hal_state.frame_lock, portMAX_DELAY);
    if (hal_state.depth_frame_fresh) {
        memcpy(out_buf, hal_state.depth_frame, sizeof(hal_state.depth_frame));
        hal_state.depth_frame_fresh = false;
        status = CAMERA_HAL_OK;
    }
    xSemaphoreGive(hal_state.frame_lock);
    return status;
}

camera_hal_status_t camera_hal_get_latest_rgb_frame(uint8_t *out_buf, size_t buf_size)
{
    /* TODO(camera_hal): RGB path shares the same USB isochronous transfer
     * as depth on this sensor per HW-ARCH-001; wire up once
     * usb_camera_drv exposes dual-stream demux. */
    (void)out_buf;
    (void)buf_size;
    return CAMERA_HAL_ERR_NOT_READY;
}

/* --- camera_task: periodic capture + publish, per SW-ARCH-001 19.1 --- */
void camera_task_entry(void *params)
{
    (void)params;
    TickType_t last_wake = xTaskGetTickCount();

    size_t full_sz = CAMERA_DEPTH_WIDTH * CAMERA_DEPTH_HEIGHT * 3;
    uint8_t *raw_frame = pvPortMalloc(full_sz);
    if (raw_frame == NULL) {
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    for (;;) {
        if (usb_camera_drv_read_frame(raw_frame, full_sz) == 0) {
            uint32_t half_w = CAMERA_HALF_W;
            uint32_t half_h = CAMERA_HALF_H;
            uint32_t fw = CAMERA_DEPTH_WIDTH;
            for (uint32_t y = 0; y < half_h; y++)
                for (uint32_t x = 0; x < half_w; x++)
                    hal_state.depth_frame[y * half_w + x] =
                        raw_frame[(y * 2 * fw + x * 2) * 3];
            hal_state.depth_frame_fresh = true;
            message_bus_publish(MSG_RAW_FRAME, NULL, 0, 4);
        }

        task_manager_feed_watchdog(TASK_CAMERA);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CAMERA_TASK_PERIOD_MS));
    }
}
