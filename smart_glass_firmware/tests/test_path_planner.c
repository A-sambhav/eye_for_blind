#include "test_harness.h"

static int mock_log_count = 0;
void log_info(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_warn(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_error(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_debug(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_msg(int l, const char *m, const char *f, ...) { (void)l;(void)m;(void)f;mock_log_count++; }

typedef uint32_t TickType_t;
TickType_t xTaskGetTickCount(void) { static TickType_t t=0; return t++; }

#include "../../src/navigation/src/path_planner.c"

TEST(path_planner_init_ok)
{
    path_config_t cfg = { .grid_cell_size_m = 0.1f,
                          .safe_corridor_min_m = 0.5f,
                          .obstacle_inflation_radius_m = 0.2f,
                          .max_path_iterations = 5000,
                          .enable_smoothing = false };
    ASSERT_EQ(path_planner_init(&cfg), PATH_OK);
    path_planner_deinit();
}

TEST(path_planner_init_null)
{
    ASSERT_EQ(path_planner_init(NULL), PATH_ERR_INVALID_PARAM);
}

TEST(path_planner_plan_straight_line)
{
    path_config_t cfg = { .grid_cell_size_m = 0.1f,
                          .safe_corridor_min_m = 0.5f,
                          .obstacle_inflation_radius_m = 0.2f,
                          .max_path_iterations = 5000,
                          .enable_smoothing = false };
    path_planner_init(&cfg);

    nav_position_t start = { .x = 0, .y = 0, .z = 0, .heading_deg = 0 };
    nav_position_t goal  = { .x = 0.5f, .y = 0, .z = 0, .heading_deg = 0 };

    path_msg_t *out = NULL;
    ASSERT_EQ(path_planner_plan(&start, &goal, &out), PATH_OK);
    ASSERT_NOT_NULL(out);
    ASSERT_TRUE(out->count > 0);
    ASSERT_TRUE(out->total_length_m > 0);

    path_planner_deinit();
}

TEST(path_planner_plan_diagonal)
{
    path_config_t cfg = { .grid_cell_size_m = 0.1f,
                          .safe_corridor_min_m = 0.5f,
                          .obstacle_inflation_radius_m = 0.2f,
                          .max_path_iterations = 10000,
                          .enable_smoothing = false };
    path_planner_init(&cfg);

    nav_position_t start = { .x = 0, .y = 0 };
    nav_position_t goal  = { .x = 0.3f, .y = 0.4f };

    path_msg_t *out = NULL;
    ASSERT_EQ(path_planner_plan(&start, &goal, &out), PATH_OK);
    ASSERT_NOT_NULL(out);
    ASSERT_TRUE(out->count > 1);

    path_planner_deinit();
}

TEST(path_planner_plan_same_start_goal)
{
    path_config_t cfg = { .grid_cell_size_m = 0.1f,
                          .safe_corridor_min_m = 0.5f,
                          .obstacle_inflation_radius_m = 0.2f,
                          .max_path_iterations = 5000,
                          .enable_smoothing = false };
    path_planner_init(&cfg);

    nav_position_t start = { .x = 0, .y = 0 };
    nav_position_t goal  = { .x = 0, .y = 0 };

    path_msg_t *out = NULL;
    ASSERT_EQ(path_planner_plan(&start, &goal, &out), PATH_OK);
    ASSERT_NOT_NULL(out);
    ASSERT_EQ(out->count, 1);

    path_planner_deinit();
}

TEST(path_planner_plan_no_path)
{
    path_config_t cfg = { .grid_cell_size_m = 0.1f,
                          .safe_corridor_min_m = 0.5f,
                          .obstacle_inflation_radius_m = 0.2f,
                          .max_path_iterations = 5000,
                          .enable_smoothing = false };
    path_planner_init(&cfg);

    nav_position_t start = { .x = 0, .y = 0 };

    for (int x = 0; x < PATH_GRID_SIZE; x++)
        for (int y = 0; y < PATH_GRID_SIZE; y++)
            pl.grid[y][x] = CELL_OBSTACLE;

    nav_position_t goal  = { .x = 0.3f, .y = 0.4f };
    path_msg_t *out = NULL;
    ASSERT_EQ(path_planner_plan(&start, &goal, &out), PATH_ERR_NO_PATH);

    path_planner_deinit();
}

TEST(path_planner_set_cost_map)
{
    path_config_t cfg = { .grid_cell_size_m = 0.1f,
                          .safe_corridor_min_m = 0.5f,
                          .obstacle_inflation_radius_m = 0.2f,
                          .max_path_iterations = 5000,
                          .enable_smoothing = false };
    path_planner_init(&cfg);

    uint8_t cost_map[PATH_GRID_SIZE * PATH_GRID_SIZE];
    memset(cost_map, 0, sizeof(cost_map));
    cost_map[5 * PATH_GRID_SIZE + 5] = 255;

    ASSERT_EQ(path_planner_set_cost_map(cost_map), PATH_OK);

    path_planner_deinit();
}

int main(void)
{
    return run_all_tests();
}
