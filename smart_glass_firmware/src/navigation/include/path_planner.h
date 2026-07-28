#ifndef PATH_PLANNER_H
#define PATH_PLANNER_H

#include <stdint.h>
#include <stdbool.h>
#include "navigation_engine.h"
#include "object_tracking.h"

#define PATH_MAX_WAYPOINTS 128
#define PATH_GRID_SIZE 100

typedef struct {
    float x, y;
    float heading_deg;
    bool is_critical;
} path_waypoint_t;

typedef struct {
    path_waypoint_t waypoints[PATH_MAX_WAYPOINTS];
    uint32_t count;
    float total_length_m;
    float min_corridor_width_m;
    uint32_t timestamp_us;
    uint32_t plan_id;
} path_msg_t;

typedef struct {
    float grid_cell_size_m;
    float safe_corridor_min_m;
    float obstacle_inflation_radius_m;
    uint16_t max_path_iterations;
    bool enable_smoothing;
} path_config_t;

typedef enum {
    PATH_OK = 0,
    PATH_ERR_NOT_INIT,
    PATH_ERR_NO_PATH,
    PATH_ERR_TOO_NARROW,
    PATH_ERR_INVALID_PARAM
} path_status_t;

path_status_t path_planner_init(const path_config_t *config);
path_status_t path_planner_plan(nav_position_t *start,
                                 nav_position_t *goal,
                                 path_msg_t **out_path);
path_status_t path_planner_replan(path_msg_t *current_path,
                                   const tracked_obj_msg_t *obstacles,
                                   path_msg_t **out_new_path);
path_status_t path_planner_get_safe_corridor(float *width, float *bearing);
path_status_t path_planner_set_cost_map(uint8_t *grid);
path_status_t path_planner_deinit(void);

#endif /* PATH_PLANNER_H */
