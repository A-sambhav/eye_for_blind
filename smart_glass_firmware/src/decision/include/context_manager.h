#ifndef CONTEXT_MANAGER_H
#define CONTEXT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "scene_understanding.h"

#define CONTEXT_HISTORY_SIZE 32

typedef enum {
    kUserStationary,
    kUserWalking,
    kUserRunning,
    kUserSitting,
    kUserLyingDown,
    kUserFalling,
    kUserUnknown
} user_state_t;

typedef struct {
    float avg_speed_mps;
    uint8_t step_rate;
    float heading_deg;
    bool using_gps;
    bool using_dr;
} location_context_t;

typedef struct {
    scene_type_t scene_type;
    scene_type_t prev_scene_type;
    user_state_t user_state;
    location_context_t location;
    char location_name[64];
    uint32_t time_at_location_ms;
    uint32_t time_since_transition_ms;
    uint8_t time_of_day;
    bool is_indoor;
    bool is_familiar_location;
    uint32_t timestamp_us;
    float ambient_light;
} context_msg_t;

typedef struct {
    uint32_t history_size;
    bool enable_location_naming;
} context_config_t;

typedef enum {
    CONTEXT_OK = 0,
    CONTEXT_ERR_NOT_INIT
} context_status_t;

context_status_t context_init(const context_config_t *config);
context_status_t context_process(const scene_desc_t *scene,
                                  context_msg_t **out_context);
context_status_t context_get_current_state(user_state_t *out_state);
context_status_t context_get_location_type(uint8_t *out_type);
context_status_t context_is_moving(bool *out_moving);
context_status_t context_deinit(void);

#endif /* CONTEXT_MANAGER_H */
