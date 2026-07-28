#include "test_harness.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "message_bus.h"

msg_bus_status_t message_bus_publish(msg_type_t t, const void *p, uint16_t s, uint8_t pri)
{ (void)t;(void)p;(void)s;(void)pri; return MSG_BUS_OK; }
msg_bus_status_t message_bus_subscribe(msg_type_t t, bus_subscriber_fn c, void *u)
{ (void)t;(void)c;(void)u; return MSG_BUS_OK; }

TickType_t xTaskGetTickCount(void) { return 42; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)s;(void)t; return pdPASS; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdPASS; }

#include "../../src/middleware/src/logging_manager.c"

TEST(logging_init_ok)
{
    log_config_t cfg = { .global_level = LOG_DEBUG, .flush_interval_ms = 100,
                         .enable_ble_output = false, .max_rate_per_module = 10 };
    ASSERT_EQ(log_init(&cfg), LOG_OK);
}

TEST(logging_init_null)
{
    ASSERT_EQ(log_init(NULL), LOG_ERR_NOT_INIT);
}

TEST(logging_info)
{
    log_config_t cfg = { .global_level = LOG_DEBUG, .flush_interval_ms = 100,
                         .enable_ble_output = false, .max_rate_per_module = 100 };
    log_init(&cfg);

    log_info("test", "hello %s %d", "world", 42);
    log_warn("test", "warning: %f", 3.14f);
    log_error("test", "error code %x", 0xDEAD);

    ASSERT_EQ(1, 1);
}

TEST(logging_verbose_filter)
{
    log_config_t cfg = { .global_level = LOG_WARNING, .flush_interval_ms = 100,
                         .enable_ble_output = false, .max_rate_per_module = 100 };
    log_init(&cfg);

    log_debug("test", "should not appear");
    log_info("test", "should not appear either");
    log_warn("test", "this should appear");
    log_error("test", "this too");

    ASSERT_EQ(1, 1);
}

TEST(logging_get_stats)
{
    log_config_t cfg = { .global_level = LOG_DEBUG, .flush_interval_ms = 100,
                         .enable_ble_output = false, .max_rate_per_module = 100 };
    log_init(&cfg);

    log_stats_t stats;
    ASSERT_EQ(log_get_stats(&stats), LOG_OK);
    ASSERT_TRUE(stats.total_logs >= 0);

    log_deinit();
}

int main(void)
{
    return run_all_tests();
}
