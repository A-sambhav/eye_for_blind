#include <string.h>
#include "application_manager.h"
#include "message_bus.h"
#include "message_types.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static struct {
    user_prefs_t prefs;
    SemaphoreHandle_t lock;
    bool running;
    bool initialized;
} app;

static void publish_speech(const char *text, uint8_t priority)
{
    speech_req_t req;
    memset(&req, 0, sizeof(req));
    req.severity = priority;
    strncpy(req.speech_text, text, SCENE_DESC_MAX - 1);
    req.duration_ms = strlen(text) * 60;
    message_bus_publish(MSG_SPEECH_REQ, &req, sizeof(req), priority);
}

app_status_t app_manager_init(void)
{
    memset(&app, 0, sizeof(app));
    app.lock = xSemaphoreCreateMutex();
    if (app.lock == NULL) return APP_ERR_NOT_INIT;

    app.prefs.active_use_case = kUseCaseDualMode;
    app.prefs.navigation_active = true;
    app.prefs.obstacle_avoidance_enabled = true;
    app.prefs.scene_description_enabled = true;
    app.prefs.reminders_enabled = true;
    app.prefs.face_recognition_enabled = true;
    app.prefs.disorientation_alerts_enabled = true;
    app.prefs.speech_verbosity = 2;
    app.prefs.orientation_announce_interval_s = 60;
    app.prefs.location_announce_interval_s = 120;

    app.initialized = true;
    log_info("app", "Initialized use_case=%d", app.prefs.active_use_case);
    return APP_OK;
}

app_status_t app_manager_start(void)
{
    if (!app.initialized) return APP_ERR_NOT_INIT;
    app.running = true;
    log_info("app", "Started");
    publish_speech("System ready", 1);
    return APP_OK;
}

app_status_t app_manager_stop(void)
{
    if (!app.initialized) return APP_ERR_NOT_INIT;
    app.running = false;
    log_info("app", "Stopped");
    return APP_OK;
}

app_status_t app_manager_process_user_cmd(const char *cmd)
{
    if (!app.initialized || cmd == NULL) return APP_ERR_NOT_INIT;

    xSemaphoreTake(app.lock, portMAX_DELAY);

    if (strcmp(cmd, "start_blind_mode") == 0) {
        app.prefs.active_use_case = kUseCaseBlindAssist;
        app.prefs.navigation_active = true;
        app.prefs.obstacle_avoidance_enabled = true;
        app.prefs.scene_description_enabled = true;
        app.prefs.face_recognition_enabled = false;
        app.prefs.reminders_enabled = false;
        publish_speech("Switched to blind assist mode", 2);
        log_info("app", "Switched to Blind Assist mode");
    } else if (strcmp(cmd, "start_alzheimer_mode") == 0) {
        app.prefs.active_use_case = kUseCaseAlzheimerAssist;
        app.prefs.navigation_active = true;
        app.prefs.obstacle_avoidance_enabled = false;
        app.prefs.scene_description_enabled = true;
        app.prefs.face_recognition_enabled = true;
        app.prefs.reminders_enabled = true;
        app.prefs.disorientation_alerts_enabled = true;
        publish_speech("Switched to Alzheimer assistance mode", 2);
        log_info("app", "Switched to Alzheimer Assist mode");
    } else if (strcmp(cmd, "switch_mode") == 0 || strcmp(cmd, "dual_mode") == 0) {
        app.prefs.active_use_case = kUseCaseDualMode;
        app.prefs.navigation_active = true;
        app.prefs.obstacle_avoidance_enabled = true;
        app.prefs.scene_description_enabled = true;
        app.prefs.face_recognition_enabled = true;
        app.prefs.reminders_enabled = true;
        app.prefs.disorientation_alerts_enabled = true;
        publish_speech("Switched to dual mode", 2);
        log_info("app", "Switched to Dual mode");
    } else if (strcmp(cmd, "describe_scene") == 0) {
        publish_speech("Describing scene", 1);
    } else if (strcmp(cmd, "where_am_i") == 0 || strcmp(cmd, "orientation") == 0) {
        publish_speech("Checking location and orientation", 2);
    } else {
        log_warn("app", "Unknown command: %s", cmd);
    }

    xSemaphoreGive(app.lock);
    return APP_OK;
}

app_status_t app_manager_get_user_prefs(user_prefs_t *out)
{
    if (!app.initialized || out == NULL) return APP_ERR_NOT_INIT;
    xSemaphoreTake(app.lock, portMAX_DELAY);
    *out = app.prefs;
    xSemaphoreGive(app.lock);
    return APP_OK;
}

app_status_t app_manager_set_user_prefs(const user_prefs_t *prefs)
{
    if (!app.initialized || prefs == NULL) return APP_ERR_NOT_INIT;
    xSemaphoreTake(app.lock, portMAX_DELAY);
    app.prefs = *prefs;
    xSemaphoreGive(app.lock);
    return APP_OK;
}

app_status_t app_manager_trigger_orientation(void)
{
    if (!app.initialized) return APP_ERR_NOT_INIT;
    publish_speech("Announcing current orientation", 1);
    return APP_OK;
}

app_status_t app_manager_trigger_location_report(void)
{
    if (!app.initialized) return APP_ERR_NOT_INIT;
    publish_speech("Announcing current location", 1);
    return APP_OK;
}

app_status_t app_manager_deinit(void)
{
    app.initialized = false;
    return APP_OK;
}
