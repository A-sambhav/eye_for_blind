# Navigation System Architecture Document

**Document ID:** NAV-ARCH-001

**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients

**Author:** Senior Navigation Engineer

**Revision:** 0.1

**Date:** 2026-07-27

---

## Table of Contents

1. Executive Summary
2. Navigation System Overview
3. Indoor Navigation
4. Outdoor Navigation
5. GPS Navigation
6. Obstacle Avoidance
7. Safe Path Planning
8. Walking Guidance
9. Turn-by-turn Guidance
10. Stair Detection
11. Door Detection
12. Road Crossing
13. Traffic Light Recognition
14. Algorithms
15. Flowcharts
16. State Diagrams
17. Sequence Diagrams
18. Limitations
19. Future SLAM Integration

---

## 1. Executive Summary

The navigation system fuses GPS, IMU, and AI-based visual perception to provide comprehensive pedestrian navigation. Outdoor navigation relies on GPS with IMU dead-reckoning bridging GPS-denied intervals. Indoor navigation uses IMU dead-reckoning augmented by landmark recognition from the AI layer. The system is designed to guide visually impaired users and prevent wandering in Alzheimer's patients.

---

## 2. Navigation System Overview

### 2.1 System Architecture

```
+---------------------------------------------------------------------+
|                     NAVIGATION SYSTEM                                |
+---------------------------------------------------------------------+
|                                                                     |
|  INPUTS                                                             |
|  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐ |
|  │ GPS     │  │ IMU     │  │ Depth   │  │ RGB    │  │ Decision│ |
|  │ Module  │  │ (9-axis)│  │ Camera  │  │ Camera │  │ Engine  │ |
|  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘ |
|       │             │             │             │             │    |
|       v             v             v             v             v    |
|  ┌─────────────────────────────────────────────────────────────┐  |
|  │                 NAVIGATION MANAGER                           │  |
|  │                                                             │  |
|  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │  |
|  │  │ GPS Navigator │  │ IMU Navigator │  │ Path Planner    │  │  |
|  │  │ - Position    │  │ - Orientation │  │ - Route from A  │  │  |
|  │  │ - Route calc  │  │ - Step detect │  │   to B          │  │  |
|  │  │ - Waypoints   │  │ - Dead recon  │  │ - Safe path     │  │  |
|  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────────┘  │  |
|  │         │                 │                   │              │  |
|  │         └────────┬────────┘───────────────────┘              │  |
|  │                  │                                           │  |
|  │  ┌───────────────┴──────────────────────────────────────┐   │  |
|  │  │         Sensor Fusion Engine (EKF)                   │   │  |
|  │  │  - 15-state Extended Kalman Filter                   │   │  |
|  │  │  - States: x, y, z, vx, vy, vz, roll, pitch, yaw,   │   │  |
|  │  │            gyro_bias(3), accel_bias(3)               │   │  |
|  │  └───────────────────────┬──────────────────────────────┘   │  |
|  │                          │                                   │  |
|  │  ┌───────────────────────┴──────────────────────────────┐   │  |
|  │  │         Obstacle Avoider                              │   │  |
|  │  │  - Collision detection from depth map                 │   │  |
|  │  │  - Path adjustment around obstacles                   │   │  |
|  │  │  - Stair/curb/door detection                          │   │  |
|  │  └───────────────────────┬──────────────────────────────┘   │  |
|  │                          │                                   │  |
|  │  OUTPUTS                 │                                   │  |
|  └──────────────────────────┼───────────────────────────────────┘  |
|                             |                                      |
|          ┌──────────────────┼──────────────────┐                  |
|          v                  v                  v                    |
|  ┌────────────────┐  ┌────────────┐  ┌──────────────┐            |
|  │ Direction Text │  │ Hazard     │  │ Position     │            |
|  │ →Decision Eng  │  │ Alert      │  │ Update       │            |
|  └────────────────┘  └────────────┘  │ →DB, Context │            |
|                                       └──────────────┘            |
+---------------------------------------------------------------------+
```

### 2.2 Navigation Manager API

```c
// nav_manager.h

typedef struct {
    double latitude;
    double longitude;
    float  altitude_m;
    float  accuracy_m;
    float  speed_mps;
    float  heading_deg;
} nav_position_t;

typedef struct {
    nav_position_t waypoints[128];
    uint8_t        waypoint_count;
    uint8_t        current_waypoint_index;
    float          total_distance_m;
    float          remaining_distance_m;
    char           destination_name[64];
} nav_route_t;

typedef enum {
    NAV_OFF,
    NAV_CALCULATING_ROUTE,
    NAV_ACTIVE,
    NAV_PAUSED,
    NAV_ARRIVED,
    NAV_ERROR
} nav_state_t;

// Core API
int  nav_init(void);
int  nav_start_route(const char* destination, double lat, double lon);
int  nav_start_route_to_home(void);
int  nav_cancel(void);
int  nav_pause(void);
int  nav_resume(void);

nav_state_t    nav_get_state(void);
nav_position_t nav_get_current_position(void);
nav_route_t*   nav_get_current_route(void);
uint8_t        nav_get_next_turn(nav_turn_t* turn);
float          nav_get_distance_to_next_turn(void);

// Callbacks
void nav_register_turn_callback(void (*cb)(nav_turn_t* turn));
void nav_register_arrival_callback(void (*cb)(void));
void nav_register_deviation_callback(void (*cb)(float deviation_m));
```

---

## 3. Indoor Navigation

### 3.1 Architecture

```
+---------------------------------------------------------------------+
|                      INDOOR NAVIGATION                               |
+---------------------------------------------------------------------+
|                                                                     |
|  No GPS available. Navigation relies on:                           |
|    1. IMU dead-reckoning (PDR — Pedestrian Dead Reckoning)          |
|    2. Landmark recognition (AI: doors, stairs, signs)               |
|    3. Pre-mapped building layouts (future)                          |
|                                                                     |
|  ┌────────────────────────────────────────────────────────────────┐ |
|  |                   PDR ALGORITHM                                | |
|  |  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌────────────────┐   | |
|  |  │ Step    │  │ Step    │  │ Heading │  │ Position       │   | |
|  |  │ Detect  │  │ Length  │  │ Estimate│  │ Update         │   | |
|  |  │ (peak   │  │ (height-│  │ (IMU    │  │ x += step_len  │   | |
|  |  │ detect) │  │ dependent│  │ yaw)   │  │   * sin(head)  │   | |
|  |  └─────────┘  └─────────┘  └─────────┘  │ y += step_len  │   | |
|  |                                           │   * cos(head)  │   | |
|  |                                           └────────────────┘   | |
|  └────────────────────────────────────────────────────────────────┘ |
|                                                                     |
|  Correction sources (reset drift):                                  |
|  ┌──────────────┐    ┌──────────────┐    ┌──────────────────────┐  |
|  │ Door detected │    │ Stair cases  │    │ Known landmark      │  |
|  │ →Reset pos   │    │ →Floor level │    │ →Reset position     │  |
|  │ to doorway   │    │   detection  │    │   to mapped loc     │  |
|  └──────────────┘    └──────────────┘    └──────────────────────┘  |
|                                                                     |
+---------------------------------------------------------------------+
```

### 3.2 PDR Algorithm

```c
// imu_navigator.c — Pedestrian Dead Reckoning

#define STEP_DETECTION_THRESHOLD  2.0f   // m/s² peak acceleration
#define STEP_MIN_INTERVAL_MS       200    // Minimum time between steps
#define STEP_MAX_INTERVAL_MS       2000   // Maximum time between steps

static float user_height_m = 1.7f;  // Configurable

typedef struct {
    float x_m;       // Relative X position (meters)
    float y_m;       // Relative Y position (meters)
    float z_m;       // Relative Z position (floor level * 3m)
    float heading_deg;
    uint32_t step_count;
    uint32_t last_step_ms;
} pdr_state_t;

static pdr_state_t pdr;

float estimate_step_length(float height_m, float frequency_hz) {
    // Weinberg formula: step_length = height * sqrt(accel_range / g)
    // Simplified: 0.4-0.8 m depending on height and cadence
    float base = height_m * 0.415f;
    float cadence_factor = fmin(frequency_hz / 2.0f, 1.0f);
    return base * (0.7f + 0.3f * cadence_factor);
}

bool detect_step(imu_data_t* imu) {
    uint32_t now = xTaskGetTickCount();
    if (now - pdr.last_step_ms < STEP_MIN_INTERVAL_MS) {
        return false;
    }

    float acc_mag = sqrt(imu->accel_x*imu->accel_x +
                          imu->accel_y*imu->accel_y +
                          imu->accel_z*imu->accel_z);

    // Peak detection with hysteresis
    static float prev_acc = 0;
    static bool peak_mode = false;

    if (acc_mag > STEP_DETECTION_THRESHOLD && !peak_mode) {
        peak_mode = true;
    } else if (acc_mag < STEP_DETECTION_THRESHOLD - 0.5f && peak_mode) {
        // Step detected (acceleration cycle complete)
        peak_mode = false;
        pdr.last_step_ms = now;
        pdr.step_count++;

        // Update position
        float step_len = estimate_step_length(user_height_m,
                                              1000.0f / (now - pdr.last_step_ms));
        float heading_rad = pdr.heading_deg * M_PI / 180.0f;

        pdr.x_m += step_len * sinf(heading_rad);
        pdr.y_m += step_len * cosf(heading_rad);

        return true;
    }

    prev_acc = acc_mag;
    return false;
}
```

### 3.3 Drift Error Budget

| Error Source | Drift Rate | After 1 min | After 5 min | After 10 min |
|---|---|---|---|---|
| IMU heading drift | 1°/s (consumer grade) | 60° | 300° | — |
| With gyro bias correction | 0.1°/s | 6° | 30° | 60° |
| Step length estimation | ±10% per step | ±5 m | ±25 m | ±50 m |
| With landmark correction | Reset on detection | Reset | Reset | Reset |

**Conclusion:** Indoor navigation is reliable for 30-60 seconds between landmark corrections. Beyond that, position error exceeds 10 meters. **Landmark detection is essential for useful indoor navigation.**

---

## 4. Outdoor Navigation

### 4.1 GPS-IMU Fusion

```
+---------------------------------------------------------------------+
|                     OUTDOOR NAVIGATION                               |
+---------------------------------------------------------------------+
|                                                                     |
|  GPS (10 Hz update)              IMU (1 kHz update)                |
|  ┌─────────────────┐            ┌─────────────────┐               |
|  │ Position: 1.5 m │            │ Heading: 1°/s   │               |
|  │ CEP accuracy    │            │ drift (uncorrected)│             |
|  │ Speed: 0.05 m/s │            │ Accel: 0.1 m/s² │               |
|  │ Heading: 3° RMS │            │                  │               |
|  └────────┬────────┘            └────────┬────────┘               |
|           │                              │                        |
|           v                              v                        |
|  ┌──────────────────────────────────────────────────────────┐     |
|  │              EXTENDED KALMAN FILTER (15-state)            │     |
|  │                                                          │     |
|  │  States:                                                  │     |
|  │    x, y, z        → Position (m)                        │     |
|  │    vx, vy, vz     → Velocity (m/s)                      │     |
|  │    roll, pitch, yaw → Attitude (rad)                    │     |
|  │    bgx, bgy, bgz  → Gyro bias (rad/s)                   │     |
|  │    bax, bay, baz  → Accel bias (m/s²)                  │     |
|  │                                                          │     |
|  │  GPS UPDATE (10 Hz):                                     │     |
|  │    z = [lat, lon, alt, speed, heading]                   │     |
|  │    R = [2.5m, 2.5m, 5m, 0.5m/s, 5°]                   │     |
|  │                                                          │     |
|  │  IMU PREDICTION (1 kHz):                                 │     |
|  │    z = [accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z]│   |
|  │    Q = process noise matrix                              │     |
|  │                                                          │     |
|  └────────────────────────┬─────────────────────────────────┘     |
|                           │                                       |
|                           v                                       |
|  ┌──────────────────────────────────────────────────────────────┐ |
|  │              FUSED OUTPUT (100 Hz)                          │ |
|  │  Position: ±1.5 m CEP (GPS), ±3 m/min (GPS-denied)        │ |
|  │  Heading: ±2° RMS                                          │ |
|  │  Velocity: ±0.1 m/s                                        │ |
|  └──────────────────────────────────────────────────────────────┘ |
+---------------------------------------------------------------------+
```

### 4.2 GPS Navigation States

```
                    ┌──────────┐
                    │ NO_FIX   │
                    └────┬─────┘
                         │
                         v
                    ┌──────────┐
           ┌────────│ 2D_FIX   │
           │        └────┬─────┘
           │             │
           │             v
           │        ┌──────────┐
           │ ┌──────│ 3D_FIX   │
           │ │      └────┬─────┘
           │ │           │
           │ │           v
           │ │      ┌──────────┐
           │ │      │ DGPS_FIX │
           │ │      └────┬─────┘
           │ │           │
           │ │           │ GPS lost
           │ │           v
           │ │      ┌──────────┐
           │ │      │ IMU_ONLY │ (hold last known position, drift)
           │ │      └────┬─────┘
           │ │           │ GPS regained
           │ │           v
           │ │      ┌──────────┐
           │ └──────│ REACQUIRE│
           │        └──────────┘
           │
           │ (Short tunnel < 30s → IMU dead-reckoning)
           │ (Long tunnel > 30s → open loop, warn user)
```

---

## 5. GPS Navigation

### 5.1 Route Calculation

The system does not perform on-device route calculation. Routes are:

1. **Pre-calculated** by a companion app (phone) and transferred via BLE
2. **Hardcoded** for frequent routes: home → hospital, home → pharmacy
3. **Simple waypoint navigation**: direct bearing to destination

```c
// gps_navigator.c

typedef struct {
    double lat;
    double lon;
    char   name[32];
} waypoint_t;

typedef struct {
    waypoint_t waypoints[128];
    uint8_t    count;
    uint8_t    current;
} route_t;

// Haversine distance
float haversine_distance(double lat1, double lon1, double lat2, double lon2) {
    double R = 6371000;  // Earth radius in meters
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * c;
}

// Bearing from current position to waypoint
float bearing_to_waypoint(double current_lat, double current_lon,
                           double target_lat, double target_lon) {
    double dlon = (target_lon - current_lon) * M_PI / 180.0;
    double lat1 = current_lat * M_PI / 180.0;
    double lat2 = target_lat * M_PI / 180.0;

    double y = sin(dlon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
    double brng = atan2(y, x) * 180.0 / M_PI;

    return fmod(brng + 360.0, 360.0);
}

// Update navigation progress
void nav_update_progress(nav_position_t* current_pos) {
    route_t* route = nav_get_current_route();
    if (!route || route->count == 0) return;

    waypoint_t* target = &route->waypoints[route->current];

    float dist = haversine_distance(
        current_pos->latitude, current_pos->longitude,
        target->lat, target->lon);

    route->remaining_distance_m = dist;

    // Check if arrived at waypoint (within 10 m)
    if (dist < 10.0f) {
        route->current++;
        if (route->current >= route->count) {
            // Arrived at final destination
            nav_set_state(NAV_ARRIVED);
            decision_engine_trigger(EVENT_ARRIVED);
        } else {
            // Approaching next waypoint
            decision_engine_trigger(EVENT_APPROACHING_WAYPOINT);
        }
    }

    // Check if off-route (> 50 m from path)
    float cross_track = calculate_cross_track_error(current_pos, route);
    if (cross_track > 50.0f) {
        decision_engine_trigger(EVENT_OFF_ROUTE);
    }
}

// Turn detection
nav_turn_t detect_upcoming_turn(nav_position_t* pos, route_t* route) {
    nav_turn_t turn = {0};

    if (route->current >= route->count - 1) {
        turn.type = TURN_NONE;
        return turn;
    }

    waypoint_t* current = &route->waypoints[route->current];
    waypoint_t* next = &route->waypoints[route->current + 1];

    float bearing_current = bearing_to_waypoint(
        pos->latitude, pos->longitude,
        current->lat, current->lon);
    float bearing_next = bearing_to_waypoint(
        current->lat, current->lon,
        next->lat, next->lon);

    float turn_angle = fmod(bearing_next - bearing_current + 540, 360) - 180;

    if (fabs(turn_angle) < 20) {
        turn.type = TURN_STRAIGHT;
        turn.angle_deg = 0;
    } else if (turn_angle > 0 && turn_angle < 100) {
        turn.type = TURN_LEFT;
        turn.angle_deg = turn_angle;
    } else if (turn_angle < 0 && turn_angle > -100) {
        turn.type = TURN_RIGHT;
        turn.angle_deg = -turn_angle;
    } else {
        turn.type = TURN_UTURN;
        turn.angle_deg = 180;
    }

    turn.distance_m = haversine_distance(
        pos->latitude, pos->longitude,
        current->lat, current->lon);
    turn.waypoint_name[0] = '\0';

    return turn;
}
```

---

## 6. Obstacle Avoidance

### 6.1 Obstacle Detection and Response

```
+---------------------------------------------------------------------+
|                     OBSTACLE AVOIDANCE                               |
+---------------------------------------------------------------------+
|                                                                     |
|  Depth Map (640×360)                                               |
|       │                                                             |
|       v                                                             |
|  ┌─────────────────────────────────────────────────────────────────┐|
|  |                  OBSTACLE AVOIDER                               ||
|  |                                                               ||
|  |  ┌─────────────────────┐                                       ||
|  |  │ Divide FOV into     │  Left zone: -45° to -15°             ||
|  |  │ 6 zones             │  Mid-left: -15° to -5°               ||
|  |  │                     │  Center: -5° to +5° (path ahead)     ||
|  |  │          1 2 3 4 5 6│  Mid-right: +5° to +15°             ||
|  |  │     Left ←———C———→ Right  Right zone: +15° to +45°          ||
|  |  └─────────────────────┘                                       ||
|  |                                                               ||
|  |  FOR EACH zone:                                                ||
|  |    min_distance = min(depth_map[zone])                        ||
|  |    IF min_distance < DANGER_THRESHOLD (0.5m):                 ||
|  |      → EMERGENCY STOP — alert immediately                     ||
|  |    IF min_distance < WARNING_THRESHOLD (2.0m):                ||
|  |      → Guide user away: "Obstacle on your left. Step right."  ||
|  |    IF min_distance < INFO_THRESHOLD (5.0m):                   ||
|  |      → "Obstacle ahead, 3 meters on your left"                ||
|  |                                                               ||
|  └─────────────────────────────────────────────────────────────────┘|
|                                                                     |
|  Obstacle avoidance guidance message format:                       |
|    "{Direction}: {object}, {distance} meters"                     |
|    "Move {direction} to avoid"                                    |
|                                                                     |
|  Examples:                                                         |
|    "Center: Person, 1.2 meters. Stop."                            |
|    "Left: Table, 0.8 meters. Move right."                         |
|    "Right: Wall, 0.5 meters. Move left."                          |
|                                                                     |
+---------------------------------------------------------------------+
```

### 6.2 Obstacle Avoidance Algorithm

```c
// obstacle_avoider.c

#define FOV_HORIZONTAL_DEG 70.0f
#define NUM_ZONES 6

typedef enum {
    ZONE_LEFT_EXTREME,   // -35° to -20°
    ZONE_LEFT,            // -20° to -7°
    ZONE_CENTER_LEFT,     // -7° to -2°
    ZONE_CENTER_RIGHT,    // +2° to +7°
    ZONE_RIGHT,           // +7° to +20°
    ZONE_RIGHT_EXTREME    // +20° to +35°
} fov_zone_t;

typedef struct {
    fov_zone_t  zone;
    float       min_distance_m;
    int         object_class;   // AI-detected class
    bool        has_object;
} zone_info_t;

void analyze_obstacle_zones(uint16_t* depth_map, int width, int height,
                             detection_t* objects, int num_objects,
                             zone_info_t zones[6]) {
    // Clear zones
    for (int i = 0; i < NUM_ZONES; i++) {
        zones[i].min_distance_m = INFINITY;
        zones[i].has_object = false;
    }

    // Check depth map for each zone
    int zone_width = width / NUM_ZONES;
    for (int zone = 0; zone < NUM_ZONES; zone++) {
        int start_col = zone * zone_width;
        int end_col = start_col + zone_width;
        int start_row = height * 0.3;     // Skip sky
        int end_row = height * 0.9;       // Skip ground

        float min_dist = INFINITY;
        for (int r = start_row; r < end_row; r++) {
            for (int c = start_col; c < end_col; c++) {
                uint16_t d = depth_map[r * width + c];
                if (d > 0 && d < min_dist) {
                    min_dist = d;
                }
            }
        }

        zones[zone].zone = (fov_zone_t)zone;
        zones[zone].min_distance_m = min_dist / 1000.0f;

        // Check if AI detection overlaps this zone
        for (int o = 0; o < num_objects; o++) {
            int obj_center_x = (objects[o].x + objects[o].w / 2);
            if (obj_center_x >= start_col && obj_center_x < end_col) {
                zones[zone].has_object = true;
                zones[zone].object_class = objects[o].class_id;
            }
        }
    }
}

// Determine best direction of travel (clearest path)
fov_zone_t find_clearest_path(zone_info_t zones[6]) {
    float best_distance = 0;
    fov_zone_t best_zone = ZONE_CENTER_LEFT;

    // Prefer forward (center zones); fall back to sides
    fov_zone_t check_order[] = {
        ZONE_CENTER_LEFT, ZONE_CENTER_RIGHT,
        ZONE_LEFT, ZONE_RIGHT,
        ZONE_LEFT_EXTREME, ZONE_RIGHT_EXTREME
    };

    for (int i = 0; i < 6; i++) {
        fov_zone_t z = check_order[i];
        if (zones[z].min_distance_m > best_distance) {
            best_distance = zones[z].min_distance_m;
            best_zone = z;
        }
    }

    return best_zone;
}
```

---

## 7. Safe Path Planning

### 7.1 Path Planning Strategy

The system does not perform global path planning on-device (computationally infeasible). Instead:

1. **Global route** is provided by companion app or pre-loaded
2. **Local path adjustment** uses obstacle avoidance to navigate around hazards
3. **Safe corridor** is maintained along the route

```
┌─────────────────────────────────────────────────────────────┐
│                    SAFE CORRIDOR CONCEPT                      │
│                                                             │
│                    ┌──────────────────┐                      │
│                    │                  │                      │
│                    │   SAFE PATH      │                      │
│                    │   (2m wide       │                      │
│       ────         │    corridor)     │        ────          │
│       │           │                  │           │          │
│       │   ┌───────┘                  └───────┐   │          │
│       │   │                                  │   │          │
│       │   │   !OBSTACLE!         CLEAR       │   │          │
│       │   │   [Table]           [Continue]   │   │          │
│       │   └───────┐                  ┌───────┘   │          │
│       ────        │     USER →       │        ────          │
│                    │   ●              │                      │
│                    │     ← adjust     │                      │
│                    │       right       │                      │
│                    └──────────────────┘                      │
│                                                             │
│   Audio: "Table on your left. Move slightly right to pass." │
└─────────────────────────────────────────────────────────────┘
```

---

## 8. Walking Guidance

### 8.1 Guidance Messages

| Situation | Distance | Message |
|---|---|---|
| Route started | — | "Navigating to {destination}. Total distance: {dist}." |
| Continue straight | — | "Continue straight for {distance} meters." (every 100 m) |
| Turn approaching | 50 m | "In 50 meters, turn {direction}." |
| Turn approaching | 20 m | "In 20 meters, turn {direction}." |
| Turn now | 5 m | "Turn {direction} now." |
| Turn confirmed | 10 m past | "You have turned {direction}. Continue straight." |
| Obstacle on path | < 3 m | "{Object} ahead. {Action} to avoid." |
| Off-route | > 50 m | "You have left the route. Recalculating." |
| Re-route found | — | "New route found. Continue straight." |
| Arrival | < 20 m | "You are approaching {destination} on your {side}." |
| Arrived | 0 m | "You have arrived at {destination}." |
| Stairs detected | < 2 m | "Stairs ahead. {Up/Down}. {n} steps." |
| Road crossing | < 10 m | "Crosswalk ahead. Stop and check for traffic." |

### 8.2 Guidance Cadence

```c
// nav_app.c — Cadence control

#define GUIDANCE_INTERVAL_NO_INTURRUPTION_MS  5000   // Don't speak more often than 5s
#define GUIDANCE_INTERVAL_SAME_DIRECTION_MS   30000  // Same "continue straight" max 30s
#define TURN_PRE_WARNING_DISTANCE_1            50     // First warning at 50m
#define TURN_PRE_WARNING_DISTANCE_2            20     // Second warning at 20m
#define TURN_EXECUTION_DISTANCE                5      // "Turn now" at 5m

static uint32_t last_guidance_ms = 0;
static uint32_t last_straight_ms = 0;

bool should_provide_guidance(nav_state_t* nav, context_t* ctx) {
    uint32_t now = xTaskGetTickCount();

    // Don't interrupt user while speaking
    if (voice_is_currently_speaking()) return false;

    // Rate limiting
    if (now - last_guidance_ms < GUIDANCE_INTERVAL_NO_INTURRUPTION_MS)
        return false;

    // Check for turn alerts (always high priority)
    nav_turn_t turn;
    float dist = nav_get_distance_to_next_turn();
    uint8_t next_turn = nav_get_next_turn(&turn);

    if (next_turn > 0 && dist < TURN_PRE_WARNING_DISTANCE_1) {
        return true;
    }

    // Regular straight guidance
    if (now - last_straight_ms >= GUIDANCE_INTERVAL_SAME_DIRECTION_MS) {
        last_straight_ms = now;
        return true;
    }

    return false;
}
```

---

## 9. Turn-by-turn Guidance

### 9.1 Sequence Diagram

```
User          Smart Glasses          GPS             Decision Engine    Voice
 │                  │                 │                    │              │
 │ "Navigate to     │                 │                    │              │
 │  Central Park"   │                 │                    │              │
 ├─────────────────▶│                 │                    │              │
 │                  │ Request route   │                    │              │
 │                  │ from (phone)    │                    │              │
 │                  ├───── BLE ───────▶(companion app)     │              │
 │                  │◀─── route ──────┤                    │              │
 │                  │                 │                    │              │
 │                  │ Start GPS       │ GPS fix            │              │
 │                  │ ├──────────────▶│◀──── position ────┤              │
 │                  │                 │                    │              │
 │                  │ Calc first turn │                    │              │
 │                  │ ├──────────────────────────────────▶│              │
 │                  │                 │                    │ "Navigating  │
 │                  │                 │                    │  to Central  │
 │                  │                 │                    │  Park. 1.2   │
 │                  │                 │                    │  kilometers" │
 │                  │                 │                    ├──── TTS ────▶│
 │ ◀══ "Navigating ═║══ to Central ══║══ Park..." ═══════║═══════════╝ │
 │  to Central Park"│                 │                    │              │
 │  ░░░░░░░░░░░░░░░░│░░░░░░░░░░░░░░░░│░░░░░░░░░░░░░░░░░░░│░░░░░░░       │
 │                  │                 │                    │              │
 │  50 m from turn  │                 │                    │              │
 │                  │ ├──────────────▶│ "In 50 meters,    │              │
 │                  │                 │  turn right"      ├──── TTS ────▶│
 │ ◀══ "In 50 ══════║══ meters, ═════║══ turn right" ════║═══════════╝ │
 │                  │                 │                    │              │
 │  20 m from turn  │                 │                    │              │
 │                  │ ├──────────────▶│ "In 20 meters,    │              │
 │                  │                 │  turn right"      ├──── TTS ────▶│
 │ ◀══ "In 20 ══════║══ meters, ═════║══ turn right" ════║═══════════╝ │
 │                  │                 │                    │              │
 │  5 m from turn   │                 │                    │              │
 │                  │ ├──────────────▶│ "Turn right now"  ├──── TTS ────▶│
 │ ◀══ "Turn right ══════════════════║════════════════════║═══════════╝ │
 │  now"            │                 │                    │              │
 │                  │                 │                    │              │
 │   User turns     │                 │                    │              │
 │   right          │ IMU: heading    │                    │              │
 │                  │ change detected │                    │              │
 │                  │ ├──────────────▶│ "Continue straight │              │
 │                  │                 │  on {street}"     ├──── TTS ────▶│
 │ ◀══ "Continue ═══║══ straight on ══║══ 5th Avenue" ═══║═══════════╝ │
```

---

## 10. Stair Detection

### 10.1 Algorithm

```c
// obstacle_avoider.c — Stair detection

typedef enum {
    STAIR_NONE,
    STAIRS_UP,
    STAIRS_DOWN,
    STAIRS_UNKNOWN
} stair_type_t;

typedef struct {
    stair_type_t type;
    float        distance_m;
    int          estimated_steps;
    float        width_m;
} stair_detection_t;

stair_detection_t detect_stairs(uint16_t* depth_map, int width, int height) {
    stair_detection_t result = {0};

    // Analyze bottom-center region of depth map (ground ahead)
    int center_col_start = width * 0.3;
    int center_col_end = width * 0.7;
    int scan_row_start = height * 0.5;
    int scan_row_end = height * 0.85;

    // Accumulate depth profiles for each column in center region
    float prev_depth = 0;
    int edge_count = 0;
    float edge_distances[10];
    int edge_directions[10];  // +1 = up, -1 = down

    for (int c = center_col_start; c < center_col_end; c++) {
        for (int r = scan_row_start; r < scan_row_end; r++) {
            uint16_t d = depth_map[r * width + c];
            float depth_m = d / 1000.0f;

            if (prev_depth > 0 && d > 0) {
                float delta = depth_m - prev_depth;

                // Positive delta = drop-off (stairs down)
                // Negative delta = step-up (stairs up)
                if (delta > 0.1f && delta < 0.5f) {
                    // Step down detected
                    if (edge_count < 10) {
                        edge_distances[edge_count] = depth_m;
                        edge_directions[edge_count] = -1;
                        edge_count++;
                    }
                    break;  // One edge per column
                } else if (delta < -0.1f && delta > -0.5f) {
                    // Step up detected
                    if (edge_count < 10) {
                        edge_distances[edge_count] = depth_m;
                        edge_directions[edge_count] = +1;
                        edge_count++;
                    }
                    break;
                }
            }
            prev_depth = depth_m;
        }
        prev_depth = 0;
    }

    if (edge_count == 0) {
        return result;  // No stairs
    }

    // Average edge distance
    float sum_dist = 0;
    int up_count = 0, down_count = 0;
    for (int i = 0; i < edge_count; i++) {
        sum_dist += edge_distances[i];
        if (edge_directions[i] > 0) up_count++;
        else down_count++;
    }

    result.distance_m = sum_dist / edge_count;
    result.type = (up_count > down_count) ? STAIRS_UP : STAIRS_DOWN;
    result.estimated_steps = edge_count;  // Rough
    result.width_m = (center_col_end - center_col_start) * 0.002f;  // Rough width

    return result;
}
```

### 10.2 Stair Guidance Messages

| Situation | Message |
|---|---|
| Stairs up detected, 3m | "Stairs going up ahead, 3 meters." |
| Stairs up, at base | "Stairs up. {N} steps. Use handrail if available." |
| Stairs down detected, 2m | "Stairs going down ahead, 2 meters. Caution." |
| Stairs down, at edge | "Stairs down. {N} steps. Step carefully." |
| Approaching escalator | "Escalator ahead. Step on carefully." |

---

## 11. Door Detection

### 11.1 Algorithm

```c
typedef struct {
    bool   detected;
    float  distance_m;
    float  width_m;
    bool   is_open;
    char   label[32];  // "Entrance", "Exit", "Room 204"
} door_detection_t;

door_detection_t detect_door(uint16_t* depth_map, detection_t* objects,
                              int num_objects) {
    door_detection_t result = {0};

    // Check for door-shaped objects from AI
    for (int i = 0; i < num_objects; i++) {
        if (objects[i].class_id == CLASS_DOOR ||
            objects[i].class_id == CLASS_ENTRANCE) {

            // Verify with depth: door = vertical gap in wall
            // Check depth discontinuity at door edges
            int door_center = objects[i].x + objects[i].w / 2;
            int door_y = objects[i].y + objects[i].h / 2;

            uint16_t left_depth = depth_map[door_y * 640 + objects[i].x];
            uint16_t right_depth = depth_map[door_y * 640 + objects[i].x + objects[i].w];

            if (left_depth > 0 && right_depth > 0) {
                float left_m = left_depth / 1000.0f;
                float right_m = right_depth / 1000.0f;

                result.detected = true;
                result.distance_m = (left_m + right_m) / 2.0f;
                result.width_m = objects[i].w * 0.002f;  // Rough pixel-to-meter
                result.is_open = (fabs(left_m - right_m) > 0.5f);

                // Default label
                snprintf(result.label, sizeof(result.label), "Door");
                return result;
            }
        }
    }

    return result;
}
```

---

## 12. Road Crossing

### 12.1 State Machine

```
                    ┌──────────────────┐
                    │    IDLE          │
                    │ (no crosswalk)   │
                    └────────┬─────────┘
                             │ Scene: crosswalk detected
                             │ GPS: near intersection
                             v
                    ┌──────────────────┐
                    │ APPROACHING      │
                    │ "Crosswalk ahead,│
                    │  10 meters"      │
                    └────────┬─────────┘
                             │ Distance < 2 m
                             v
                    ┌──────────────────┐
                    │ AT_CROSSWALK     │
                    │ "Stop. Check    │
                    │  for traffic."   │─── "Is it clear?" ──▶ TRAFFIC_CHECK
                    └────────┬─────────┘
                             │ Vehicle detected?
                             │
                    ┌────────┴─────────┐
                    │ YES              │ NO
                    v                  v
              ┌────────────┐   ┌──────────────┐
              │ VEHICLE    │   │ CLEAR        │
              │ APPROACHING│   │ "Road is     │
              │ "Vehicle   │   │  clear.      │
              │  from left"│   │  Cross now." │
              └────────────┘   └──────┬───────┘
                                     │ User starts crossing
                                     v
                            ┌──────────────────┐
                            │ CROSSING         │
                            │ Monitor traffic  │
                            │ continuously     │
                            └────────┬─────────┘
                                     │ Vehicle detected mid-crossing
                                     │ OR reached other side
                            ┌────────┴─────────┐
                            │ Vehicle          │ Reached other side
                            v                  v
                      ┌────────────┐   ┌──────────────┐
                      │ HURRY      │   │ COMPLETE     │
                      │ "Vehicle   │   │ "Crossing    │
                      │  approach  │   │  complete.   │
                      │  ing.      │   │  Continue    │
                      │  Hurry!"   │   │  straight."  │
                      └────────────┘   └──────┬───────┘
                                              │
                                              v
                                        ┌──────────┐
                                        │  IDLE    │
                                        └──────────┘
```

### 12.2 Traffic Light Detection (Future)

```c
// traffic_light.c — Future enhancement

typedef enum {
    TL_UNKNOWN,
    TL_RED,
    TL_GREEN,
    TL_YELLOW,
    TL_NO_LIGHT
} traffic_light_state_t;

typedef struct {
    bool                 detected;
    traffic_light_state_t state;
    float                distance_m;
    float                height_m;  // Height above ground
} traffic_light_detection_t;

// Placeholder — requires dedicated model or color-based detection
traffic_light_detection_t detect_traffic_light(uint8_t* rgb_frame,
                                                int width, int height) {
    // Future implementation:
    // 1. Detect circular red/green/yellow objects in upper frame region
    // 2. Classify color using histogram in bounding box
    // 3. Determine state based on illuminated circle
    //
    // Not implemented in v1.0 — requires additional Edge Impulse model
    // or color blob detection.

    traffic_light_detection_t result = {0};
    // ... future implementation ...
    return result;
}
```

---

## 13. Algorithms

### 13.1 Extended Kalman Filter (15-state)

```c
// ekf.c — Simplified EKF for GPS-IMU fusion

#define NUM_STATES 15

typedef struct {
    float x[NUM_STATES];   // State vector
    float P[NUM_STATES][NUM_STATES];  // Covariance matrix
} ekf_t;

// State indices
enum {
    IDX_X, IDX_Y, IDX_Z,           // Position (m)
    IDX_VX, IDX_VY, IDX_VZ,        // Velocity (m/s)
    IDX_ROLL, IDX_PITCH, IDX_YAW,  // Attitude (rad)
    IDX_BGX, IDX_BGY, IDX_BGZ,     // Gyro bias (rad/s)
    IDX_BAX, IDX_BAY, IDX_BAZ      // Accel bias (m/s²)
};

void ekf_predict(ekf_t* ekf, imu_data_t* imu, float dt) {
    // State prediction using IMU measurements
    // x = x + vx*dt + 0.5*ax*dt²  (simplified)
    ekf->x[IDX_X] += ekf->x[IDX_VX] * dt +
        0.5f * (imu->accel_x - ekf->x[IDX_BAX]) * dt * dt;
    // ... similar for Y, Z ...

    // Velocity update
    ekf->x[IDX_VX] += (imu->accel_x - ekf->x[IDX_BAX]) * dt;
    // ... similar for VY, VZ ...

    // Attitude from gyroscope
    ekf->x[IDX_ROLL] += (imu->gyro_x - ekf->x[IDX_BGX]) * dt;
    ekf->x[IDX_PITCH] += (imu->gyro_y - ekf->x[IDX_BGY]) * dt;
    ekf->x[IDX_YAW] += (imu->gyro_z - ekf->x[IDX_BGZ]) * dt;

    // Covariance prediction: P = F*P*F' + Q
    // (Simplified — full Jacobian computation omitted for brevity)
    // In practice, Edge Impulse or CMSIS-DSP handles matrix operations.
}

void ekf_update_gps(ekf_t* ekf, gps_position_t* gps) {
    // GPS measurement update (10 Hz)
    // Measurement: z = [lat, lon, alt, speed, heading]
    // Convert GPS lat/lon to local ENU coordinates

    // Innovation: y = z - H*x
    // Kalman gain: K = P*H'*(H*P*H' + R)⁻¹
    // State update: x = x + K*y
    // Covariance: P = (I - K*H)*P

    // (Full implementation requires matrix library — using CMSIS-DSP)
}
```

### 13.2 Step Detection Algorithm

```c
// Step detection using IMU acceleration

bool detect_walking_step(imu_data_t* imu) {
    static float acc_buffer[50];  // 50 samples @ 50 Hz = 1 second
    static uint8_t idx = 0;
    static bool above_threshold = false;

    float acc_mag = sqrt(imu->accel_x * imu->accel_x +
                          imu->accel_y * imu->accel_y +
                          imu->accel_z * imu->accel_z);
    acc_buffer[idx % 50] = acc_mag;

    // Compute moving average
    float sum = 0;
    for (int i = 0; i < 50; i++) sum += acc_buffer[i];
    float mean = sum / 50.0f;

    // Detect step: acceleration crosses threshold with minimum interval
    static uint32_t last_step_ms = 0;
    uint32_t now = xTaskGetTickCount();

    if (acc_mag > mean + 1.5f && !above_threshold &&
        (now - last_step_ms > 200)) {
        above_threshold = true;
        last_step_ms = now;
        return true;
    } else if (acc_mag < mean + 0.5f) {
        above_threshold = false;
    }

    idx++;
    return false;
}
```

---

## 14. Flowcharts

### 14.1 Navigation Update Loop

```
                    ┌──────────────────────┐
                    │   NAV TASK          │
                    │   (runs every 100ms) │
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
                    │ Read GPS position    │
                    │ (if available)       │
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
                    │ Read IMU (accel, gyro)│
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
                    │ EKF prediction       │
                    │ (IMU propagation)    │
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
               ┌───│ GPS update available? │───No───┐
               │   └───────────┬──────────┘        │
               │               │ Yes                │
               │               v                    │
               │   ┌──────────────────────┐        │
               │   │ EKF GPS update       │        │
               │   │ (correct position    │        │
               │   │  and velocity)       │        │
               │   └───────────┬──────────┘        │
               │               │                    │
               └───────────────┘                    │
                               │                    │
                               v                    │
                    ┌──────────────────────┐        │
                    │ Step detection       │        │
                    │ (from IMU)           │        │
                    └───────────┬──────────┘        │
                               │                    │
                               v                    │
                    ┌──────────────────────┐        │
                    │ Update PDR position  │        │
                    │ (if GPS lost)        │        │
                    └───────────┬──────────┘        │
                               │                    │
                               v                    │
                    ┌──────────────────────┐        │
                    │ Check route progress │        │
                    │ (waypoint distance)  │        │
                    └───────────┬──────────┘        │
                               │                    │
                               v                    │
                    ┌──────────────────────┐        │
                    │ Detect upcoming turn │        │
                    │ (bearing difference) │        │
                    └───────────┬──────────┘        │
                               │                    │
                               v                    │
                    ┌──────────────────────┐        │
                    │ Publish position     │        │
                    │ to message bus       │        │
                    ├──────────────────────┤        │
                    │ Publish turn alert   │        │
                    │ (if turn < 50m)     │        │
                    └───────────┬──────────┘        │
                               │                    │
                               v                    │
                    ┌──────────────────────┐        │
                    │ Return to loop start │◀───────┘
                    └──────────────────────┘
```

---

## 15. State Diagrams

### 15.1 Navigation Manager State Machine

```
                    ┌──────────┐
                    │   OFF    │
                    └────┬─────┘
                         │ nav_start_route()
                         v
                    ┌──────────┐
                    │  CALC    │
                    │ ROUTE    │─── Error ──▶ NAV_ERROR
                    └────┬─────┘
                         │ Route ready
                         v
                    ┌──────────┐
             ┌──────│  ACTIVE  │
             │      └────┬─────┘
             │           │
             │  ┌────────┴────────┐
             │  │                 │
             │  v                 v
             │ ┌──────┐    ┌──────────┐
             │ │PAUSED│    │OFF ROUTE │──▶ Re-route
             │ └──┬───┘    └────┬─────┘
             │    │             │
             │    └──────┬──────┘
             │           │
             │           v
             │      ┌──────────┐
             │      │  ACTIVE  │
             │      └────┬─────┘
             │           │ Arrived at destination
             │           v
             │      ┌──────────┐
             └──────│ ARRIVED  │
                    └────┬─────┘
                         │ nav_cancel() or 60s timeout
                         v
                    ┌──────────┐
                    │   OFF    │
                    └──────────┘

                    ┌──────────┐
                    │  ERROR   │ (from any state)
                    │  "GPS    │─── Auto-retry every 30s
                    │   lost"  │    → OFF → CALC → ACTIVE
                    └──────────┘
```

---

## 16. Sequence Diagrams

### 16.1 Full Navigation Session

```
User         Voice Layer     Decision Eng     Navigation      GPS+IMU       AI Layer
 │                │               │               │             │            │
 │"Navigate home"│               │               │             │            │
 ├──────────────▶│               │               │             │            │
 │               │ Intent:       │               │             │            │
 │               │ NAVIGATE_HOME │               │             │            │
 │               ├──────────────▶│               │             │            │
 │               │               │ nav_start_    │             │            │
 │               │               │ route_to_home│             │            │
 │               │               ├──────────────▶│             │            │
 │               │               │               │ Start GPS   │            │
 │               │               │               ├────────────▶│            │
 │               │               │               │◀─── fix ────┤            │
 │               │               │               │ Load home    │            │
 │               │               │               │ waypoint     │            │
 │               │               │               ├──── route ──┤            │
 │               │               │               │◀── ready ──│            │
 │               │               │               │             │            │
 │               │               │ process_event │             │            │
 │               │               │ ◀─────────────┤             │            │
 │               │               │               │             │            │
 │"Navigating to │               │ Speak:        │             │            │
 │ home. 500m"   │               │ "Navigating   │             │            │
 │◀══════════════║═══════════════║══ to home. ═══║═════════════║═══════════│
 │               │               │  500 meters"  │             │            │
 │               │               │               │             │            │
 │               │               │          (Walking...)        │            │
 │               │               │               │             │            │
 │               │               │               │─── step ────▶│ (IMU)     │
 │               │               │               │◀─ detect ────┤            │
 │               │               │               │─── pos ─────▶│ (GPS)     │
 │               │               │               │             │            │
 │               │               │               │ Turn left    │            │
 │               │               │               │ at 20m      │            │
 │               │               │               ├── event ────│            │
 │               │               │ ◀─────────────┤             │            │
 │               │               │ Process turn  │             │            │
 │               │               │ (priority 2)  │             │            │
 │"In 20 meters, │               │ Speak:        │             │            │
 │ turn left"    │               │ "In 20 meters,│             │            │
 │◀══════════════║═══════════════║══ turn left" ═║═════════════║═══════════│
 │               │               │               │             │            │
 │               │               │               │─── turn ────▶│            │
 │               │               │               │◀─ heading ──┤            │
 │               │               │               │   change    │            │
 │               │               │               ├── event ────│            │
 │               │               │ ◀─────────────┤             │            │
 │               │               │ Confirm turn  │             │            │
 │"Now turn left"│               │ Speak:        │             │            │
 │◀══════════════║═══════════════║══ "Now turn ══║═════════════║═══════════│
 │               │               │  left"        │             │            │
 │               │               │               │             │            │
 │               │               │    (Arriving...)             │            │
 │               │               │               │             │            │
 │               │               │               │─── pos ─────│            │
 │               │               │               │◀ distance   │            │
 │               │               │               │   < 10m     │            │
 │               │               │               ├── event ────│            │
 │               │               │ ◀─────────────┤             │            │
 │               │               │ State: ARRIVED│             │            │
 │"You have       │               │ Speak:       │             │            │
 │ arrived at    │               │ "You have    │             │            │
 │ home"         │               │  arrived at  │             │            │
 │◀══════════════║═══════════════║══ home" ══════║═════════════║═══════════│
 │               │               │               │             │            │
```

---

## 17. Limitations

| ID | Limitation | Impact | Mitigation |
|---|---|---|---|
| NAV-LIM-001 | GPS accuracy degrades in urban canyons | Position error up to 30 m | IMU dead-reckoning + landmark correction |
| NAV-LIM-002 | GPS unavailable indoors | No absolute position | PDR + landmark-based reset |
| NAV-LIM-003 | IMU heading drift: 1°/s uncorrected | Orientation error after 30s | Gyro bias correction in EKF |
| NAV-LIM-004 | Step length varies by user | PDR distance error ±20% | User height calibration + adaptive |
| NAV-LIM-005 | No global path planning on-device | Requires companion app for new routes | Pre-load frequent routes |
| NAV-LIM-006 | Stair detection limited to downward depth edge detection | May miss low-contrast stairs | Multi-frame verification |
| NAV-LIM-007 | Traffic light detection not in v1.0 | No automated crossing guidance | User assessment + audio assist |
| NAV-LIM-008 | Building-level maps not stored | No room-level indoor navigation | Landmark-based guidance |
| NAV-LIM-009 | No elevation data | Stair counting approximate | Calibrated step height |
| NAV-LIM-010 | EKF computation cost | ~5% CPU at 100 Hz update | Optimized matrix operations |

---

## 18. Future SLAM Integration

### 18.1 Visual SLAM Roadmap

```
Phase 1 (Current): Landmark-based navigation
  - Detect doors, stairs, signs as navigation anchors
  - No persistent map

Phase 2 (3-month): Visual Odometry
  - Track visual features between frames
  - Estimate camera motion from feature flow
  - Improve IMU dead-reckoning with visual constraints

Phase 3 (6-month): Lightweight SLAM
  - Build sparse map of visual landmarks
  - Loop closure detection
  - Persistent indoor maps for known environments
  - Map sharing (BLE transfer between devices)

Phase 4 (12-month): Full SLAM
  - Dense mapping for obstacle-aware path planning
  - Semantic SLAM (label mapped objects)
  - Multi-session map merging
  - Cloud map sync (with user consent)
```

### 18.2 SLAM Constraints

| Constraint | Value | Rationale |
|---|---|---|
| Map size limit | 10 MB flash | QSPI flash budget |
| Feature count | 500 points | SRAM limit for active features |
| Loop closure rate | 1 Hz | CPU budget |
| Mapping FPS | 5 fps | Lower than detection to save power |
| Map format | Binary compressed | Minimize flash writes |

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior Navigation Engineer | Initial draft |

---

*End of Document — NAV-ARCH-001*
