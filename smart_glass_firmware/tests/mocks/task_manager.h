#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <stdint.h>

typedef enum {
    TASK_CAMERA = 0,
    TASK_DEPTH,
    TASK_AI_INFERENCE,
    TASK_AI_POSTPROCESS,
    TASK_NAV,
    TASK_DECISION,
    TASK_GPS,
    TASK_AUDIO_IN,
    TASK_AUDIO_OUT,
    TASK_VOICE,
    TASK_SAFETY,
    TASK_BATTERY,
    TASK_WIFI,
    TASK_DB,
    TASK_LOG,
    TASK_SYSTEM,
    TASK_COUNT
} task_id_t;

void task_manager_feed_watchdog(task_id_t id);

#endif
