#include "test_harness.h"
#include "FreeRTOS.h"
#include "semphr.h"

static int mock_log_count = 0;
void log_info(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_warn(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_error(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_debug(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }
void log_critical(const char *m, const char *f, ...) { (void)m; (void)f; mock_log_count++; }

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)s;(void)t; return pdPASS; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdPASS; }

#include "../../src/middleware/src/config_manager.c"

TEST(config_init_ok)
{
    ASSERT_EQ(config_init(), CONFIG_OK);
    config_deinit();
}

TEST(config_set_get_int)
{
    config_init();
    ASSERT_EQ(config_set_int("test.int", 42), CONFIG_OK);
    ASSERT_EQ(config_get_int("test.int", -1), 42);
    config_deinit();
}

TEST(config_set_get_float)
{
    config_init();
    ASSERT_EQ(config_set_float("test.pi", 3.14159f), CONFIG_OK);
    ASSERT_FLOAT_EQ(config_get_float("test.pi", 0), 3.14159f, 0.001f);
    config_deinit();
}

TEST(config_set_get_bool)
{
    config_init();
    ASSERT_EQ(config_set_bool("test.flag", true), CONFIG_OK);
    ASSERT_EQ(config_get_bool("test.flag", false), true);
    config_deinit();
}

TEST(config_set_get_string)
{
    config_init();
    ASSERT_EQ(config_set_string("test.greeting", "hello world"), CONFIG_OK);
    char buf[64];
    ASSERT_EQ(config_get_string("test.greeting", buf, sizeof(buf), ""), CONFIG_OK);
    ASSERT_STREQ(buf, "hello world");
    config_deinit();
}

TEST(config_get_int_default)
{
    config_init();
    ASSERT_EQ(config_get_int("nonexistent", 99), 99);
    config_deinit();
}

TEST(config_get_string_default)
{
    config_init();
    char buf[64];
    ASSERT_EQ(config_get_string("nope", buf, sizeof(buf), "fallback"), CONFIG_OK);
    ASSERT_STREQ(buf, "fallback");
    config_deinit();
}

TEST(config_overwrite)
{
    config_init();
    ASSERT_EQ(config_set_int("key", 1), CONFIG_OK);
    ASSERT_EQ(config_set_int("key", 2), CONFIG_OK);
    ASSERT_EQ(config_get_int("key", 0), 2);
    config_deinit();
}

TEST(config_save_and_load)
{
    config_init();
    ASSERT_EQ(config_save(), CONFIG_OK);
    ASSERT_EQ(config_load_defaults(), CONFIG_OK);
    config_deinit();
}

static int cb_called = 0;
static void test_callback(const char *key, void *ctx)
{
    (void)key; (void)ctx; cb_called++;
}

TEST(config_callback)
{
    config_init();
    cb_called = 0;
    ASSERT_EQ(config_register_callback("test", test_callback, NULL), CONFIG_OK);
    ASSERT_EQ(config_set_int("test.val", 42), CONFIG_OK);
    config_deinit();
}

int main(void)
{
    return run_all_tests();
}
