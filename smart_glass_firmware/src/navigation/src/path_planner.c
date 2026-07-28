#include <string.h>
#include <math.h>
#include "path_planner.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"

#define DEG_TO_RAD_F (3.14159265f / 180.0f)

static float deg2rad_f(float d) { return d * DEG_TO_RAD_F; }

#define HEAP_MAX 4096
#define CELL_FREE      0
#define CELL_OBSTACLE  1
#define CELL_INFLATED  2
#define STATE_UNVISITED 0
#define STATE_OPEN      1
#define STATE_CLOSED    2

typedef struct {
    uint16_t x, y;
    uint16_t f;
} heap_node_t;

static struct {
    path_config_t config;
    uint8_t grid[PATH_GRID_SIZE][PATH_GRID_SIZE];
    uint8_t astar_state[PATH_GRID_SIZE][PATH_GRID_SIZE];
    uint16_t astar_g[PATH_GRID_SIZE][PATH_GRID_SIZE];
    uint16_t came_from[PATH_GRID_SIZE][PATH_GRID_SIZE];
    heap_node_t heap[HEAP_MAX];
    uint32_t heap_count;
    path_msg_t current_path;
    uint32_t plan_count;
    uint32_t replan_count;
    bool grid_ready;
    bool initialized;
} pl;

static float grid_cell(void) { return pl.config.grid_cell_size_m > 0 ? pl.config.grid_cell_size_m : 0.1f; }

static float coord_to_grid(float coord_m)
{
    return coord_m / grid_cell() + PATH_GRID_SIZE / 2.0f;
}



static void heap_swap(uint32_t i, uint32_t j)
{
    heap_node_t t = pl.heap[i];
    pl.heap[i] = pl.heap[j];
    pl.heap[j] = t;
}

static void heap_push(uint16_t x, uint16_t y, uint16_t f)
{
    if (pl.heap_count >= HEAP_MAX) return;
    uint32_t i = pl.heap_count++;
    pl.heap[i].x = x;
    pl.heap[i].y = y;
    pl.heap[i].f = f;
    while (i > 0) {
        uint32_t p = (i - 1) / 2;
        if (pl.heap[p].f <= pl.heap[i].f) break;
        heap_swap(i, p);
        i = p;
    }
}

static bool heap_pop(uint16_t *x, uint16_t *y)
{
    if (pl.heap_count == 0) return false;
    *x = pl.heap[0].x;
    *y = pl.heap[0].y;
    pl.heap[0] = pl.heap[--pl.heap_count];
    uint32_t i = 0;
    for (;;) {
        uint32_t smallest = i;
        uint32_t l = 2 * i + 1;
        uint32_t r = 2 * i + 2;
        if (l < pl.heap_count && pl.heap[l].f < pl.heap[smallest].f) smallest = l;
        if (r < pl.heap_count && pl.heap[r].f < pl.heap[smallest].f) smallest = r;
        if (smallest == i) break;
        heap_swap(i, smallest);
        i = smallest;
    }
    return true;
}

static uint16_t heuristic(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    int16_t dx = x1 > x2 ? (int16_t)(x1 - x2) : (int16_t)(x2 - x1);
    int16_t dy = y1 > y2 ? (int16_t)(y1 - y2) : (int16_t)(y2 - y1);
    return (uint16_t)((dx + dy) * 10);
}

static void build_grid(nav_position_t *start, nav_position_t *goal,
                       const tracked_obj_msg_t *obstacles)
{
    (void)start;
    (void)goal;
    memset(pl.grid, 0, sizeof(pl.grid));
    pl.grid_ready = false;

    if (obstacles == NULL) {
        pl.grid_ready = true;
        return;
    }

    float inflate = pl.config.obstacle_inflation_radius_m;
    if (inflate <= 0) inflate = 0.5f;
    uint16_t inflate_cells = (uint16_t)(inflate / grid_cell() + 0.5f);
    if (inflate_cells < 1) inflate_cells = 1;

    for (uint32_t ti = 0; ti < obstacles->active_count && ti < MAX_TRACKS; ti++) {
        const tracked_object_t *trk = &obstacles->tracks[ti];
        if (!trk->active) continue;
        float ox = trk->last_box.pos_x;
        float oy = trk->last_box.pos_y;
        int16_t gx = (int16_t)coord_to_grid(ox);
        int16_t gy = (int16_t)coord_to_grid(oy);
        for (int16_t dx = -(int16_t)inflate_cells; dx <= (int16_t)inflate_cells; dx++) {
            for (int16_t dy = -(int16_t)inflate_cells; dy <= (int16_t)inflate_cells; dy++) {
                int16_t nx = gx + dx;
                int16_t ny = gy + dy;
                float dist = sqrtf((float)(dx * dx + dy * dy));
                if (dist > inflate_cells) continue;
                if (nx >= 0 && nx < PATH_GRID_SIZE && ny >= 0 && ny < PATH_GRID_SIZE) {
                    pl.grid[nx][ny] = CELL_OBSTACLE;
                }
            }
        }
    }
    pl.grid_ready = true;
}

static bool is_free(uint16_t x, uint16_t y)
{
    if (x >= PATH_GRID_SIZE || y >= PATH_GRID_SIZE) return false;
    return pl.grid[x][y] == CELL_FREE;
}

#define DIR_COUNT 8
static const int8_t dir_dx[DIR_COUNT] = {1,1,0,-1,-1,-1,0,1};
static const int8_t dir_dy[DIR_COUNT] = {0,1,1,1,0,-1,-1,-1};
static const uint16_t dir_cost[DIR_COUNT] = {10,14,10,14,10,14,10,14};

static bool astar_search(uint16_t sx, uint16_t sy,
                          uint16_t gx, uint16_t gy,
                          path_waypoint_t *out_wp, uint32_t *out_len)
{
    memset(pl.astar_state, 0, sizeof(pl.astar_state));
    memset(pl.astar_g, 0xFF, sizeof(pl.astar_g));
    memset(pl.came_from, 0xFF, sizeof(pl.came_from));
    pl.heap_count = 0;

    if (!is_free(sx, sy) || !is_free(gx, gy)) {
        log_warn("path", "A* start or goal blocked (%u,%u)->(%u,%u)", sx, sy, gx, gy);
        return false;
    }

    pl.astar_g[sx][sy] = 0;
    pl.astar_state[sx][sy] = STATE_OPEN;
    heap_push(sx, sy, heuristic(sx, sy, gx, gy));

    uint32_t iter = 0;
    uint32_t max_iter = pl.config.max_path_iterations > 0 ? pl.config.max_path_iterations : 10000;

    while (pl.heap_count > 0 && iter < max_iter) {
        iter++;
        uint16_t cx, cy;
        if (!heap_pop(&cx, &cy)) break;
        if (pl.astar_state[cx][cy] == STATE_CLOSED) continue;
        pl.astar_state[cx][cy] = STATE_CLOSED;

        if (cx == gx && cy == gy) break;

        for (int d = 0; d < DIR_COUNT; d++) {
            uint16_t nx = cx + dir_dx[d];
            uint16_t ny = cy + dir_dy[d];
            if (!is_free(nx, ny)) continue;
            if (pl.astar_state[nx][ny] == STATE_CLOSED) continue;

            uint16_t ng = pl.astar_g[cx][cy] + dir_cost[d];
            if (pl.astar_state[nx][ny] == STATE_OPEN && ng >= pl.astar_g[nx][ny]) continue;

            pl.astar_g[nx][ny] = ng;
            pl.came_from[nx][ny] = (uint16_t)d;
            if (pl.astar_state[nx][ny] != STATE_OPEN) {
                pl.astar_state[nx][ny] = STATE_OPEN;
                heap_push(nx, ny, ng + heuristic(nx, ny, gx, gy));
            }
        }
    }

    if (pl.astar_state[gx][gy] != STATE_CLOSED) {
        log_warn("path", "A* no path found (%u iterations)", iter);
        return false;
    }

    uint16_t path_cells[PATH_MAX_WAYPOINTS][2];
    uint32_t pc = 0;
    uint16_t cx = gx, cy = gy;
    while (!(cx == sx && cy == gy) && pc < PATH_MAX_WAYPOINTS) {
        path_cells[pc][0] = cx;
        path_cells[pc][1] = cy;
        pc++;
        uint16_t dir = pl.came_from[cx][cy];
        if (dir == 0xFFFF) break;
        int d = (int)dir;
        cx = cx - dir_dx[d];
        cy = cy - dir_dy[d];
    }
    if (pc == 0) return false;
    path_cells[pc][0] = sx;
    path_cells[pc][1] = sy;
    pc++;

    *out_len = 0;
    for (uint32_t i = 0; i < pc && *out_len < PATH_MAX_WAYPOINTS; i++) {
        uint32_t ri = pc - 1 - i;
        float mx, my;
        float cell = grid_cell();
        mx = ((int16_t)path_cells[ri][0] - PATH_GRID_SIZE / 2) * cell;
        my = ((int16_t)path_cells[ri][1] - PATH_GRID_SIZE / 2) * cell;
        float deg = 0;
        if (ri > 0) {
            deg = atan2f((float)path_cells[ri-1][1] - (float)path_cells[ri][1],
                         (float)path_cells[ri-1][0] - (float)path_cells[ri][0]) * 180.0f / 3.14159f;
        }
        out_wp[*out_len] = (path_waypoint_t){
            .x = mx, .y = my,
            .heading_deg = deg,
            .is_critical = false
        };
        (*out_len)++;
    }
    return true;
}

static uint32_t smooth_path(path_waypoint_t *wp, uint32_t len)
{
    if (len <= 2) return len;
    uint32_t out = 0;
    wp[out++] = wp[0];
    for (uint32_t i = 1; i < len - 1; i++) {
        float dx1 = wp[i].x - wp[out-1].x;
        float dy1 = wp[i].y - wp[out-1].y;
        float dx2 = wp[i+1].x - wp[i].x;
        float dy2 = wp[i+1].y - wp[i].y;
        float dot = dx1*dx2 + dy1*dy2;
        float mag1 = sqrtf(dx1*dx1 + dy1*dy1);
        float mag2 = sqrtf(dx2*dx2 + dy2*dy2);
        float cos_a = (mag1 > 0.01f && mag2 > 0.01f) ? dot / (mag1 * mag2) : 1.0f;
        if (cos_a < 0.995f) {
            wp[out++] = wp[i];
        }
    }
    wp[out++] = wp[len - 1];
    return out;
}

static path_status_t validate_path(path_waypoint_t *wp, uint32_t len)
{
    if (len < 2) return PATH_ERR_NO_PATH;
    float min_w = pl.config.safe_corridor_min_m;
    if (min_w <= 0) min_w = 1.5f;
    return PATH_OK;
}

path_status_t path_planner_init(const path_config_t *config)
{
    if (config == NULL) return PATH_ERR_INVALID_PARAM;
    memset(&pl, 0, sizeof(pl));
    pl.config = *config;
    if (pl.config.grid_cell_size_m <= 0) pl.config.grid_cell_size_m = 0.1f;
    if (pl.config.safe_corridor_min_m <= 0) pl.config.safe_corridor_min_m = 1.5f;
    if (pl.config.obstacle_inflation_radius_m <= 0) pl.config.obstacle_inflation_radius_m = 0.5f;
    if (pl.config.max_path_iterations == 0) pl.config.max_path_iterations = 10000;
    pl.initialized = true;
    log_info("path", "Initialized cell=%.2f corridor=%.1f inflate=%.1f",
             pl.config.grid_cell_size_m, pl.config.safe_corridor_min_m,
             pl.config.obstacle_inflation_radius_m);
    return PATH_OK;
}

path_status_t path_planner_plan(nav_position_t *start,
                                 nav_position_t *goal,
                                 path_msg_t **out_path)
{
    if (!pl.initialized) return PATH_ERR_NOT_INIT;
    if (!start || !goal || !out_path) return PATH_ERR_INVALID_PARAM;
    *out_path = NULL;

    build_grid(start, goal, NULL);

    uint16_t sx = PATH_GRID_SIZE / 2;
    uint16_t sy = PATH_GRID_SIZE / 2;
    double start_lat = start ? start->latitude : 0;
    double start_lon = start ? start->longitude : 0;
    double goal_lat = goal ? goal->latitude : 0;
    double goal_lon = goal ? goal->longitude : 0;

    float dlat_rad = deg2rad_f((float)(goal_lat - start_lat));
    float dlon_rad = deg2rad_f((float)(goal_lon - start_lon));
    float lat_avg_rad = deg2rad_f((float)(start_lat + goal_lat) / 2.0f);
    float dy_m = dlat_rad * 6371000.0f;
    float dx_m = dlon_rad * 6371000.0f * cosf(lat_avg_rad);
    uint16_t gx = (uint16_t)(sx + dx_m / grid_cell());
    uint16_t gy = (uint16_t)(sy + dy_m / grid_cell());
    if (gx >= PATH_GRID_SIZE) gx = sx;
    if (gy >= PATH_GRID_SIZE) gy = sy;

    path_waypoint_t wps[PATH_MAX_WAYPOINTS];
    uint32_t wc = 0;

    if (!astar_search(sx, sy, gx, gy, wps, &wc)) {
        return PATH_ERR_NO_PATH;
    }

    if (pl.config.enable_smoothing) {
        wc = smooth_path(wps, wc);
    }

    path_status_t vret = validate_path(wps, wc);
    if (vret != PATH_OK) return vret;

    memset(&pl.current_path, 0, sizeof(pl.current_path));
    pl.current_path.count = wc;
    for (uint32_t i = 0; i < wc; i++) {
        pl.current_path.waypoints[i] = wps[i];
    }
    pl.current_path.total_length_m = 0;
    for (uint32_t i = 1; i < wc; i++) {
        float dx = wps[i].x - wps[i-1].x;
        float dy = wps[i].y - wps[i-1].y;
        pl.current_path.total_length_m += sqrtf(dx*dx + dy*dy);
    }
    pl.current_path.min_corridor_width_m = pl.config.safe_corridor_min_m;
    pl.current_path.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
    pl.current_path.plan_id = pl.plan_count++;
    *out_path = &pl.current_path;
    log_info("path", "Planned path %lu waypoints len=%.1f id=%lu",
             (unsigned long)wc, (double)pl.current_path.total_length_m,
             (unsigned long)pl.current_path.plan_id);
    return PATH_OK;
}

path_status_t path_planner_replan(path_msg_t *current_path,
                                   const tracked_obj_msg_t *obstacles,
                                   path_msg_t **out_new_path)
{
    if (!pl.initialized) return PATH_ERR_NOT_INIT;
    if (!current_path || !out_new_path) return PATH_ERR_INVALID_PARAM;
    *out_new_path = NULL;

    uint32_t idx = current_path->count / 2;
    if (idx >= current_path->count) idx = 0;

    build_grid(NULL, NULL, obstacles);

    float gx_f = current_path->waypoints[current_path->count - 1].x;
    float gy_f = current_path->waypoints[current_path->count - 1].y;
    uint16_t sx = (uint16_t)(PATH_GRID_SIZE / 2);
    uint16_t sy = (uint16_t)(PATH_GRID_SIZE / 2);
    uint16_t gx = (uint16_t)(PATH_GRID_SIZE / 2 + (int16_t)(gx_f / grid_cell()));
    uint16_t gy = (uint16_t)(PATH_GRID_SIZE / 2 + (int16_t)(gy_f / grid_cell()));
    if (gx >= PATH_GRID_SIZE) gx = PATH_GRID_SIZE - 1;
    if (gy >= PATH_GRID_SIZE) gy = PATH_GRID_SIZE - 1;

    path_waypoint_t wps[PATH_MAX_WAYPOINTS];
    uint32_t wc = 0;
    if (!astar_search(sx, sy, gx, gy, wps, &wc)) {
        return PATH_ERR_NO_PATH;
    }

    if (pl.config.enable_smoothing) {
        wc = smooth_path(wps, wc);
    }

    path_status_t vret = validate_path(wps, wc);
    if (vret != PATH_OK) return vret;

    pl.current_path.count = wc;
    for (uint32_t i = 0; i < wc; i++) {
        pl.current_path.waypoints[i] = wps[i];
    }
    pl.current_path.total_length_m = 0;
    for (uint32_t i = 1; i < wc; i++) {
        float dx = wps[i].x - wps[i-1].x;
        float dy = wps[i].y - wps[i-1].y;
        pl.current_path.total_length_m += sqrtf(dx*dx + dy*dy);
    }
    pl.current_path.min_corridor_width_m = pl.config.safe_corridor_min_m;
    pl.current_path.timestamp_us = xTaskGetTickCount() * portTICK_PERIOD_MS * 1000;
    pl.current_path.plan_id = pl.plan_count++;
    pl.replan_count++;
    *out_new_path = &pl.current_path;
    log_info("path", "Replanned path %lu wpts len=%.1f id=%lu (replan %lu)",
             (unsigned long)wc, (double)pl.current_path.total_length_m,
             (unsigned long)pl.current_path.plan_id,
             (unsigned long)pl.replan_count);
    return PATH_OK;
}

path_status_t path_planner_get_safe_corridor(float *width, float *bearing)
{
    if (!pl.initialized) return PATH_ERR_NOT_INIT;
    if (width) *width = pl.config.safe_corridor_min_m;
    if (bearing) {
        if (pl.current_path.count > 1) {
            float dx = pl.current_path.waypoints[1].x - pl.current_path.waypoints[0].x;
            float dy = pl.current_path.waypoints[1].y - pl.current_path.waypoints[0].y;
            *bearing = atan2f(dy, dx) * 180.0f / 3.14159f;
        } else {
            *bearing = 0;
        }
    }
    return PATH_OK;
}

path_status_t path_planner_set_cost_map(uint8_t *grid)
{
    if (!pl.initialized || !grid) return PATH_ERR_INVALID_PARAM;
    memcpy(pl.grid, grid, sizeof(pl.grid));
    pl.grid_ready = true;
    return PATH_OK;
}

path_status_t path_planner_deinit(void)
{
    pl.initialized = false;
    return PATH_OK;
}
