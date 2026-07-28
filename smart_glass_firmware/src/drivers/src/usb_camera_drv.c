#include <string.h>
#include "usb_camera_drv.h"
#include "message_types.h"
#include "FreeRTOS.h"
#include "task.h"

#define UVC_HEADER_LEN 12
#define MAX_FRAME_SIZE (DEPTH_FULL_W * DEPTH_FULL_H * 3)

static struct {
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    bool streaming;
    uint32_t frame_count;
    bool initialized;
} cam;

int usb_camera_drv_init(uint16_t width, uint16_t height, uint8_t fps)
{
    memset(&cam, 0, sizeof(cam));
    cam.width = width > 0 ? width : DEPTH_FULL_W;
    cam.height = height > 0 ? height : DEPTH_FULL_H;
    cam.fps = fps > 0 ? fps : 30;
    cam.initialized = true;
    return 0;
}

int usb_camera_drv_start(void)
{
    if (!cam.initialized) return -1;
    cam.streaming = true;
    cam.frame_count = 0;
    return 0;
}

int usb_camera_drv_stop(void)
{
    cam.streaming = false;
    return 0;
}

int usb_camera_drv_read_frame(uint8_t *buf, size_t buf_size)
{
    if (!cam.initialized || buf == NULL) return -1;
    if (!cam.streaming) return -1;

    size_t expected = (size_t)cam.width * cam.height * 3;
    if (buf_size < expected) return -1;

    uint32_t t = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;

    for (uint32_t y = 0; y < cam.height; y++) {
        for (uint32_t x = 0; x < cam.width; x++) {
            uint32_t idx = (y * cam.width + x) * 3;
            uint8_t r = (uint8_t)((x * 37 + y * 53 + t / 1000) & 0xFF);
            uint8_t g = (uint8_t)((x * 41 + y * 59 + t / 1000 + 80) & 0xFF);
            uint8_t b = (uint8_t)((x * 43 + y * 61 + t / 1000 + 160) & 0xFF);
            buf[idx] = r;
            buf[idx + 1] = g;
            buf[idx + 2] = b;
        }
    }

    cam.frame_count++;
    return 0;
}
