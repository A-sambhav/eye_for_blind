# Software Architecture Document

**Document ID:** SW-ARCH-001

**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients

**Author:** Senior Software Architect

**Revision:** 0.1

**Date:** 2026-07-27

---

## Table of Contents

1. Executive Summary
2. Layered Architecture
3. Operating System Layer
4. Drivers
5. Hardware Abstraction Layer (HAL)
6. Middleware
7. AI Layer
8. Navigation Layer
9. Decision Engine
10. Voice Layer
11. Application Layer
12. Database Layer
13. Logging Layer
14. Configuration Layer
15. UML Package Diagrams
16. Module Dependency Diagrams
17. Inter-Module Communication
18. Project Directory Structure
19. Threading Model
20. Startup Sequence
21. Shutdown Sequence
22. Exception Handling
23. Watchdog Mechanism

---

## 1. Executive Summary

The software architecture follows a layered design with strict separation of concerns. The system runs on Arduino UNO Q (ARM Cortex-M7, 600 MHz) with FreeRTOS as the real-time operating system. All AI inference is performed on-device using Edge Impulse SDK. Communication between software modules uses a publish-subscribe message bus with prioritized message queues to ensure real-time behavior for safety-critical paths.

---

## 2. Layered Architecture

```
+-------------------------------------------------------------------+
|                      LAYERED ARCHITECTURE                          |
+-------------------------------------------------------------------+
|                                                                   |
|  +-------------------------------------------------------------+  |
|  |                    APPLICATION LAYER                         |  |
|  |  Navigation  |  Voice  |  Safety  |  Reminder  |  Settings  |  |
|  |    App        |  App    |  App     |    App     |    App     |  |
|  +---------------------------+---------------------------------+  |
|                              |                                     |
|  +---------------------------+-----------------------------------+ |
|  |                     DECISION ENGINE                           | |
|  |  Context      |  Priority      |  State        |  Dialog     | |
|  |  Manager      |  Queue         |  Machine      |  Manager    | |
|  +---------------------------+-----------------------------------+ |
|                              |                                     |
|  +---------------------------+-----------------------------------+ |
|  |                     MIDDLEWARE                                | |
|  |  Message Bus  |  Task Manager  |  Timer  |  Event  |  Log    | |
|  |  (Pub/Sub)    |  (Scheduler)   |  Service |  Queue  |  Engine | |
|  +---------------------------+-----------------------------------+ |
|                              |                                     |
|  +---------------------------+-----------------------------------+ |
|  |              HARDWARE ABSTRACTION LAYER (HAL)                 | |
|  |  Camera     |  Audio    |  GPS     |  IMU    |  WiFi/BT     | |
|  |  Abstraction |  Manager  |  Manager |  Fusion |  Manager      | |
|  +---------------------------+-----------------------------------+ |
|                              |                                     |
|  +---------------------------+-----------------------------------+ |
|  |                       DRIVERS                                 | |
|  |  USB Host  |  I2C     |  SPI    |  UART  |  I2S  |  GPIO    | |
|  |  Camera    |  IMU     |  GPS    |  ESP   |  Audio |  BMS     | |
|  +---------------------------+-----------------------------------+ |
|                              |                                     |
|  +---------------------------+-----------------------------------+ |
|  |               OPERATING SYSTEM LAYER (FreeRTOS)               | |
|  |  Task       |  Queue   |  Semaphore |  Mutex  |  Timer       | |
|  |  Scheduler  |  Manager |  /Mutex    |         |  Manager      | |
|  +---------------------------+-----------------------------------+ |
|                              |                                     |
|  +---------------------------+-----------------------------------+ |
|  |                    HARDWARE (Arduino UNO Q)                    | |
|  +---------------------------------------------------------------+ |
+-------------------------------------------------------------------+
```

---

## 3. Operating System Layer

### 3.1 FreeRTOS Configuration

| Parameter | Value | Rationale |
|---|---|---|
| Kernel version | 10.5.1 | Latest stable |
| Tick rate | 1000 Hz (1 ms) | Required for 1 kHz IMU sampling |
| Max tasks | 32 | Sufficient for 15 planned tasks |
| Max queues | 20 | One per module + scratch |
| Max semaphores | 16 | For shared resource protection |
| Stack size (default) | 1024 words | 4 KB per task |
| Heap size | 256 KB | Config 5 (heap_5) for multiple regions |
| CPU clock | 600 MHz | Full speed for inference |

### 3.2 Task Priorities

| Priority Level | Value | Used By |
|---|---|---|
| IDLE | 0 | Idle task, system monitoring |
| LOW | 1 | Logging, battery monitoring, BMS |
| NORMAL | 2 | Database, config, WiFi/BT |
| HIGH | 3 | Audio output, GPS, voice processing |
| CRITICAL | 4 | AI inference, obstacle detection, IMU |
| ISR | 5 | Interrupt service routines (not tasks) |

---

## 4. Drivers

### 4.1 Driver Inventory

| Driver | Interface | IRQ | DMA | Buffer Strategy | Data Rate |
|---|---|---|---|---|---|
| USB Host Camera | USB 2.0 HS | Yes (USB) | Yes | Double-buffered (2 × 640 × 360) | 30 fps |
| I2C IMU | I2C (400 kHz) | Yes (INT1) | No | FIFO read on interrupt | 1 kHz |
| SPI GPS | SPI (10 MHz) | No (poll) | No | Circular buffer (256 bytes) | 10 Hz |
| UART ESP32 | UART (115200 baud) | Yes (RX) | No | Ring buffer (1024 bytes) | Variable |
| I2S Audio | I2S (48 kHz) | Yes (DMA) | Yes | Ping-pong (2 × 512 samples) | 48 kHz |
| GPIO BMS | Digital input | Yes (D4) | No | Debounced state machine | Event-driven |
| ADC Battery | ADC (12-bit) | No | No | Moving average (16 samples) | 1 Hz |

### 4.2 Driver Interface Example — IMU Driver

```c
// imu_driver.h
typedef struct {
    float accel_x, accel_y, accel_z;   // m/s²
    float gyro_x, gyro_y, gyro_z;      // rad/s
    float mag_x, mag_y, mag_z;         // µT
    float temperature;                  // °C
    uint32_t timestamp_us;             // System microsecond timestamp
} imu_data_t;

typedef void (*imu_callback_t)(imu_data_t *data);

int  imu_init(uint32_t sample_rate_hz);           // Returns 0 on success
void imu_register_callback(imu_callback_t cb);    // Called from ISR
int  imu_start_stream(void);                      // Returns 0 on success
int  imu_stop_stream(void);                       // Returns 0 on success
int  imu_self_test(void);                         // Returns 0 on pass
void imu_get_last(imu_data_t *out);               // Copy last sample
```

---

## 5. Hardware Abstraction Layer (HAL)

### 5.1 HAL Module Diagram

```
+-------------------+      +------------------+
| CameraHAL         |      | AudioHAL         |
| - init(camera_t)  |      | - init(audio_cfg)|
| - start_stream()  |      | - play(wav_buf)  |
| - stop_stream()   |      | - record(buf,len)|
| - get_frame()     |      | - set_volume(lvl)|
| - set_resolution()|      | - set_mute(bool) |
+--------+----------+      +--------+---------+
         |                          |
+--------+----------+      +--------+---------+
| GPSHAL             |      | IMUFusionHAL     |
| - init(gps_cfg)    |      | - init(imu_cfg)  |
| - get_position()   |      | - start()        |
| - get_satellites() |      | - stop()         |
| - set_update_rate()|      | - get_orientation|
| - power_save()     |      | - get_linear_acc |
+--------+----------+      +--------+---------+
         |                          |
+--------+----------+      +--------+---------+
| WiFiBtHAL          |      | BMSHAL           |
| - init()           |      | - init()         |
| - connect(ssid)    |      | - get_voltage()  |
| - send(data,len)   |      | - get_current()  |
| - recv(buf,len)    |      | - get_temp()     |
| - ble_pair()       |      | - get_status()   |
+-------------------+      +------------------+
```

### 5.2 HAL Design Principles

- All HAL functions return `int` (0 = success, negative = error code)
- All HAL modules have `init()`, `start()`, `stop()`, `deinit()` lifecycle
- HAL hides interrupt details from upper layers
- HAL provides blocking and non-blocking variants where appropriate
- Hardware errors are converted to standard error codes (`HAL_ERR_I2C`, `HAL_ERR_SPI`, etc.)

---

## 6. Middleware

### 6.1 Message Bus (Pub/Sub)

```
+-------------------------------------------------------------------+
|                    MESSAGE BUS ARCHITECTURE                        |
+-------------------------------------------------------------------+
|                                                                   |
|  PUBLISHERS                    SUBSCRIBERS                        |
|  +----------+                 +-------------+                     |
|  | IMU      |----+            | Decision    |                     |
|  | Driver   |    |            | Engine      |                     |
|  +----------+    |            +-------------+                     |
|  +----------+    |            +-------------+                     |
|  | GPS HAL  |----+---> MSG   | Navigation  |                     |
|  +----------+    |    BUS    | App         |                     |
|  +----------+    |            +-------------+                     |
|  | Camera   |----+            +-------------+                     |
|  | HAL      |    |            | Voice       |                     |
|  +----------+    |            | Assistant   |                     |
|  +----------+    |            +-------------+                     |
|  | BMS      |----+            +-------------+                     |
|  | HAL      |    |            | Safety      |                     |
|  +----------+    |            | App         |                     |
|  +----------+    |            +-------------+                     |
|  | Voice    |----+            +-------------+                     |
|  | Recog    |    |            | Logging     |                     |
|  +----------+    |            | Service     |                     |
|                   |            +-------------+                     |
+-------------------------------------------------------------------+
```

### 6.2 Message Types

```c
typedef enum {
    MSG_IMU_DATA,           // imu_data_t
    MSG_GPS_POSITION,       // gps_position_t
    MSG_GPS_STATUS,         // gps_status_t
    MSG_DEPTH_FRAME,        // depth_frame_t (640×360)
    MSG_RGB_FRAME,          // rgb_frame_t (640×480)
    MSG_OBJECT_DETECTED,    // detection_t
    MSG_OBSTACLE_ALERT,     // obstacle_t
    MSG_FACE_RECOGNIZED,    // face_t
    MSG_TEXT_READ,          // text_result_t
    MSG_VOICE_COMMAND,      // voice_command_t
    MSG_VOICE_WAKE_WORD,    // wake_word_t
    MSG_NAV_DIRECTION,      // nav_direction_t
    MSG_NAV_STATUS,         // nav_status_t
    MSG_BATTERY_LEVEL,      // battery_level_t
    MSG_BMS_FAULT,          // bms_fault_t
    MSG_FALL_DETECTED,      // fall_event_t
    MSG_GEO_ALERT,          // geo_alert_t
    MSG_SYSTEM_ERROR,       // system_error_t
    MSG_SYSTEM_SHUTDOWN,    // void
    MSG_BUTTON_PRESS,       // button_event_t
    MSG_REMINDER,           // reminder_t
    MSG_CONFIG_CHANGE,      // config_change_t
} message_type_t;
```

### 6.3 Message Queue Configuration

| Queue | Length | Item Size | Used By | Priority |
|---|---|---|---|---|
| High-priority queue | 32 | 128 bytes | AI, Obstacle, IMU, Fall | > 0 |
| Normal queue | 64 | 128 bytes | GPS, Nav, Voice, Audio | 0 |
| Low-priority queue | 32 | 64 bytes | Logging, Battery, DB | < 0 |

### 6.4 Timer Service

| Timer ID | Period | Callback | Purpose |
|---|---|---|---|
| TMR_BATTERY | 5000 ms | battery_check_cb | ADC read + level estimation |
| TMR_GPS_ALMANAC | 3600000 ms | gps_almanac_refresh | Refresh GPS ephemeris (1 hour) |
| TMR_NAV_UPDATE | 1000 ms | nav_update_cb | Trigger navigation update |
| TMR_SYSTEM_HEALTH | 30000 ms | health_check_cb | Watchdog + memory check |
| TMR_LOG_FLUSH | 60000 ms | log_flush_cb | Flush log buffer to flash |

---

## 7. AI Layer

### 7.1 AI Module Architecture

```
+-------------------------------------------------------------------+
|                        AI LAYER                                    |
+-------------------------------------------------------------------+
|                                                                   |
|  +------------------+  +------------------+  +------------------+ |
|  | Object Detection  |  | Depth Processor  |  | Scene            | |
|  | Pipeline          |  | Pipeline         |  | Understanding    | |
|  | (Edge Impulse)    |  | (Edge Impulse)   |  | Pipeline         | |
|  +--------+---------+  +--------+---------+  +---------+--------+ |
|           |                       |                      |         |
|           +-----------+-----------+----------+-----------+         |
|                       |                      |                     |
|                       v                      v                     |
|              +--------+----------------------+--------+            |
|              |            AI Manager                |            |
|              |  - Pipeline scheduler                 |            |
|              |  - Model loading/unloading             |           |
|              |  - Inference orchestration             |           |
|              |  - Result post-processing              |           |
|              +--------+----------------------+--------+            |
|                       |                      |                     |
|              +--------+---------+  +---------+--------+            |
|              | Edge Impulse     |  | Post-processor   |            |
|              | Runtime SDK      |  | NMS, threshold,  |            |
|              | (EI model)       |  | coordinate calc  |            |
|              +------------------+  +------------------+            |
|                                                                   |
+-------------------------------------------------------------------+
```

### 7.2 AI Inference Pipeline

```
Camera Frame (USB) → Preprocess (resize, normalize) → EI Model → Postprocess → Message Bus
       |                      |                            |            |
  640×360                320×320                        Output      detections_t
  30 fps                 uint8[]                        tensor      

Total pipeline latency target: < 200 ms (PR-001)
  - Frame capture:       10 ms
  - USB transfer:        15 ms
  - Preprocessing:       15 ms
  - Inference:          120 ms (estimated for 320×320 model)
  - Postprocessing:      10 ms
  - Message publish:      5 ms
  - Audio output:        25 ms
  ---------------------------------
  Total:                200 ms
```

---

## 8. Navigation Layer

See dedicated Navigation System document (NAV-ARCH-001) for full details.

---

## 9. Decision Engine

See dedicated Decision Engine document (DEC-ENG-001) for full details.

---

## 10. Voice Layer

### 10.1 Voice Pipeline

```
+-------------------------------------------------------------------+
|                      VOICE PIPELINE                                |
+-------------------------------------------------------------------+
|                                                                   |
|  MICROPHONE (I2S, 48 kHz, 16-bit, mono)                          |
|       |                                                           |
|       v                                                           |
|  +-----------+                                                    |
|  | Noise     |  → Wiener filter, adaptive noise cancellation       |
|  | Reduction |    Reference: ambient mic (future)                  |
|  +-----------+                                                    |
|       |                                                           |
|       v                                                           |
|  +-----------+                                                    |
|  | VAD       |  → Voice Activity Detection (energy-based)         |
|  | Detector  |    Threshold: adaptive SNR > 12 dB                 |
|  +-----------+                                                    |
|       |                                                           |
|       +-------------------+                                       |
|       |                   |                                       |
|       v                   v                                       |
|  +-----------+    +---------------+                               |
|  | Wake Word |    | Command       |  → Keyword spotting           |
|  | Detector  |    | Recognition   |    (25 keywords,              |
|  | ("Hey     |    | (Edge Impulse)|     Edge Impulse model)       |
|  |  Glass")  |    |               |                               |
|  +-----------+    +-------+-------+                               |
|                           |                                       |
|                           v                                       |
|  +----------------------+                                         |
|  | Intent Parser        |  → Regex + entity extraction            |
|  | "navigate to [home]" |    Intent: navigation                   |
|  +----------------------+    Entity: destination = home           |
|                           |                                       |
|                           v                                       |
|  +----------------------+                                         |
|  | Dialog Manager       |  → State machine for multi-turn         |
|  |                      |    "Where do you want to go?"           |
|  +----------------------+    User: "Home"                         |
|                           |    Action: start navigation to home   |
|                           v                                       |
|  +----------------------+                                         |
|  | TTS Engine           |  → Text-to-Speech (pre-recorded or     |
|  | (Bone Conduction)    |    Edge Impulse audio model)           |
|  +----------------------+    "Navigating to home. Turn left."    |
|                           |                                       |
|                           v                                       |
|  SPEAKER (I2S, 48 kHz, bone conduction)                          |
+-------------------------------------------------------------------+
```

### 10.2 Voice Module Functions

```c
// voice_layer.h
typedef enum {
    INTENT_NAVIGATE, INTENT_STOP, INTENT_HELP, INTENT_HOME,
    INTENT_READ, INTENT_REMIND, INTENT_SETTINGS, INTENT_EMERGENCY,
    INTENT_DESCRIBE, INTENT_SILENT, INTENT_PRIVACY, INTENT_BATTERY
} intent_t;

typedef struct {
    intent_t intent;
    char     entities[4][32];   // e.g., "home", "07:00", "medication"
    float    confidence;
} parsed_command_t;

int  voice_init(void);
void voice_start_listening(void);
void voice_stop_listening(void);
int  voice_speak(const char *text, uint32_t duration_ms);
int  voice_play_alert(alert_type_t type);
void voice_set_volume(uint8_t level);  // 0-10
```

---

## 11. Application Layer

### 11.1 Application Modules

```
+-------------------------------------------------------------------+
|                    APPLICATION LAYER                               |
+-------------------------------------------------------------------+
|                                                                   |
|  +------------------+  +------------------+  +------------------+ |
|  | Navigation App   |  | Voice App        |  | Safety App       | |
|  | - manages route  |  | - dialog flow    |  | - fall detection | |
|  | - turn tracking  |  | - command queue  |  | - geo-fencing    | |
|  | - progress voice |  | - TTS queue      |  | - emergency call  | |
|  +--------+---------+  +--------+---------+  +---------+--------+ |
|           |                       |                      |         |
|  +--------+---------+  +--------+---------+  +---------+--------+ |
|  | Reminder App      |  | Settings App     |  | Describe App     | |
|  | - schedule check  |  | - config read    |  | - scene summary  | |
|  | - time alerts     |  | - config write   |  | - face announce  | |
|  | - geo triggers    |  | - persist        |  | - text read      | |
|  +------------------+  +------------------+  +------------------+ |
|                                                                   |
+-------------------------------------------------------------------+
```

### 11.2 Application State Machine (Global)

```
                  +----------+
                  |  BOOT    |
                  +----+-----+
                       |
                       v
                  +----------+
        +-------->|  IDLE    |<---------+
        |         +----+-----+          |
        |              |                |
        |    +---------+--------+       |
        |    |         |        |       |
        |    v         v        v       |
        | +------+ +------+ +------+   |
        | | NAV  | | VOICE| | SAFE |   |
        | | APP  | | APP  | | APP  |   |
        | +------+ +------+ +------+   |
        |    |         |        |       |
        |    +---------+--------+       |
        |              |                |
        |              v                |
        |         +---------+           |
        |         | ACTIVE  |           |
        |         +----+----+           |
        |              |                |
        |              v                |
        |         +---------+           |
        |         | SLEEP   |---------->+ (low power, IMU wake)
        |         +---------+           |
        |              |                |
        +--------------+ (wake event)
                       |
                       v
                  +----------+
                  | SHUTDOWN |
                  +----------+
```

---

## 12. Database Layer

See dedicated Database Design document (DB-ARCH-001) for full details.

---

## 13. Logging Layer

### 13.1 Logging Architecture

```
+-------------------------------------------------------------------+
|                      LOGGING LAYER                                 |
+-------------------------------------------------------------------+
|                                                                   |
|  +------------------+                                             |
|  | Application APIs |  LOG_INFO, LOG_WARN, LOG_ERROR, LOG_DEBUG   |
|  +--------+---------+                                             |
|           |                                                       |
|           v                                                       |
|  +------------------+    +-----------+    +-------------------+  |
|  | Log Formatter    |--->| Ring      |--->| Log Consumer      |  |
|  | [timestamp]      |    | Buffer    |    | - UART (debug)    |  |
|  | [level]          |    | (256      |    | - Flash (persist) |  |
|  | [module] message |    | entries)  |    | - BLE (optional)  |  |
|  +------------------+    +-----------+    +-------------------+  |
|                                                 |                 |
|                                                 v                 |
|                                          +-------------------+   |
|                                          | Flash Storage     |   |
|                                          | (circular, 1K     |   |
|                                          |  entries max)     |   |
|                                          +-------------------+   |
+-------------------------------------------------------------------+
```

### 13.2 Log Levels

| Level | Value | Color (debug UART) | Persist | Purpose |
|---|---|---|---|---|
| LOG_ERROR | 0 | Red | Always | System faults, crashes |
| LOG_WARN | 1 | Yellow | Always | Degraded modes |
| LOG_INFO | 2 | Green | Configurable | State changes |
| LOG_DEBUG | 3 | Blue | Never | Development only |

---

## 14. Configuration Layer

### 14.1 Configuration Storage

```c
// config_types.h
typedef struct {
    uint8_t  volume;                    // 0-10
    uint8_t  language;                  // 0 = English
    char     wake_word[16];             // "hey glass"
    uint8_t  wake_word_threshold;       // 0-100
    float    geo_fence_zones[10][4];    // [lat, lon, radius_m, enabled]
    char     emergency_contact[20][64]; // name, phone
    uint8_t  face_count;                // 0-20
    uint32_t face_hashes[20][8];        // Irreversible embeddings
    bool     privacy_mode;              // Disable cameras/GPS
    bool     verbose_nav;               // Detailed navigation prompts
    uint8_t  audio_profile;             // 0=voice, 1=balanced, 2=bass
    reminder_t reminders[32];           // Schedule items
} system_config_t;
```

### 14.2 Configuration Lifecycle

```
Boot → Load from flash → Validate CRC → Apply defaults if invalid → Runtime updates → Save on change → Flush to flash
```

- Configuration is stored in external QSPI flash (W25Q128JV)
- CRC32 protects configuration integrity
- Atomic writes: write new config + CRC, then swap pointer
- Save triggered: on explicit user save, on shutdown, every 5 min if dirty

---

## 15. UML Package Diagrams

### 15.1 Package Structure

```
+-------------------------------------------------------------------+
|                        PACKAGE DIAGRAM                             |
+-------------------------------------------------------------------+
|                                                                   |
|  +-------------------+    +-------------------+                   |
|  | ar::drivers       |    | ar::hal           |                   |
|  | - usb_camera      |<-+ | - camera_hal     |                   |
|  | - i2c_imu         |  | | - audio_hal      |                   |
|  | - spi_gps         |  | | - gps_hal        |                   |
|  | - uart_esp        |  | | - imu_hal        |                   |
|  | - i2s_audio       |  | | - wifi_hal       |                   |
|  | - gpio_bms        |  | | - bms_hal        |                   |
|  | - adc_battery     |  | +-------^----------+                   |
|  +--------+----------+  |         |                              |
|           |             +---------+                              |
|           |                                                      |
|  +--------+----------+    +-------------------+                   |
|  | ar::middleware     |    | ar::ai            |                   |
|  | - message_bus      |    | - object_detector |                   |
|  | - task_manager     |    | - depth_processor |                   |
|  | - timer_service    |    | - face_recognizer |                   |
|  | - event_queue      |    | - text_reader    |                   |
|  +--------+----------+    | - scene_analyzer  |                   |
|           |               +--------+----------+                   |
|           |                        |                              |
|  +--------+----------+    +--------+----------+                   |
|  | ar::decision      |    | ar::navigation    |                   |
|  | - context_manager  |    | - gps_nav        |                   |
|  | - priority_queue   |    | - imu_fusion     |                   |
|  | - state_machine    |    | - path_planner   |                   |
|  | - dialog_manager   |    | - obstacle_avoid |                   |
|  +--------+----------+    +--------+----------+                   |
|           |                        |                              |
|           +-----------+------------+                              |
|                       |                                           |
|              +--------+----------+                                |
|              | ar::application   |                                |
|              | - nav_app        |                                |
|              | - voice_app      |                                |
|              | - safety_app     |                                |
|              | - reminder_app   |                                |
|              +------------------+                                |
|                                                                   |
+-------------------------------------------------------------------+
```

---

## 16. Module Dependency Diagrams

### 16.1 Dependency Graph (Top-Down)

```
Application Layer
    ├── depends on → Decision Engine
    ├── depends on → Voice Layer
    └── depends on → Navigation Layer

Decision Engine
    ├── depends on → AI Layer (for perception results)
    ├── depends on → Navigation Layer (for position)
    ├── depends on → Middleware (message bus)
    └── depends on → Configuration Layer

AI Layer
    ├── depends on → HAL (Camera, IMU)
    ├── depends on → Middleware (message bus)
    └── depends on → Database Layer (face hashes, models)

Navigation Layer
    ├── depends on → HAL (GPS, IMU)
    ├── depends on → AI Layer (obstacle detection)
    ├── depends on → Middleware (message bus)
    └── depends on → Configuration Layer

Voice Layer
    ├── depends on → HAL (Audio, Microphone)
    ├── depends on → AI Layer (keyword spotting)
    ├── depends on → Decision Engine (dialog)
    └── depends on → Middleware (message bus)

Middleware
    ├── depends on → HAL (for hardware status)
    └── depends on → FreeRTOS (queues, tasks)

HAL
    ├── depends on → Drivers
    └── depends on → FreeRTOS (mutexes, ISR)

Drivers
    └── depends on → Arduino core (GPIO, I2C, SPI, UART, I2S, USB)

Database Layer
    ├── depends on → HAL (Flash storage)
    └── depends on → Configuration Layer

Logging Layer
    ├── depends on → HAL (UART debug, Flash)
    └── depends on → Middleware (message bus for async logs)

Configuration Layer
    └── depends on → HAL (Flash storage, CRC)
```

### 16.2 Acyclic Dependency Rule

**No circular dependencies are allowed.** The dependency graph is strictly acyclic. The layering order is:

```
Application → Decision → AI/Navigation/Voice → Middleware → HAL → Drivers → OS → Hardware
```

If a circular dependency is discovered during implementation, the shared functionality must be extracted into a lower-layer service.

---

## 17. Inter-Module Communication

### 17.1 Communication Matrix

```
              | NavApp | VoiceApp | SafetyApp | Decision | AI | Nav | Voice | Middleware | HAL | DB
--------------+--------+----------+-----------+----------+----+-----+-------+-----------+-----+----
NavApp        |   -    | MsgBus   |   MsgBus  | MsgBus   |  - | Msg | Msg   |   MsgBus  |  -  | API
VoiceApp      | MsgBus |   -      |   MsgBus  | MsgBus   |  - |  -  |  -   |   MsgBus  | API | API
SafetyApp     | MsgBus | MsgBus   |    -      | MsgBus   |  - |  -  |  -   |   MsgBus  | API | API
Decision      |   -    |   -      |    -      |   -      | Msg | Msg | Msg  |   MsgBus  |  -  |  -
AI Layer      |   -    |   -      |    -      | MsgBus   |  - |  -  |  -   |   MsgBus  | API | API
Nav Layer     |   -    |   -      |    -      | MsgBus   | Msg |  -  |  -   |   MsgBus  | API |  -
Voice Layer   |   -    |   -      |    -      | MsgBus   | Msg |  -  |  -   |   MsgBus  | API |  -
Middleware    |   -    |   -      |    -      |   -      |  - |  -  |  -   |    -      |  -  |  -
HAL           |   -    |   -      |    -      |   -      |  - |  -  |  -   |   API     |  -  |  -
DB Layer      |   -    |   -      |    -      |   -      |  - |  -  |  -   |   API     | API |  -
```

**Legend:**
- MsgBus = Publish-subscribe message bus
- API = Direct function call interface
- API (OS) = FreeRTOS API call
- Msg = Message queue (direct sender-receiver)

### 17.2 Data Flow for a Typical Scenario

**Scenario: User walks toward a staircase, system warns them.**

```
1. Depth Camera        → USB frame → Camera Driver → CameraHAL
2. CameraHAL           → MSG_DEPTH_FRAME → Message Bus
3. AI Depth Processor  ← subscribe(MSG_DEPTH_FRAME)
4. AI Depth Processor  → Edge Impulse inference → detects downward edge
5. AI Depth Processor  → MSG_OBSTACLE_ALERT (obstacle_t) → Message Bus
6. Decision Engine     ← subscribe(MSG_OBSTACLE_ALERT)
7. Decision Engine     → Priority Queue → classify hazard = HIGH
8. Decision Engine     → Check context: walking, outdoor
9. Decision Engine     → state_machine_transition(NAV_APP, OBSTACLE)
10. Decision Engine    → MSG_NAV_STATUS (obstacle_ahead) → Message Bus
11. Navigation App     ← subscribe(MSG_NAV_STATUS)
12. Navigation App     → "Staircase ahead, 2 meters. Stop and check." → Voice Layer
13. Voice Layer        → I2S buffer → Bone conduction speaker
14. Logging Layer      ← subscribe(MSG_SYSTEM_LOG) → writes to Flash
```

**End-to-end latency:** ~180 ms (within PR-001 limit of 200 ms)

---

## 18. Project Directory Structure

```
smart_glass_firmware/
│
├── README.md
├── LICENSE
├── CMakeLists.txt                   # Top-level CMake
├── platformio.ini                   # PlatformIO configuration
├── sdkconfig                        # FreeRTOS/Arduino config
│
├── src/
│   ├── main.c                       # Entry point, init sequence
│   ├── freertos_hooks.c             # vApplicationIdleHook, vApplicationTickHook
│   │
│   ├── drivers/
│   │   ├── include/
│   │   │   ├── usb_camera_drv.h
│   │   │   ├── i2c_imu_drv.h
│   │   │   ├── spi_gps_drv.h
│   │   │   ├── uart_esp_drv.h
│   │   │   ├── i2s_audio_drv.h
│   │   │   ├── gpio_bms_drv.h
│   │   │   └── adc_battery_drv.h
│   │   └── src/
│   │       ├── usb_camera_drv.c
│   │       ├── i2c_imu_drv.c
│   │       ├── spi_gps_drv.c
│   │       ├── uart_esp_drv.c
│   │       ├── i2s_audio_drv.c
│   │       ├── gpio_bms_drv.c
│   │       └── adc_battery_drv.c
│   │
│   ├── hal/
│   │   ├── include/
│   │   │   ├── camera_hal.h
│   │   │   ├── audio_hal.h
│   │   │   ├── gps_hal.h
│   │   │   ├── imu_hal.h
│   │   │   ├── wifi_hal.h
│   │   │   └── bms_hal.h
│   │   └── src/
│   │       ├── camera_hal.c
│   │       ├── audio_hal.c
│   │       ├── gps_hal.c
│   │       ├── imu_hal.c
│   │       ├── wifi_hal.c
│   │       └── bms_hal.c
│   │
│   ├── middleware/
│   │   ├── include/
│   │   │   ├── message_bus.h
│   │   │   ├── task_manager.h
│   │   │   ├── timer_service.h
│   │   │   ├── event_queue.h
│   │   │   └── system_health.h
│   │   └── src/
│   │       ├── message_bus.c
│   │       ├── task_manager.c
│   │       ├── timer_service.c
│   │       ├── event_queue.c
│   │       └── system_health.c
│   │
│   ├── ai/
│   │   ├── include/
│   │   │   ├── ai_manager.h
│   │   │   ├── object_detector.h
│   │   │   ├── depth_processor.h
│   │   │   ├── face_recognizer.h
│   │   │   ├── text_reader.h
│   │   │   └── scene_analyzer.h
│   │   ├── src/
│   │   │   ├── ai_manager.c
│   │   │   ├── object_detector.c
│   │   │   ├── depth_processor.c
│   │   │   ├── face_recognizer.c
│   │   │   ├── text_reader.c
│   │   │   └── scene_analyzer.c
│   │   └── models/
│   │       ├── object_detection.eim
│   │       ├── depth_estimation.eim
│   │       ├── face_recognition.eim
│   │       ├── text_recognition.eim
│   │       ├── scene_classification.eim
│   │       └── wake_word.eim
│   │
│   ├── decision/
│   │   ├── include/
│   │   │   ├── decision_engine.h
│   │   │   ├── context_manager.h
│   │   │   ├── priority_queue.h
│   │   │   ├── state_machine.h
│   │   │   └── dialog_manager.h
│   │   └── src/
│   │       ├── decision_engine.c
│   │       ├── context_manager.c
│   │       ├── priority_queue.c
│   │       ├── state_machine.c
│   │       └── dialog_manager.c
│   │
│   ├── navigation/
│   │   ├── include/
│   │   │   ├── nav_manager.h
│   │   │   ├── gps_navigator.h
│   │   │   ├── imu_navigator.h
│   │   │   ├── path_planner.h
│   │   │   └── obstacle_avoider.h
│   │   └── src/
│   │       ├── nav_manager.c
│   │       ├── gps_navigator.c
│   │       ├── imu_navigator.c
│   │       ├── path_planner.c
│   │       └── obstacle_avoider.c
│   │
│   ├── voice/
│   │   ├── include/
│   │   │   ├── voice_manager.h
│   │   │   ├── wake_word.h
│   │   │   ├── speech_recognizer.h
│   │   │   ├── intent_parser.h
│   │   │   └── tts_engine.h
│   │   └── src/
│   │       ├── voice_manager.c
│   │       ├── wake_word.c
│   │       ├── speech_recognizer.c
│   │       ├── intent_parser.c
│   │       └── tts_engine.c
│   │
│   ├── application/
│   │   ├── include/
│   │   │   ├── nav_app.h
│   │   │   ├── voice_app.h
│   │   │   ├── safety_app.h
│   │   │   ├── reminder_app.h
│   │   │   ├── settings_app.h
│   │   │   └── describe_app.h
│   │   └── src/
│   │       ├── nav_app.c
│   │       ├── voice_app.c
│   │       ├── safety_app.c
│   │       ├── reminder_app.c
│   │       ├── settings_app.c
│   │       └── describe_app.c
│   │
│   ├── database/
│   │   ├── include/
│   │   │   ├── db_manager.h
│   │   │   ├── flash_storage.h
│   │   │   └── config_storage.h
│   │   └── src/
│   │       ├── db_manager.c
│   │       ├── flash_storage.c
│   │       └── config_storage.c
│   │
│   └── logging/
│       ├── include/
│       │   └── logger.h
│       └── src/
│           └── logger.c
│
├── test/
│   ├── unit/
│   │   ├── test_imu_driver.c
│   │   ├── test_gps_hal.c
│   │   ├── test_message_bus.c
│   │   ├── test_priority_queue.c
│   │   ├── test_state_machine.c
│   │   ├── test_intent_parser.c
│   │   └── test_nav_manager.c
│   ├── integration/
│   │   ├── test_ai_pipeline.c
│   │   ├── test_nav_decision.c
│   │   ├── test_voice_decision.c
│   │   └── test_full_scenario.c
│   └── mock/
│       ├── mock_camera.c
│       ├── mock_imu.c
│       ├── mock_gps.c
│       ├── mock_audio.c
│       └── mock_flash.c
│
├── tools/
│   ├── log_parser.py                # Parse UART log output
│   ├── model_converter.py           # Convert trained models to .eim
│   ├── flash_programmer.py          # Flash firmware via USB
│   └── config_generator.py          # Generate config binary
│
└── docs/
    ├── SRS-001.md
    ├── HW-ARCH-001.md
    ├── SW-ARCH-001.md
    ├── AI-ARCH-001.md
    ├── DEC-ENG-001.md
    ├── NAV-ARCH-001.md
    ├── VOICE-ARCH-001.md
    ├── DB-ARCH-001.md
    ├── COMM-ARCH-001.md
    ├── TEST-001.md
    ├── ROADMAP-001.md
    └── SDD-001.md
```

---

## 19. Threading Model

### 19.1 Task Definitions

| Task Name | Priority | Stack (words) | Period/Trigger | Core Function |
|---|---|---|---|---|
| `idle_task` | 0 | 256 | Always | System health, power management |
| `imu_task` | 4 | 512 | IRQ (1 kHz) | Read IMU, publish MSG_IMU_DATA |
| `camera_task` | 4 | 1024 | 33 ms (30 fps) | Capture frame, publish MSG_DEPTH_FRAME |
| `gps_task` | 3 | 512 | 100 ms (10 Hz) | Poll GPS, publish MSG_GPS_POSITION |
| `audio_in_task` | 3 | 512 | IRQ (48 kHz) | I2S microphone, VAD, wake word |
| `audio_out_task` | 3 | 512 | Msg queue | TTS playback, audio alerts |
| `ai_inference_task` | 4 | 2048 | Msg queue | Edge Impulse inference |
| `ai_postprocess_task` | 3 | 512 | Msg queue | NMS, threshold, coordinate calc |
| `nav_task` | 3 | 1024 | 1 s timer | GPS-IMU fusion, path planning |
| `decision_task` | 4 | 1024 | Msg queue | Hazard prioritization, state machine |
| `voice_task` | 3 | 1024 | Msg queue | Intent parsing, TTS scheduling |
| `safety_task` | 4 | 512 | Msg queue + 50 ms poll | Fall detection, geo-fence |
| `battery_task` | 1 | 256 | 5 s timer | ADC read, level estimation |
| `wifi_task` | 2 | 512 | Event + 10 s poll | ESP32 AT communication |
| `db_task` | 2 | 512 | Msg queue | Flash read/write, config persist |
| `log_task` | 1 | 512 | 60 s timer | Flush log buffer to flash |

### 19.2 CPU Utilization Estimate

| Task | CPU per Execution | Executions/s | CPU % |
|---|---|---|---|
| imu_task | 0.05 ms | 1000 | 5.0% |
| camera_task | 5.0 ms | 30 | 15.0% |
| gps_task | 0.5 ms | 10 | 0.5% |
| audio_in_task | 0.5 ms | 100 | 5.0% |
| audio_out_task | 0.5 ms | 50 | 2.5% |
| ai_inference_task | 120.0 ms | 15 | 60.0%* |
| ai_postprocess_task | 5.0 ms | 15 | 7.5% |
| nav_task | 2.0 ms | 1 | 0.2% |
| decision_task | 1.0 ms | 30 | 3.0% |
| voice_task | 5.0 ms | 2 | 1.0% |
| safety_task | 1.0 ms | 20 | 2.0% |
| battery_task | 0.2 ms | 0.2 | <0.1% |
| wifi_task | 1.0 ms | 5 | 0.5% |
| db_task | 2.0 ms | 1 | 0.2% |
| log_task | 5.0 ms | 0.017 | <0.1% |
| **Total** | | | **~102.5%** |

***Note:** 60% CPU for AI inference assumes running at 15 fps (every other frame). At 30 fps, AI would consume 120% of CPU — impossible. **This confirms the requirement to run inference at 15 fps maximum**, which still meets the obstacle detection latency requirement of 200 ms.

The 2.5% over-allocation is manageable because not all tasks run at full rate simultaneously (battery, wifi, db, log are low-rate or event-driven).

---

## 20. Startup Sequence

```
                    SMART GLASSES STARTUP SEQUENCE
                    ==============================

Time    Component          Action
────    ─────────          ──────
0 ms    Battery            Power applied
                           BMS enables VSYS rail
5 ms    3V3 LDOs           Rails stabilize
10 ms   Arduino UNO Q      ROM bootloader executes
                           Jump to application flash
15 ms   main()             - Initialize hardware (cache, clock, PLL @ 600 MHz)
                           - Initialize FreeRTOS kernel
20 ms                       - Create startup task (priority 3)
                           - Start scheduler
25 ms   startup_task       - Initialize GPIO (LEDs, button, BMS)
                           - Initialize I2C (400 kHz)
                           - Initialize SPI (10 MHz)
                           - Initialize UART (debug 115200, ESP 115200)
                           - Initialize I2S (48 kHz)
                           - Initialize USB host
40 ms                       - Initialize drivers:
                             - IMU driver (self-test → pass/fail)
                             - GPS driver (config 10 Hz update)
                             - Audio driver (I2S, 48 kHz, 16-bit)
                             - BMS driver (read status)
                             - Battery ADC (config 12-bit, 1 Hz)
55 ms                       - Initialize HAL modules:
                             - CameraHAL (config 640×360, 30 fps)
                             - AudioHAL (config volume, TTS buffer)
                             - GPSHAL (config update rate)
                             - IMUHAL (config orientation fusion)
                             - WiFiHAL (config ESP32 baud)
70 ms                       - Initialize middleware:
                             - Message bus (create queues, register types)
                             - Timer service (start battery timer, health timer)
                             - Event queue (create system event queue)
85 ms                       - Initialize database, load configuration from flash
                             - Load system_config_t → global config struct
100 ms                      - Initialize AI layer:
                             - Load Edge Impulse models from flash → SRAM
                             - Object detection model (1.2 MB)
                             - Depth processing model (0.6 MB)
                             - Face recognition model (0.4 MB)
                             - Wake word model (0.2 MB)
120 ms                      - Start sensor streams:
                             - IMU stream (1 kHz, interrupt-driven)
                             - Camera stream (30 fps, USB isochronous)
                             - GPS stream (10 Hz, SPI polling)
                             - Audio stream (48 kHz, I2S DMA)
140 ms                      - Initialize application modules:
                             - NavApp (load saved route, set state = IDLE)
                             - VoiceApp (start listening for wake word)
                             - SafetyApp (set geo-fences, arm fall detection)
                             - ReminderApp (load schedule, start timer)
                             - DescribeApp (ready for scene description)
150 ms                      - Start decision engine task
                           - Signal system ready → blink LED green
                           - Audio: "System ready"
                           - Delete startup task
160 ms   System             Fully operational
```

---

## 21. Shutdown Sequence

```
                    SMART GLASSES SHUTDOWN SEQUENCE
                    ===============================

Trigger Conditions:
  - Battery voltage < 3.25 V (PR-011 threshold)
  - BMS fault (over-temperature, over-current)
  - User button hold (5 seconds)
  - Software crash (watchdog timeout)

Sequence:

1. Shutdown Trigger Detected (by safety_task or ISR)
   ├── Safety task: publish MSG_SYSTEM_SHUTDOWN
   ├── Decision engine: stop all non-critical processing
   └── Voice task: play "Shutting down" (200 ms audio)
          │
          v
2. Stop Sensor Streams (t = 0 ms)
   ├── Stop IMU stream
   ├── Stop camera stream
   ├── Stop GPS stream
   └── Stop audio IN stream
          │
          v
3. Save State (t = 10 ms)
   ├── NavApp → save current position, route state
   ├── VoiceApp → save dialog state
   ├── SafetyApp → save geo-fence state
   ├── ReminderApp → save pending reminders
   └── DB task → flush all pending writes
          │
          v
4. Stop Tasks (t = 30 ms)
   ├── Suspend AI inference task
   ├── Suspend decision task
   ├── Suspend nav task
   ├── Suspend voice task
   └── Suspend all application tasks
          │
          v
5. Hardware De-init (t = 50 ms)
   ├── Disable 5V boost converter (depth camera off)
   ├── Disable I2C, SPI, I2S peripherals
   ├── Set all GPIO to safe state (input, pull-down)
   └── LED off
          │
          v
6. System Halt (t = 70 ms)
   ├── Flush debug UART
   ├── __disable_irq()
   ├── Enter deep sleep (if battery > threshold)
   │   └── Wake on RTC alarm or button press
   └── OR disconnect BMS (if battery critical)
       └── System off
```

---

## 22. Exception Handling

### 22.1 Exception Categories

| Category | Examples | Recovery Strategy |
|---|---|---|
| Hardware fault | I2C timeout, SPI NACK, USB disconnect | Retry (3×), then degrade mode. Notify user via audio. |
| Sensor fault | IMU not responding, GPS no fix | Degrade gracefully (use dead-reckoning without GPS). |
| AI inference fault | Model load failure, OOM during inference | Reload model, reduce resolution, reduce FPS. |
| Software fault | Null pointer, assert fail, stack overflow | Watchdog reset. Log error to flash. |
| Communication fault | ESP32 not responding, BLE disconnect | Retry connection, fall back to offline mode. |
| Memory fault | malloc fail, heap full | Free caches, reduce buffer sizes, trigger warning. |
| User input fault | Unrecognized voice command | "Sorry, I didn't understand that." Repeat available commands. |

### 22.2 Error Handling Code Pattern

```c
// Example: I2C IMU read with retry + degrade

imu_data_t imu_safe_read(void) {
    int retries = 3;
    imu_data_t data = {0};

    while (retries--) {
        int result = imu_driver_read(&data);
        if (result == 0) {
            return data;  // Success
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // All retries failed — publish error
    system_error_t err = {
        .module = MODULE_IMU,
        .code   = ERR_I2C_TIMEOUT,
        .severity = SEVERITY_HIGH,
        .message = "IMU communication failed"
    };
    message_bus_publish(MSG_SYSTEM_ERROR, &err, sizeof(err));

    // Degrade: return last known good data (from cache)
    return imu_last_good_data;
}
```

### 22.3 Assert Policy

```c
#define ASSERT(cond, msg) do {                          \
    if (!(cond)) {                                      \
        logger_log(LOG_CRITICAL, "ASSERT: %s", msg);    \
        logger_flush();                                 \
        __disable_irq();                                \
        while(1) {                                      \
            // Blink LED error pattern: SOS              \
            led_set(LED_RED, 1); delay(150);            \
            led_set(LED_RED, 0); delay(150);            \
            led_set(LED_RED, 1); delay(150);            \
            led_set(LED_RED, 0); delay(300);            \
            led_set(LED_RED, 1); delay(150);            \
            led_set(LED_RED, 0); delay(150);            \
            led_set(LED_RED, 1); delay(150);            \
            led_set(LED_RED, 0); delay(1000);           \
        }                                               \
    }                                                   \
} while(0)
```

Asserts are enabled in DEBUG builds only. Release builds use controlled error recovery instead of infinite loops.

---

## 23. Watchdog Mechanism

### 23.1 Watchdog Architecture

```
+-------------------------------------------------------------------+
|                     WATCHDOG ARCHITECTURE                          |
+-------------------------------------------------------------------+
|                                                                   |
|  +------------------+    +------------------+                     |
|  | Hardware Watchdog|    | Software Watchdog|                     |
|  | (ARM Cortex-M7   |    | (Task-level)     |                     |
|  |  internal IWDG)  |    |                   |                     |
|  | - Independent    |    | - 3 tasks must   |                     |
|  |   RC oscillator  |    |   check in within |                     |
|  | - 10 s timeout   |    |   10 s window     |                     |
|  | - Window mode    |    | - Tasks:          |                     |
|  |   disabled       |    |   - decision_task |                     |
|  +--------+---------+    |   - ai_task       |                     |
|           |              |   - safety_task   |                     |
|           |              +--------+---------+                     |
|           |                       |                               |
|           +-----------+-----------+                               |
|                       |                                           |
|                       v                                           |
|              +--------+-----------+                               |
|              | Watchdog Manager   |                               |
|              | - Refresh IWDG     |                               |
|              | - Check task       |                               |
|              |   heartbeats       |                               |
|              | - Force reset if   |                               |
|              |   any task missed  |                               |
|              +--------+-----------+                               |
|                       |                                           |
|                       v                                           |
|              +--------+-----------+                               |
|              | System Reset       |                               |
|              | (NVIC_SystemReset) |                               |
|              +-------------------+                               |
+-------------------------------------------------------------------+
```

### 23.2 Watchdog Configuration

```c
// watchdog.c

#define IWDG_TIMEOUT_MS    10000   // 10 seconds
#define TASK_CHECK_IN_MS   5000    // Heartbeat every 5 seconds
#define IWDG_PRESCALER     256     // LSI = 32 kHz / 256 = 125 Hz
#define IWDG_RELOAD_VALUE  1250    // 1250 ticks = 10 seconds

void watchdog_init(void) {
    // Enable independent watchdog
    IWDG->KR = 0x5555;           // Enable write access
    IWDG->PR = 3;                // Prescaler = 256
    IWDG->RLR = IWDG_RELOAD_VALUE; // 10 s timeout
    IWDG->KR = 0xCCCC;           // Start watchdog
    IWDG->KR = 0xAAAA;           // Refresh
}

void watchdog_refresh(void) {
    IWDG->KR = 0xAAAA;           // Reset counter
}

// Called by system_health_task every 5 seconds
void watchdog_check_task_heartbeats(void) {
    static uint32_t last_decision_beat = 0;
    static uint32_t last_ai_beat = 0;
    static uint32_t last_safety_beat = 0;

    uint32_t now = xTaskGetTickCount();

    // Check all critical tasks checked in within 10 seconds
    if ((now - last_decision_beat) > pdMS_TO_TICKS(10000)) {
        logger_log(LOG_CRITICAL, "WATCHDOG: decision_task missed heartbeat");
        NVIC_SystemReset();
    }
    if ((now - last_ai_beat) > pdMS_TO_TICKS(10000)) {
        logger_log(LOG_CRITICAL, "WATCHDOG: ai_task missed heartbeat");
        NVIC_SystemReset();
    }
    if ((now - last_safety_beat) > pdMS_TO_TICKS(10000)) {
        logger_log(LOG_CRITICAL, "WATCHDOG: safety_task missed heartbeat");
        NVIC_SystemReset();
    }

    // All healthy — refresh hardware watchdog
    watchdog_refresh();
}
```

### 23.3 Watchdog Behavior Summary

| Condition | Action | Recovery |
|---|---|---|
| IWDG timeout (10 s no refresh) | Hardware reset | Full system reboot |
| Task heartbeat miss (10 s window) | Software-initiated reset | Full system reboot |
| 3 consecutive task heartbeat misses | Immediate reset (no logging) | Full system reboot |
| Power-on after watchdog reset | Check reset cause register | Log "watchdog reset" event |
| Frequent watchdog resets (> 3 / hour) | Enter safe mode (minimal functionality) | Require caregiver intervention |

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior Software Architect | Initial draft |

---

*End of Document — SW-ARCH-001*
