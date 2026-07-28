#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* --------------------------------------------------------------------------
 * Message type enum — one entry per route in LLD-001.md section 1.7
 * Inter-Module Communication Matrix. Add new types at the end to keep
 * numeric IDs stable across firmware revisions.
 * ------------------------------------------------------------------------ */
typedef enum {
    MSG_RAW_FRAME = 0,           /* raw_frame_t       — Camera -> Depth, OD  */
    MSG_DEPTH_MAP,               /* depth_map_t       — Depth -> OD          */
    MSG_DETECTION_LIST,          /* detection_list_t  — OD -> Scene           */
    MSG_SCENE_DESC,              /* scene_desc_t      — Scene -> Context      */
    MSG_HAZARD_LIST,             /* hazard_list_t     — Scene -> Hazard       */
    MSG_TRACKED_OBJECTS,         /* tracked_obj_t     — Tracker -> Nav        */
    MSG_GPS_POSITION,            /* gps_position_t    — GPS -> Nav            */
    MSG_IMU_DATA,                /* imu_data_t        — IMU -> Nav, Emerg     */
    MSG_NAV_PLAN,                /* nav_plan_t        — Nav -> PathPlanner    */
    MSG_PATH,                    /* path_t            — PathPlanner -> ObsAvoid */
    MSG_HAZARD_EVENT,            /* hazard_event_t    — Hazard -> DecEng      */
    MSG_CONTEXT,                 /* context_t         — Context -> DecEng     */
    MSG_VOICE_COMMAND,           /* voice_cmd_t       — VoiceRec -> DecEng    */
    MSG_EMERGENCY,               /* emergency_t       — Emerg -> DecEng       */
    MSG_REMINDER,                /* reminder_t        — Reminder -> DecEng    */
    MSG_MEMORY_HINT,             /* memory_hint_t     — MemAsst -> DecEng     */
    MSG_SPEECH_REQ,              /* speech_req_t      — DecEng -> SpeechSynth */
    MSG_NAV_OVERRIDE,            /* nav_override_t    — DecEng -> Nav         */
    MSG_NAV_SPEECH,              /* nav_speech_t      — Nav -> SpeechSynth    */
    MSG_EMERGENCY_ALERT,         /* emergency_alert_t — Emerg -> SpeechSynth  */
    MSG_BATTERY_STATUS,          /* battery_status_t  — BMS -> Power          */
    MSG_POWER_EVENT,             /* power_event_t     — Power -> Sys          */
    MSG_SYSTEM_CMD,              /* system_cmd_t      — Sys -> All            */
    MSG_DIAG_RESULT,             /* diag_result_t     — Diag -> Log           */
    MSG_LOG_HEALTH,              /* log_health_t      — Log -> Wdog           */
    MSG_CAMERA_STATUS,           /* camera_status_t   — Camera -> Diag        */

    MSG_TYPE_COUNT               /* keep last */
} msg_type_t;

/* --- Payload structs ---------------------------------------------------- */

#define MAX_DETECTIONS 32
#define DEPTH_FULL_W 640
#define DEPTH_FULL_H 480
#define DEPTH_SMALL_W 160
#define DEPTH_SMALL_H 120
#define SCENE_DESC_MAX 256

typedef struct {
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    float quat_w, quat_x, quat_y, quat_z;
    float pitch, roll, yaw;
    float temperature;
    uint32_t timestamp_us;
    uint32_t sample_count;
    bool step_detected;
    bool fall_detected;
    uint8_t gesture;
} imu_data_t;

typedef struct {
    double latitude;
    double longitude;
    double altitude;
    float speed_mps;
    float heading_deg;
    float hdop;
    float vdop;
    uint8_t num_satellites;
    uint8_t fix_type;
    uint32_t timestamp_us;
    bool valid;
} gps_position_t;

typedef struct {
    uint16_t x, y, width, height;
    float confidence;
    uint8_t class_id;
    float pos_x, pos_y, pos_z;
    bool has_valid_depth;
} bounding_box_t;

typedef struct {
    bounding_box_t boxes[MAX_DETECTIONS];
    uint32_t count;
    uint32_t timestamp_us;
    uint32_t frame_id;
    float avg_confidence;
} detection_list_t;

typedef struct {
    float map[DEPTH_SMALL_H][DEPTH_SMALL_W];
    uint8_t confidence[DEPTH_SMALL_H][DEPTH_SMALL_W];
    uint8_t min_dist_grid[DEPTH_FULL_H / 32][DEPTH_FULL_W / 32];
    uint32_t timestamp_us;
    uint32_t frame_id;
} depth_map_t;

typedef struct {
    uint8_t level_pct;
    bool charging;
    bool low_warning;
    bool critical;
    uint32_t voltage_mv;
    int32_t current_ma;
    float temp_c;
} battery_status_t;

typedef struct {
    uint8_t severity;
    char speech_text[SCENE_DESC_MAX];
    float nav_bearing;
    float nav_distance;
    uint8_t nav_reason;
    uint32_t duration_ms;
} speech_req_t;

typedef struct {
    uint16_t error_code;
    char module[16];
    char message[64];
} system_error_t;

/* Generic bus envelope — max payload sized for largest struct above */
#define MSG_MAX_PAYLOAD_BYTES 256

typedef struct {
    msg_type_t type;
    uint32_t timestamp_us;
    uint8_t payload[MSG_MAX_PAYLOAD_BYTES];
    size_t payload_size;
    uint8_t priority;
} bus_message_t;

#endif /* MESSAGE_TYPES_H */
