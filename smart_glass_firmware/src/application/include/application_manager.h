#ifndef APPLICATION_MANAGER_H
#define APPLICATION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    kUseCaseBlindAssist,
    kUseCaseAlzheimerAssist,
    kUseCaseDualMode
} use_case_t;

typedef struct {
    use_case_t active_use_case;
    bool navigation_active;
    bool obstacle_avoidance_enabled;
    bool scene_description_enabled;
    bool reminders_enabled;
    bool face_recognition_enabled;
    bool disorientation_alerts_enabled;
    uint8_t speech_verbosity;
    uint32_t orientation_announce_interval_s;
    uint32_t location_announce_interval_s;
} user_prefs_t;

typedef enum {
    APP_OK = 0,
    APP_ERR_NOT_INIT,
    APP_ERR_INVALID_MODE,
    APP_ERR_UNKNOWN_CMD
} app_status_t;

app_status_t app_manager_init(void);
app_status_t app_manager_start(void);
app_status_t app_manager_stop(void);
app_status_t app_manager_process_user_cmd(const char *cmd);
app_status_t app_manager_get_user_prefs(user_prefs_t *out);
app_status_t app_manager_set_user_prefs(const user_prefs_t *prefs);
app_status_t app_manager_trigger_orientation(void);
app_status_t app_manager_trigger_location_report(void);
app_status_t app_manager_deinit(void);

#endif /* APPLICATION_MANAGER_H */
