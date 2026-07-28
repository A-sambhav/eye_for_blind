#ifndef CAMERA_HAL_H
#define CAMERA_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CAMERA_DEPTH_WIDTH  640
#define CAMERA_DEPTH_HEIGHT 360
#define CAMERA_RGB_WIDTH    640
#define CAMERA_RGB_HEIGHT   480

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t fps;
} camera_hal_config_t;

typedef enum {
    CAMERA_HAL_OK = 0,
    CAMERA_HAL_ERR_INIT,
    CAMERA_HAL_ERR_NOT_READY,
    CAMERA_HAL_ERR_TIMEOUT
} camera_hal_status_t;

/* Initializes the underlying USB camera driver and configures stream
 * parameters. Must be called during the "Initialize HAL modules" phase
 * of the startup sequence (SW-ARCH-001.md section 20), after
 * usb_camera_drv_init() has run. */
camera_hal_status_t camera_hal_init(const camera_hal_config_t *config);

/* Starts the isochronous USB frame stream. camera_task then pulls frames
 * via camera_hal_get_latest_depth_frame() on its 33ms period. */
camera_hal_status_t camera_hal_start_stream(void);
camera_hal_status_t camera_hal_stop_stream(void);

/* Copies the most recently captured depth frame into `out_buf`
 * (caller-allocated, CAMERA_DEPTH_WIDTH * CAMERA_DEPTH_HEIGHT bytes for
 * 8-bit depth). Returns CAMERA_HAL_ERR_NOT_READY if no frame has arrived
 * since the last call. Non-blocking. */
camera_hal_status_t camera_hal_get_latest_depth_frame(uint8_t *out_buf, size_t buf_size);

/* Same contract, RGB24 frame for object/face/text pipelines. */
camera_hal_status_t camera_hal_get_latest_rgb_frame(uint8_t *out_buf, size_t buf_size);

#endif /* CAMERA_HAL_H */
