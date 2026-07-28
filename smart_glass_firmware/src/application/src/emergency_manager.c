#include <string.h>
#include <math.h>
#include "emergency_manager.h"
#include "message_bus.h"
#include "message_types.h"
#include "logging_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define G 9.81f
#define PI_F 3.14159265f

typedef enum {
    FSM_MONITORING,
    FSM_FREEFALL,
    FSM_IMPACT,
    FSM_VERIFY_IMMOBILE,
    FSM_CONFIRMED,
    FSM_FALSE_ALARM
} fall_fsm_t;

static struct {
    emergency_config_t config;
    emergency_msg_t current;
    SemaphoreHandle_t lock;
    fall_fsm_t fall_state;
    uint32_t freefall_start;
    uint32_t impact_timestamp;
    float impact_magnitude;
    float orientation_change;
    uint32_t fall_count;
    uint32_t wandering_count;
    uint32_t sos_count;
    uint32_t last_wandering_check;
    bool emergency_active;
    bool initialized;
    float last_accel_mag;
} em;

#define DEG_TO_RAD_F (3.14159265f / 180.0f)

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static float accel_magnitude(const float accel[3])
{
    return sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]) / G;
}

static float orientation_change_deg(const float gyro[3], float dt_s)
{
    return sqrtf(gyro[0]*gyro[0] + gyro[1]*gyro[1] + gyro[2]*gyro[2]) * dt_s * 180.0f / PI_F;
}

static void publish_emergency(emergency_type_t type, const char *text)
{
    memset(&em.current, 0, sizeof(em.current));
    em.current.type = type;
    em.current.latitude = 0;
    em.current.longitude = 0;
    em.current.timestamp_us = now_ms() * 1000;
    em.current.severity = (type == kEmergencyNone) ? 0 : 10;
    em.current.acknowledged = false;
    strncpy(em.current.text, text, EMERGENCY_TEXT_LEN - 1);

    speech_req_t alert;
    memset(&alert, 0, sizeof(alert));
    alert.severity = 10;
    strncpy(alert.speech_text, text, SCENE_DESC_MAX - 1);

    message_bus_publish(MSG_EMERGENCY, &em.current, sizeof(em.current), 4);
    message_bus_publish(MSG_EMERGENCY_ALERT, &alert, sizeof(speech_req_t), 4);
    log_critical("emerg", "%s", text);
}

emergency_status_t emergency_init(const emergency_config_t *config)
{
    if (config == NULL) return EMERG_ERR_NOT_INIT;
    memset(&em, 0, sizeof(em));
    em.config = *config;
    if (em.config.impact_threshold_g <= 0) em.config.impact_threshold_g = 2.5f;
    if (em.config.freefall_threshold_g <= 0) em.config.freefall_threshold_g = 0.5f;
    if (em.config.orientation_change_threshold_deg <= 0) em.config.orientation_change_threshold_deg = 45.0f;
    if (em.config.impact_duration_ms == 0) em.config.impact_duration_ms = 200;
    if (em.config.post_fall_immobility_s == 0) em.config.post_fall_immobility_s = 10;
    if (em.config.false_positive_window_s == 0) em.config.false_positive_window_s = 5;

    em.lock = xSemaphoreCreateMutex();
    if (em.lock == NULL) return EMERG_ERR_NOT_INIT;
    em.fall_state = FSM_MONITORING;

    em.initialized = true;
    log_info("emerg", "Initialized impact=%.1fG freefall=%.1fG orient=%.0f deg immob=%u s",
             em.config.impact_threshold_g, em.config.freefall_threshold_g,
             em.config.orientation_change_threshold_deg,
             em.config.post_fall_immobility_s);
    return EMERG_OK;
}

emergency_status_t emergency_check_fall(const float accel[3],
                                         const float gyro[3],
                                         fall_event_t *out_fall)
{
    if (!em.initialized) return EMERG_ERR_NOT_INIT;
    if (!accel || !gyro) return EMERG_ERR_NOT_INIT;

    xSemaphoreTake(em.lock, portMAX_DELAY);

    float mag = accel_magnitude(accel);
    em.last_accel_mag = mag;
    uint32_t now = now_ms();

    switch (em.fall_state) {
    case FSM_MONITORING:
        if (mag < em.config.freefall_threshold_g) {
            em.fall_state = FSM_FREEFALL;
            em.freefall_start = now;
            log_debug("emerg", "Freefall detected (mag=%.2fG)", mag);
        }
        break;

    case FSM_FREEFALL:
        if (mag > em.config.impact_threshold_g) {
            float freefall_dur = (now - em.freefall_start) * 1e-3f;
            em.impact_timestamp = now;
            em.impact_magnitude = mag;
            em.orientation_change = orientation_change_deg(gyro, 0.01f);

            em.fall_state = FSM_VERIFY_IMMOBILE;
            log_debug("emerg", "Impact mag=%.2fG orient=%.1f deg freefall=%.1f ms",
                      mag, em.orientation_change, freefall_dur * 1000);
        } else if (mag >= em.config.freefall_threshold_g) {
            em.fall_state = FSM_MONITORING;
        }
        break;

    case FSM_IMPACT:
        em.fall_state = FSM_MONITORING;
        break;

    case FSM_VERIFY_IMMOBILE: {
        uint32_t elapsed = (now - em.impact_timestamp) / 1000;
        if (elapsed >= em.config.post_fall_immobility_s) {
            float variance = fabsf(mag - 1.0f);
            if (variance < 0.3f && em.orientation_change > em.config.orientation_change_threshold_deg) {
                em.fall_state = FSM_CONFIRMED;
                em.fall_count++;

                if (out_fall) {
                    memset(out_fall, 0, sizeof(*out_fall));
                    out_fall->timestamp_us = now * 1000;
                    out_fall->impact_magnitude_g = em.impact_magnitude;
                    out_fall->orientation_change_deg = em.orientation_change;
                    out_fall->freefall_duration_ms = (em.impact_timestamp - em.freefall_start);
                    out_fall->confirmed = true;
                }

                publish_emergency(kEmergencyFall, "Fall detected! I need help.");
                log_critical("emerg", "Fall CONFIRMED (impact=%.1fG orient=%.1f)",
                             em.impact_magnitude, em.orientation_change);
            } else {
                em.fall_state = FSM_FALSE_ALARM;
                log_debug("emerg", "Fall false alarm (variance=%.2f)", variance);
            }
        } else if (mag > em.config.impact_threshold_g * 0.5f) {
            em.impact_magnitude = fmaxf(em.impact_magnitude, mag);
            em.orientation_change += orientation_change_deg(gyro, 0.01f);
        }
        break;
    }

    case FSM_CONFIRMED:
        if (now - em.impact_timestamp > em.config.false_positive_window_s * 1000) {
            em.fall_state = FSM_MONITORING;
        }
        break;

    case FSM_FALSE_ALARM:
        em.fall_state = FSM_MONITORING;
        break;
    }

    xSemaphoreGive(em.lock);
    return EMERG_OK;
}

emergency_status_t emergency_check_wandering(double lat, double lon,
                                              bool *out_wandering)
{
    if (!em.initialized || out_wandering == NULL) return EMERG_ERR_NOT_INIT;
    *out_wandering = false;

    uint32_t now = now_ms();
    if (now - em.last_wandering_check < 1000) return EMERG_OK;
    em.last_wandering_check = now;

    xSemaphoreTake(em.lock, portMAX_DELAY);
    if (em.config.safe_zone.active) {
        double dlat = lat - em.config.safe_zone.center_lat;
        double dlon = lon - em.config.safe_zone.center_lon;
        float dlat_rad = (float)dlat * DEG_TO_RAD_F;
        float dlon_rad = (float)dlon * DEG_TO_RAD_F;
        float lat_avg = (float)(lat + em.config.safe_zone.center_lat) * 0.5f * DEG_TO_RAD_F;
        float dx = dlon_rad * 6371000.0f * cosf(lat_avg);
        float dy = dlat_rad * 6371000.0f;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist > em.config.safe_zone.radius_m) {
            *out_wandering = true;
            em.wandering_count++;
            publish_emergency(kEmergencyWandering, "You have left your safe zone.");
            log_warn("emerg", "Wandering detected %.0f m outside zone", dist - em.config.safe_zone.radius_m);
        }
    }
    xSemaphoreGive(em.lock);
    return EMERG_OK;
}

emergency_status_t emergency_sos_triggered(void)
{
    if (!em.initialized) return EMERG_ERR_NOT_INIT;
    em.sos_count++;
    publish_emergency(kEmergencySOS, "SOS triggered! Please help me.");
    return EMERG_OK;
}

emergency_status_t emergency_get_status(emergency_msg_t *out)
{
    if (!em.initialized || out == NULL) return EMERG_ERR_NOT_INIT;
    xSemaphoreTake(em.lock, portMAX_DELAY);
    *out = em.current;
    xSemaphoreGive(em.lock);
    return EMERG_OK;
}

emergency_status_t emergency_clear(emergency_type_t type)
{
    if (!em.initialized) return EMERG_ERR_NOT_INIT;
    xSemaphoreTake(em.lock, portMAX_DELAY);
    if (em.current.type == type || type == kEmergencyNone) {
        em.current.acknowledged = true;
        em.current.severity = 0;
        em.fall_state = FSM_MONITORING;
        em.emergency_active = false;
        publish_emergency(kEmergencyNone, "Emergency cleared.");
        log_info("emerg", "Emergency %d cleared", type);
    }
    xSemaphoreGive(em.lock);
    return EMERG_OK;
}

emergency_status_t emergency_deinit(void)
{
    em.initialized = false;
    return EMERG_OK;
}
