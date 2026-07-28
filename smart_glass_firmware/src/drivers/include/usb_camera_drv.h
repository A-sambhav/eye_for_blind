#ifndef USB_CAMERA_DRV_H
#define USB_CAMERA_DRV_H

#include <stdint.h>
#include <stddef.h>

/* Lowest-level driver for the USB depth/RGB camera module referenced in
 * HW-ARCH-001.md. Returns 0 on success, negative errno-style value on
 * failure. This layer talks directly to the USB host controller; callers
 * should go through camera_hal.h rather than this header directly. */

int usb_camera_drv_init(uint16_t width, uint16_t height, uint8_t fps);
int usb_camera_drv_start(void);
int usb_camera_drv_stop(void);

/* Blocking read of one frame into `buf` (buf_size must match the
 * configured width*height for the active stream). Returns 0 on success. */
int usb_camera_drv_read_frame(uint8_t *buf, size_t buf_size);

#endif /* USB_CAMERA_DRV_H */
