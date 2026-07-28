#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define SYS_MAX_MODULES 32
#define SYS_VERSION_MAJOR 1
#define SYS_VERSION_MINOR 0
#define SYS_VERSION_PATCH 0

typedef enum {
    SYS_OK = 0,
    SYS_ERR_MODULE_FAIL,
    SYS_ERR_MODE_INVALID,
    SYS_ERR_OTA_FAIL
} sys_status_t;

typedef enum {
    kModeBlindAssist,
    kModeAlzheimerAssist,
    kModeDualMode,
    kModeFactoryTest
} sys_mode_t;

typedef enum {
    kCmdShutdown,
    kCmdReboot,
    kCmdFactoryReset,
    kCmdEnterSleep,
    kCmdEnterDeepSleep,
    kCmdWakeUp,
    kCmdModeChange,
    kCmdOtaStart,
    kCmdPauseAll,
    kCmdResumeAll
} sys_cmd_t;

typedef struct {
    sys_cmd_t cmd;
    uint32_t param;
    uint32_t timestamp_us;
} sys_cmd_msg_t;

typedef sys_status_t (*module_init_cb_t)(void);
typedef sys_status_t (*module_deinit_cb_t)(void);

typedef struct {
    char name[16];
    module_init_cb_t init_cb;
    module_deinit_cb_t deinit_cb;
    bool initialized;
    uint32_t init_order;
} sys_module_entry_t;

typedef struct {
    uint32_t uptime_seconds;
    sys_mode_t mode;
    char version[16];
    uint32_t boot_count;
    uint32_t shutdown_count;
    uint32_t ota_count;
    float cpu_temp;
    uint32_t free_heap;
    uint32_t min_free_heap;
} sys_info_t;

sys_status_t sys_manager_init(void);
sys_status_t sys_manager_start(void);
sys_status_t sys_manager_shutdown(void);
sys_status_t sys_manager_reboot(void);
sys_status_t sys_manager_factory_reset(void);
sys_status_t sys_manager_set_mode(sys_mode_t mode);
sys_mode_t sys_manager_get_mode(void);
sys_status_t sys_manager_get_info(sys_info_t *out);
sys_status_t sys_manager_get_uptime(uint32_t *out_seconds);
sys_status_t sys_manager_register_module(const char *name,
                                          module_init_cb_t init_cb,
                                          module_deinit_cb_t deinit_cb,
                                          uint32_t init_order);

#endif /* SYSTEM_MANAGER_H */
