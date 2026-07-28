#include "message_bus.h"
#include "config_manager.h"
#include "logging_manager.h"
#include "watchdog_manager.h"
#include "diagnostics_manager.h"
#include "system_manager.h"
#include "system_health.h"
#include "task_manager.h"

#include "camera_hal.h"
#include "imu_hal.h"
#include "audio_hal.h"
#include "battery_hal.h"
#include "gps_hal.h"

#include "database_manager.h"

#include "ei_runtime.h"
#include "depth_processing.h"
#include "object_detection.h"
#include "object_tracking.h"
#include "scene_understanding.h"

#include "navigation_engine.h"
#include "path_planner.h"
#include "obstacle_avoidance.h"

#include "voice_recognition.h"
#include "speech_synthesis.h"

#include "application_manager.h"
#include "reminder_manager.h"
#include "memory_assistant.h"
#include "emergency_manager.h"

#include "FreeRTOS.h"
#include "task.h"
#include "portable.h"

extern uint32_t _ebss;

static void init_heap(void)
{
    static HeapRegion_t xHeapRegions[2];
    uint32_t heap_start = (uint32_t)&_ebss;
    uint32_t heap_size = 0x24080000 - heap_start;
    xHeapRegions[0].pucStartAddress = (uint8_t *)heap_start;
    xHeapRegions[0].xSizeInBytes = heap_size;
    xHeapRegions[1].pucStartAddress = NULL;
    xHeapRegions[1].xSizeInBytes = 0;
    vPortDefineHeapRegions(xHeapRegions);
}

static void init_drivers(void)
{
}

static void init_middleware(void)
{
    config_init();

    log_config_t log_cfg = {
        .global_level = LOG_INFO,
        .flush_interval_ms = 100,
        .enable_ble_output = false,
        .max_rate_per_module = 50,
    };
    if (log_init(&log_cfg) != LOG_OK) {
        system_health_report_boot_failure("logging");
    }

    if (message_bus_init() != MSG_BUS_OK) {
        system_health_report_boot_failure("message_bus");
        for (;;) { }
    }

    system_health_init();

    wdog_config_t wdog_cfg = {
        .hardware_timeout_ms = 5000,
        .monitor_interval_ms = 100,
        .default_task_timeout_ms = 500,
        .enable_hardware_wdog = true,
        .auto_recover_tasks = false,
    };
    if (wdog_init(&wdog_cfg) != WDOG_OK) {
        system_health_report_boot_failure("watchdog");
    }

    diag_config_t diag_cfg = {
        .check_interval_ms = 5000,
        .response_timeout_ms = 1000,
        .self_test_interval_ms = 60000,
        .enable_perf_collection = true,
    };
    if (diag_init(&diag_cfg) != DIAG_OK) {
        system_health_report_boot_failure("diag");
    }

    if (sys_manager_init() != SYS_OK) {
        system_health_report_boot_failure("system_mgr");
    }
}

static void init_hal(void)
{
    imu_hal_status_t imu_status = imu_hal_init();
    if (imu_status != IMU_HAL_OK) {
        system_health_report_boot_failure("imu_hal");
    }

    camera_hal_config_t cam_config = {
        .width = CAMERA_DEPTH_WIDTH,
        .height = CAMERA_DEPTH_HEIGHT,
        .fps = 30,
    };
    camera_hal_status_t cam_status = camera_hal_init(&cam_config);
    if (cam_status != CAMERA_HAL_OK) {
        system_health_report_boot_failure("camera_hal");
    } else {
        camera_hal_start_stream();
    }

    audio_hal_config_t audio_cfg = {
        .enable_mic = true,
        .enable_speaker = true,
        .volume_pct = 80,
    };
    if (audio_hal_init(&audio_cfg) != AUDIO_HAL_OK) {
        system_health_report_boot_failure("audio_hal");
    }

    battery_hal_config_t bat_cfg = {
        .alert_soc_pct = 10,
        .poll_interval_ms = 5000,
    };
    if (battery_hal_init(&bat_cfg) != BATTERY_HAL_OK) {
        system_health_report_boot_failure("battery_hal");
    }

    gps_hal_config_t gps_cfg = {
        .update_rate_hz = 10,
        .power_mode = 0,
        .max_hdop = 2.0f,
        .min_sats = 4,
    };
    if (gps_hal_init(&gps_cfg) != GPS_HAL_OK) {
        system_health_report_boot_failure("gps_hal");
    }
}

static void init_database(void)
{
    db_config_t db_cfg = {
        .path = "/data/vision_glasses.db",
        .backup_interval_hours = 24,
        .max_size_bytes = 104857600,
        .retention_days = 7,
        .enable_wal = true,
    };
    db_status_t db_status = db_init(&db_cfg);
    if (db_status != DB_OK) {
        system_health_report_boot_failure("database");
    }
}

static void init_ai_models(void)
{
    ei_config_t ei_cfg = {
        .arena_size = EI_ARENA_SIZE,
        .inference_timeout_ms = 100,
        .validate_crc = true,
    };
    if (ei_runtime_init(&ei_cfg) != EI_OK) {
        system_health_report_boot_failure("ei_runtime");
    }

    depth_config_t depth_cfg = {
        .confidence_threshold = 0.5f,
        .temporal_filter = true,
        .temporal_frames = 4,
        .min_valid_depth = 0.1f,
        .max_valid_depth = 20.0f,
    };
    if (depth_init(&depth_cfg) != DEPTH_OK) {
        system_health_report_boot_failure("depth");
    }

    od_config_t od_cfg = {
        .confidence_threshold = 0.5f,
        .nms_iou_threshold = 0.45f,
        .max_detections = 32,
        .input_width = 320,
        .input_height = 320,
        .compute_3d_pos = true,
    };
    for (int i = 0; i < OD_CLASSES / 8; i++) od_cfg.enabled_classes[i] = 0xFF;
    if (object_detection_init(&od_cfg) != OD_OK) {
        system_health_report_boot_failure("object_detection");
    }

    tracker_config_t trk_cfg = {
        .iou_threshold = 0.3f,
        .confidence_threshold = 0.5f,
        .max_stale_frames = 10,
        .max_tracks = 64,
        .velocity_threshold_mps = 0.5f,
        .use_kalman = true,
        .process_noise = 0.01f,
        .measurement_noise = 1.0f,
    };
    if (tracker_init(&trk_cfg) != TRACKER_OK) {
        system_health_report_boot_failure("tracker");
    }

    scene_config_t scene_cfg = {
        .ground_confidence = 0.7f,
        .hazard_distance_min = 0.5f,
        .free_space_threshold = 128,
        .enable_description = true,
        .max_scene_objects = 64,
    };
    if (scene_init(&scene_cfg) != SCENE_OK) {
        system_health_report_boot_failure("scene");
    }
}

static void init_navigation(void)
{
    nav_config_t nav_cfg = {
        .arrival_threshold_m = 5.0f,
        .recalc_threshold_m = 10.0f,
        .max_deviation_m = 10.0f,
        .turn_announce_distance_m = 30.0f,
        .gps_update_interval_ms = 200,
    };
    if (nav_engine_init(&nav_cfg) != NAV_OK) {
        system_health_report_boot_failure("nav_engine");
    }

    path_config_t path_cfg = {
        .grid_cell_size_m = 0.1f,
        .safe_corridor_min_m = 0.5f,
        .obstacle_inflation_radius_m = 0.3f,
        .max_path_iterations = 5000,
        .enable_smoothing = true,
    };
    if (path_planner_init(&path_cfg) != PATH_OK) {
        system_health_report_boot_failure("path_planner");
    }

    avoid_config_t avoid_cfg = {
        .safety_margin_m = 0.5f,
        .tti_warning_threshold_s = 3.0f,
        .tti_critical_threshold_s = 1.5f,
        .min_path_clearance_m = 0.5f,
        .max_consecutive_avoids = 3,
        .resume_delay_ms = 2000,
    };
    if (obstacle_avoid_init(&avoid_cfg) != AVOID_OK) {
        system_health_report_boot_failure("obstacle_avoid");
    }
}

static void init_voice(void)
{
    voice_config_t voice_cfg = {
        .wake_threshold = 0.7f,
        .command_threshold = 0.5f,
        .vad_level = 2,
        .denoise_enabled = true,
        .audio_timeout_ms = 5000,
    };
    if (voice_init(&voice_cfg) != VOICE_OK) {
        system_health_report_boot_failure("voice_recognition");
    }

    speech_config_t speech_cfg = {
        .volume_percent = 80,
        .speed = 1.0f,
        .voice_model = 0,
        .muted = false,
    };
    if (speech_init(&speech_cfg) != SPEECH_OK) {
        system_health_report_boot_failure("speech_synthesis");
    }
}

static void init_applications(void)
{
    if (app_manager_init() != APP_OK) {
        system_health_report_boot_failure("app_manager");
    }

    reminder_config_t rem_cfg = {
        .check_interval_ms = 1000,
        .advance_warning_s = 60,
        .enable_location_reminders = true,
        .max_reminders = 64,
    };
    if (reminder_init(&rem_cfg) != REMINDER_OK) {
        system_health_report_boot_failure("reminder");
    }

    mem_config_t mem_cfg = {
        .face_recognition_threshold = 0.6f,
        .enable_disorientation_check = true,
        .wandering_radius_m = 100.0f,
        .routine_check_interval_ms = 60000,
        .max_faces = 50,
    };
    if (mem_assist_init(&mem_cfg) != MEM_OK) {
        system_health_report_boot_failure("memory_assist");
    }

    emergency_config_t emerg_cfg = {
        .impact_threshold_g = 3.5f,
        .freefall_threshold_g = 0.3f,
        .orientation_change_threshold_deg = 60.0f,
        .impact_duration_ms = 200,
        .post_fall_immobility_s = 30,
        .false_positive_window_s = 10,
    };
    if (emergency_init(&emerg_cfg) != EMERG_OK) {
        system_health_report_boot_failure("emergency");
    }
}

int main(void)
{
    init_heap();
    init_drivers();
    init_middleware();
    init_hal();
    init_database();
    init_ai_models();
    init_navigation();
    init_voice();
    init_applications();

    task_manager_start_all();
    return 0;
}
