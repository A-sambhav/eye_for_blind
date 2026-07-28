#include "test_harness.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "message_bus.h"
#include "task_manager.h"

static int mock_log_count = 0;
void log_info(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_warn(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_error(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_debug(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }
void log_critical(const char *m, const char *f, ...) { (void)m;(void)f;mock_log_count++; }

TickType_t xTaskGetTickCount(void) { return 100; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)s;(void)t; return pdPASS; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdPASS; }
void task_manager_feed_watchdog(task_id_t id) { (void)id; }

msg_bus_status_t message_bus_publish(msg_type_t t, const void *p, uint16_t s, uint8_t pri)
{ (void)t;(void)p;(void)s;(void)pri; return MSG_BUS_OK; }
msg_bus_status_t message_bus_subscribe(msg_type_t t, bus_subscriber_fn c, void *u)
{ (void)t;(void)c;(void)u; return MSG_BUS_OK; }
msg_bus_status_t message_bus_unsubscribe(msg_type_t t, bus_subscriber_fn c)
{ (void)t;(void)c; return MSG_BUS_OK; }

void task_manager_start_all(void) {}
void vTaskSuspendAll(void) {}
void taskDISABLE_INTERRUPTS(void) {}
size_t xPortGetFreeHeapSize(void) { return 65536; }
size_t xPortGetMinimumEverFreeHeapSize(void) { return 32768; }
void vTaskDelayUntil(TickType_t *pxPreviousWakeTime, TickType_t xTimeIncrement)
{ (void)pxPreviousWakeTime; (void)xTimeIncrement; }

#include "../../src/middleware/src/config_manager.c"
#include "../../src/middleware/src/system_manager.c"

static sys_status_t dummy_init(void) { return SYS_OK; }
static sys_status_t dummy_deinit(void) { return SYS_OK; }

TEST(sys_manager_init)
{
    ASSERT_EQ(sys_manager_init(), SYS_OK);
}

TEST(sys_manager_register_module)
{
    sys_manager_init();
    ASSERT_EQ(sys_manager_register_module("test_module", dummy_init, dummy_deinit, 1), SYS_OK);
}

TEST(sys_manager_get_info)
{
    sys_manager_init();
    sys_manager_register_module("test_module", dummy_init, dummy_deinit, 1);

    sys_info_t info;
    ASSERT_EQ(sys_manager_get_info(&info), SYS_OK);
    ASSERT_TRUE(info.uptime_seconds >= 0 || info.uptime_seconds < 1000000);

    sys_manager_shutdown();
}

TEST(sys_manager_mode)
{
    sys_manager_init();

    ASSERT_EQ(sys_manager_set_mode(kModeBlindAssist), SYS_OK);
    ASSERT_EQ(sys_manager_get_mode(), kModeBlindAssist);

    ASSERT_EQ(sys_manager_set_mode(kModeAlzheimerAssist), SYS_OK);
    ASSERT_EQ(sys_manager_get_mode(), kModeAlzheimerAssist);

    sys_manager_shutdown();
}

TEST(sys_manager_uptime)
{
    sys_manager_init();

    uint32_t uptime = 0;
    ASSERT_EQ(sys_manager_get_uptime(&uptime), SYS_OK);

    sys_manager_shutdown();
}

int main(void)
{
    return run_all_tests();
}
