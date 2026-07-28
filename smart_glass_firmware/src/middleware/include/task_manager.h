#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/* Task table mirrors LLD-001.md section 1.4 Scheduling Model.
 * Stack sizes in words (FreeRTOS convention). */
typedef enum {
    TASK_CAMERA = 0,
    TASK_DEPTH,
    TASK_AI,
    TASK_NAVIGATION,
    TASK_VOICE,
    TASK_DECISION_ENGINE,
    TASK_BMS,
    TASK_SYSTEM,
    TASK_LOG,
    TASK_MSG_BUS_DISPATCH,
    TASK_COUNT
} task_id_t;

typedef void (*task_entry_fn_t)(void *params);

typedef struct {
    task_id_t id;
    const char *name;
    task_entry_fn_t entry;
    uint32_t stack_words;
    uint8_t priority;
    uint32_t period_ms;
    uint32_t deadline_ms;
} task_def_t;

void task_manager_start_all(void);
bool task_manager_all_healthy(void);
void task_manager_feed_watchdog(task_id_t id);
task_def_t *task_manager_get_def(task_id_t id);

#endif /* TASK_MANAGER_H */
