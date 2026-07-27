# Decision Engine Design Document

**Document ID:** DEC-ENG-001

**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients

**Author:** Senior Systems Engineer

**Revision:** 0.1

**Date:** 2026-07-27

---

## Table of Contents

1. Executive Summary
2. Decision Engine Overview
3. Decision Trees
4. State Machines
5. Priority Queue
6. Hazard Classification
7. Context Awareness
8. User Intent Recognition
9. Operating Modes
10. Flowcharts
11. Pseudocode
12. UML Activity Diagrams

---

## 1. Executive Summary

The Decision Engine is the cognitive core of the smart glasses system. It determines **when** to speak, **what** to speak, and **when to remain silent**. It prioritizes hazards, manages context, and ensures the user is never overwhelmed with information. The engine is implemented as a hybrid of deterministic state machines and priority-queue-driven decision logic. No ML is used here — all decisions are rule-based for reliability, traceability, and deterministic timing.

---

## 2. Decision Engine Overview

### 2.1 Architecture

```
+---------------------------------------------------------------------+
|                       DECISION ENGINE                                |
+---------------------------------------------------------------------+
|                                                                     |
|  INPUTS ←──────────────────────────────────────────────────┐       |
|  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐      │       |
|  │ AI Layer │ │ Nav      │ │ Context  │ │ Voice    │      │       |
|  │ Results  │ │ State    │ │ Manager  │ │ Intents  │      │       |
|  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘      │       |
|       │             │             │             │           │       |
|       v             v             v             v           │       |
|  ┌─────────────────────────────────────────────────────┐   │       |
|  │            EVENT MULTIPLEXER                        │   │       |
|  │  (Collects all inputs into prioritized queue)       │   │       |
|  └────────────────────────┬────────────────────────────┘   │       |
|                           │                                │       |
|                           v                                │       |
|  ┌─────────────────────────────────────────────────────┐   │       |
|  │              PRIORITY QUEUE                         │   │       |
|  │  Level 0: Silent                                    │   │       |
|  │  Level 1: Routine (nav, reminders)                  │   │       |
|  │  Level 2: Important (obstacle warning)              │   │       |
|  │  Level 3: Critical (fall, hazard, emergency)        │   │       |
|  └────────────────────────┬────────────────────────────┘   │       |
|                           │                                │       |
|                           v                                │       |
|  ┌─────────────────────────────────────────────────────┐   │       |
|  │              STATE MACHINE                          │   │       |
|  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────┐  │   │       |
|  │  │ Walking  │ │ Indoor   │ │ Road     │ │ Shop  │  │   │       |
|  │  │ Mode     │ │ Mode     │ │ Crossing │ │ Mode  │  │   │       |
|  │  └──────────┘ └──────────┘ └──────────┘ └───────┘  │   │       |
|  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────┐  │   │       |
|  │  │ Home     │ │ Hospital │ │ Emergency│ │ Silent│  │   │       |
|  │  │ Mode     │ │ Mode     │ │ Mode     │ │ Mode  │  │   │       |
|  │  └──────────┘ └──────────┘ └──────────┘ └───────┘  │   │       |
|  └────────────────────────┬────────────────────────────┘   │       |
|                           │                                │       |
|                           v                                │       |
|  ┌─────────────────────────────────────────────────────┐   │       |
|  │              DECISION OUTPUT                        │   │       |
|  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────┐  │   │       |
|  │  │ Speak?   │ │ What?    │ │ Urgency  │ │ Tone  │  │   │       |
|  │  │ Yes/No   │ │ Text     │ │ 1-3      │ │ Calm/ │  │   │       |
|  │  │          │ │ string   │ │          │ │ Alert │  │   │       |
|  │  └──────────┘ └──────────┘ └──────────┘ └───────┘  │   │       |
|  └────────────────────────┬────────────────────────────┘   │       |
|                           │                                │       |
|                           v                                │       |
|                   Voice Layer → TTS → Speaker              │       |
|                                                             │       |
+---------------------------------------------------------------------+
```

### 2.2 Input Event Types

| Event Source | Event Type | Priority | Example |
|---|---|---|---|
| AI — Object Detection | OBSTACLE_NEAR | Critical (3) | "Chair, 0.5 meters" |
| AI — Fall Detection | FALL_DETECTED | Critical (3) | "Fall detected!" |
| AI — Geo Fence | GEO_ALERT | Critical (3) | "Leaving safe zone" |
| AI — Object Detection | OBSTACLE_FAR | Important (2) | "Wall, 3 meters" |
| Voice — Intent | USER_COMMAND | Important (2) | "Navigate home" |
| Nav — Turn Alert | TURN_ALERT | Important (2) | "Turn left in 20m" |
| Nav — Status | NAV_STATUS | Routine (1) | "Continuing straight" |
| Voice — Intent | REMINDER | Routine (1) | "Take medicine" |
| BMS — Status | BATTERY_LOW | Routine (1) | "Battery 20%" |
| AI — Scene | SCENE_CHANGE | Routine (1) | "Entering park" |
| Nav — Progress | NAV_PROGRESS | Routine (1) | "200m remaining" |
| System — Info | SYS_INFO | Silent (0) | "GPS fix acquired" |

---

## 3. Decision Trees

### 3.1 To-Speak-or-Not Decision Tree

```
                    ┌─────────────────────────────┐
                    │  New Event Received         │
                    └─────────────┬───────────────┘
                                  │
                                  v
                    ┌─────────────────────────────┐
                    │  Is user in SILENT MODE?    │──Yes──→ DON'T SPEAK
                    └─────────────┬───────────────┘
                                  │ No
                                  v
                    ┌─────────────────────────────┐
                    │  Is priority ≥ CRITICAL?    │──Yes──→ SPEAK IMMEDIATELY
                    └─────────────┬───────────────┘        (interrupt current)
                                  │ No
                                  v
                    ┌─────────────────────────────┐
                    │  Is audio currently         │──Yes──→ Queue event
                    │  playing?                   │        (don't interrupt)
                    └─────────────┬───────────────┘
                                  │ No
                                  v
                    ┌─────────────────────────────┐
                    │  Has same event been        │──Yes──→ Is distance
                    │  announced within 10s?      │        decreased?
                    └─────────────┬───────────────┘     │
                                  │ No                  ├─Yes→ SPEAK
                                  │                     └─No → DON'T SPEAK
                                  v
                    ┌─────────────────────────────┐
                    │  Is priority ≤ ROUTINE?     │──Yes──→ Is user in
                    │                             │        conversation? (VAD)
                    └─────────────┬───────────────┘     │
                                  │ No                  ├─Yes→ QUEUE for later
                                  │                     └─No → SPEAK
                                  v
                           SPEAK WITH
                           APPROPRIATE URGENCY
```

### 3.2 Content Selection Tree

```
                    ┌─────────────────────────────┐
                    │  What to speak?             │
                    └─────────────┬───────────────┘
                                  │
                    ┌─────────────┴───────────────┐
                    │  Determine event type       │
                    └─────────────┬───────────────┘
                                  │
        ┌─────────────────────────┼─────────────────────────┐
        v                         v                         v
┌───────────────┐       ┌──────────────────┐      ┌──────────────────┐
│ HAZARD EVENT  │       │ NAVIGATION EVENT │      │ USER INFO EVENT  │
└───────┬───────┘       └────────┬─────────┘      └────────┬─────────┘
        │                        │                         │
        v                        v                         v
┌───────────────┐       ┌──────────────────┐      ┌──────────────────┐
│ Format:       │       │ Format:           │      │ Format:          │
│ "{object},    │       │ "In {distance}    │      │ "Battery is      │
│  {distance}.  │       │  meters, {action}"│      │  {level}%"       │
│  {direction}"  │       │                  │      │                  │
│               │       │ "Turn {dir} at    │      │ "Reminder:       │
│ "Stairs down, │       │  next {landmark}" │      │  {description}"  │
│  1.5 meters,  │       │                  │      │                  │
│  ahead"       │       │ "Arriving at     │      │ "It's {time},    │
│               │       │  destination in  │      │  {weather}?"     │
│ "Road ahead,  │       │  {distance}"     │      │                  │
│  3 meters"    │       │                  │      │                  │
└───────────────┘       └──────────────────┘      └──────────────────┘
```

---

## 4. State Machines

### 4.1 Global Operating Mode State Machine

```
                          ┌─────────┐
                          │  BOOT   │
                          └────┬────┘
                               │
                               v
                    ┌─────────────────────┐
           ┌───────│      IDLE           │
           │       │  (no nav, no task)  │
           │       └──────────┬──────────┘
           │                  │
           │      ┌───────────┴───────────┐
           │      │                       │
           │      v                       v
           │ ┌──────────┐         ┌──────────────┐
           │ │ WALKING  │         │  INDOOR      │
           │ │ MODE     │         │  MODE        │
           │ └─────┬────┘         └──────┬───────┘
           │       │                     │
           │       v                     v
           │ ┌──────────┐         ┌──────────────┐
           │ │ ROAD     │         │  HOME        │
           │ │ CROSSING │         │  MODE        │
           │ │ MODE     │         └──────┬───────┘
           │ └─────┬────┘               │
           │       │                    v
           │       v             ┌──────────────┐
           │ ┌──────────┐        │  HOSPITAL    │
           │ │ SHOPPING │        │  MODE        │
           │ │ MODE     │        └──────┬───────┘
           │ └─────┬────┘              │
           │       │                   v
           │       v            ┌──────────────┐
           │ ┌──────────┐       │  EMERGENCY   │
           │ │ SILENT   │       │  MODE        │
           │ │ MODE     │       │ (from any    │
           │ └─────┬────┘       │  state)      │
           │       │            └──────┬───────┘
           │       │                   │
           │       └────────┬──────────┘
           │                │
           │                v
           │       ┌─────────────────────┐
           └───────│      SLEEP          │
                   └─────────────────────┘
```

### 4.2 State Transition Triggers

| From | To | Trigger |
|---|---|---|
| BOOT | IDLE | System ready, no pending tasks |
| IDLE | WALKING | GPS speed > 1 m/s OR IMU step detection |
| IDLE | INDOOR | Scene classifier: indoor |
| WALKING | ROAD_CROSSING | Scene: crosswalk detected + GPS near intersection |
| WALKING | SHOPPING | Scene: store + GPS near known retail |
| WALKING | INDOOR | Scene: indoor |
| INDOOR | HOME | GPS: home geo-fence + scene: indoor_home |
| INDOOR | HOSPITAL | GPS: hospital geo-fence |
| ANY | EMERGENCY | Fall detected OR panic button OR "Emergency" voice command |
| ANY | SILENT | Voice command "Silent mode" |
| ANY | SLEEP | 5 min inactivity (IMU: no motion, no voice) |
| SLEEP | IDLE | IMU motion detected OR "Hey Glass" wake word |
| EMERGENCY | IDLE | Emergency resolved (30 min timeout OR caregiver reset) |
| WALKING | ROAD_CROSSING | Crosswalk detected + GPS speed < 2 m/s |

---

## 5. Priority Queue

### 5.1 Queue Architecture

```c
// priority_queue.h

#define PRIORITY_LEVELS 4

typedef enum {
    PRIORITY_SILENT    = 0,  // System info, never spoken
    PRIORITY_ROUTINE   = 1,  // Nav updates, reminders
    PRIORITY_IMPORTANT = 2,  // Obstacles, user commands
    PRIORITY_CRITICAL  = 3   // Fall, hazard, emergency
} priority_t;

typedef struct {
    priority_t      priority;
    uint32_t        timestamp_ms;    // When event occurred
    event_type_t    event_type;      // OBSTACLE, NAV, VOICE, etc.
    char            message[128];    // Formatted speech text
    float           distance_m;      // For obstacle: distance
    uint8_t         repeat_count;    // How many times announced
    bool            is_urgent;       // Interrupt current speech?
    void*           context_data;    // Pointer to event context
} priority_queue_entry_t;

typedef struct {
    priority_queue_entry_t entries[PRIORITY_LEVELS][32];  // Per-level circular buffer
    uint8_t                head[PRIORITY_LEVELS];
    uint8_t                tail[PRIORITY_LEVELS];
    SemaphoreHandle_t      mutex;
} priority_queue_t;

// Core API
int  priority_queue_init(priority_queue_t* q);
int  priority_queue_push(priority_queue_t* q, priority_queue_entry_t* entry);
int  priority_queue_pop_highest(priority_queue_t* q, priority_queue_entry_t* out);
int  priority_queue_peek(priority_queue_t* q, priority_queue_entry_t* out);
void priority_queue_clear(priority_queue_t* q);
```

### 5.2 Queue Service Logic

```c
// decision_engine.c — Main decision loop

void decision_task(void* params) {
    priority_queue_t queue;
    priority_queue_init(&queue);

    while (1) {
        // 1. Wait for any event (up to 100 ms timeout)
        decision_event_t event;
        if (xQueueReceive(decision_event_queue, &event, pdMS_TO_TICKS(100))) {

            // 2. Classify event and determine priority
            priority_queue_entry_t entry = classify_event(&event);

            // 3. Deduplication: skip if same event type + same object < 10s ago
            if (is_duplicate(&entry)) {
                continue;
            }

            // 4. Push to priority queue
            priority_queue_push(&queue, &entry);
        }

        // 5. Process queue (every iteration, even without new events)
        priority_queue_entry_t next;
        if (priority_queue_peek(&queue, &next) == 0) {

            // 6. Check if we should speak now
            if (should_speak_now(&next)) {

                // 7. Apply mode-specific formatting
                char formatted[128];
                format_message(&next, context_get_current_mode(), formatted);

                // 8. Check cooldown: don't repeat same info too often
                if (check_cooldown(&next)) {
                    // 9. Send to voice layer
                    voice_speak(formatted, get_urgency_tone(&next));

                    // 10. If critical, also trigger haptic
                    if (next.priority == PRIORITY_CRITICAL) {
                        haptic_alert(3);  // Triple pulse
                    }

                    next.repeat_count++;
                }

                // 11. Remove from queue
                priority_queue_pop_highest(&queue, NULL);
            }
        }

        // 12. Cooldown management: age-out old entries
        age_out_stale_entries(&queue, 30000);  // 30 second max age
    }
}
```

### 5.3 Queue Processing Priority

```
Always process CRITICAL first:
  ┌── FALL_DETECTED ──→ IMMEDIATE SPEECH + HAPTIC + EMERGENCY MODE
  ├── GEO_ALERT      ──→ IMMEDIATE SPEECH + HAPTIC
  └── HAZARD_IMMEDIATE→ IMMEDIATE SPEECH

Then IMPORTANT:
  ┌── OBSTACLE_FAR   ──→ SPEAK if not repeating
  ├── USER_COMMAND   ──→ PROCESS command, may interrupt
  └── TURN_ALERT     ──→ SPEAK if within turn distance

Then ROUTINE:
  ┌── NAV_STATUS     ──→ SPEAK only if user hasn't heard in 30s
  ├── REMINDER       ──→ SPEAK at scheduled time
  ├── BATTERY_LOW    ──→ SPEAK once per threshold crossing
  └── SCENE_CHANGE   ──→ SPEAK once per scene change

SILENT is never spoken (logged only)
```

---

## 6. Hazard Classification

### 6.1 Hazard Levels

```
Hazard Level 3 — IMMINENT DANGER (speak immediately, interrupt all)
  ├── Fall detected (IMU: > 2.5 g impact + orientation change)
  ├── Stairwell drop-off < 0.5 m
  ├── Vehicle approaching > 10 km/h, < 5 m
  ├── Geo-fence breach (Alzheimer's patient leaving safe zone)
  └── Fire/smoke (future: sensor-dependent)

Hazard Level 2 — WARNING (speak soon, within 1 second)
  ├── Obstacle < 1.0 m (chair, table, person)
  ├── Stairs/curb < 1.0 m
  ├── Road approaching < 3.0 m
  ├── Downward edge (drop-off) < 1.5 m
  └── Rapid descent detected (IMU z-acceleration)

Hazard Level 1 — CAUTION (speak when convenient, within 3 seconds)
  ├── Obstacle 1.0-3.0 m
  ├── Uneven terrain detected
  ├── Wet floor (future: sensor-dependent)
  ├── Doorway ahead
  └── Crowded area (many objects detected)

Hazard Level 0 — INFO (no speech, log only)
  ├── Object detected > 3.0 m
  ├── Scene change
  ├── GPS fix acquired
  └── Battery level update
```

### 6.2 Hazard Prioritization Algorithm

```c
// hazard_classifier.c

uint8_t classify_hazard(obstacle_t* obs, context_t* ctx) {
    uint8_t score = 0;

    // Base score from distance
    if (obs->distance_m < 0.5f)      score += 10;
    else if (obs->distance_m < 1.0f)  score += 7;
    else if (obs->distance_m < 2.0f)  score += 4;
    else if (obs->distance_m < 3.0f)  score += 2;
    else                               score += 0;

    // Modifier: object type
    switch (obs->class_id) {
        case CLASS_VEHICLE:    score += 5; break;  // Moving vehicle = extreme
        case CLASS_STAIRS:     score += 4; break;  // Stairs = high fall risk
        case CLASS_CURB:      score += 3; break;
        case CLASS_PERSON:    score += 2; break;   // Person may move
        case CLASS_BICYCLE:   score += 3; break;
        case CLASS_TABLE:     score += 1; break;
        case CLASS_CHAIR:     score += 1; break;
        case CLASS_WALL:      score += 1; break;
        case CLASS_DOOR:      score += 0; break;
        default:              score += 1; break;
    }

    // Modifier: motion (from IMU + object tracking)
    if (obs->is_moving)        score += 3;
    if (ctx->activity == RUNNING)  score += 2;

    // Modifier: user speed (faster = more dangerous)
    if (ctx->speed_mps > 1.5f) score += 2;
    if (ctx->speed_mps > 0.8f) score += 1;

    // Modifier: context
    if (ctx->location == OUTDOOR_URBAN) score += 1;
    if (ctx->zone == CROSSWALK)         score += 2;
    if (ctx->lighting == DIM || ctx->lighting == DARK) score += 2;

    // Map score to hazard level
    if (score >= 12) return HAZARD_LEVEL_3;  // Critical
    if (score >= 7)  return HAZARD_LEVEL_2;  // Warning
    if (score >= 3)  return HAZARD_LEVEL_1;  // Caution
    return HAZARD_LEVEL_0;                   // Info
}
```

---

## 7. Context Awareness

### 7.1 Context Manager

```c
// context_manager.h

typedef struct {
    // Current mode
    operating_mode_t    mode;           // WALKING, INDOOR, HOME, etc.
    operating_mode_t    previous_mode;  // For returning from EMERGENCY

    // Activity
    activity_t          activity;
    bool                is_walking;
    bool                is_running;
    bool                is_stationary;
    float               speed_mps;
    float               heading_deg;

    // Environment
    scene_class_t       scene;
    lighting_t          lighting;
    noise_level_t       noise;
    bool                is_crowded;

    // Location
    geo_zone_t          zone;
    bool                near_home;
    bool                near_hospital;
    bool                near_crosswalk;
    bool                near_geo_fence_boundary;

    // User state
    bool                is_speaking;        // VAD: user is talking
    bool                heard_recently;     // User heard prompt < 30s ago
    uint32_t            last_prompt_ms;     // When last audio was played
    uint32_t            last_user_response_ms;

    // Alzheimer's specific
    uint32_t            time_in_current_zone_ms;
    uint8_t             reprompts_since_response;
    bool                 orientation_reminder_due;

    // System
    uint8_t             battery_pct;
    bool                charging;
    bool                privacy_mode;
    bool                fall_detected_this_session;
} context_t;
```

### 7.2 Context Update Loop

```c
// context_manager.c

void context_update(context_t* ctx) {
    // Update activity from IMU
    imu_data_t imu;
    imu_get_last(&imu);

    float acc_magnitude = sqrt(imu.accel_x*imu.accel_x +
                                imu.accel_y*imu.accel_y +
                                imu.accel_z*imu.accel_z);

    if (acc_magnitude > 15.0f) {  // > 1.5g
        ctx->activity = FALLING;
    } else if (ctx->speed_mps > 2.0f) {
        ctx->activity = RUNNING;
    } else if (ctx->speed_mps > 0.5f) {
        ctx->activity = WALKING;
    } else if (ctx->speed_mps > 0.05f) {
        ctx->activity = SLOW_WALKING;
    } else {
        ctx->activity = STANDING;
    }

    // Update scene from AI (runs at 1 Hz)
    scene_class_t new_scene = ai_get_last_scene();
    if (new_scene != ctx->scene) {
        ctx->scene = new_scene;
        // Scene change triggers mode re-evaluation
        evaluate_mode_transition(ctx);
    }

    // Update location from GPS
    gps_position_t pos;
    gps_get_position(&pos);
    ctx->speed_mps = pos.speed_mps;
    ctx->heading_deg = pos.heading_deg;
    ctx->near_home = is_within_geo_fence(pos, GEO_FENCE_HOME);
    ctx->near_hospital = is_within_geo_fence(pos, GEO_FENCE_HOSPITAL);

    // Update geo-fence status
    for (int i = 0; i < geo_fence_count; i++) {
        if (is_within_zone(pos, geo_fences[i])) {
            ctx->zone = geo_fences[i].type;
            ctx->near_geo_fence_boundary = is_near_boundary(pos, geo_fences[i]);
            break;
        }
    }

    // Check crossing detection
    if (ctx->scene == SCENE_CROSSWALK && ctx->speed_mps < 2.0f) {
        ctx->near_crosswalk = true;
    } else {
        ctx->near_crosswalk = false;
    }

    // Update user speaking state
    ctx->is_speaking = voice_is_user_speaking();

    // Update system state
    ctx->battery_pct = battery_get_level();
    ctx->charging = battery_is_charging();

    ctx->last_update_ms = xTaskGetTickCount();
}
```

---

## 8. User Intent Recognition

### 8.1 Intent → Action Mapping

```
┌─────────────────────────────────────────────────────────────┐
│                  INTENT → ACTION MAP                        │
├─────────────────────────────────────────────────────────────┤
│                                                            │
│ INTENT_NAVIGATE  →  nav_start(destination)                  │
│                      → "Navigating to {place}"              │
│                                                            │
│ INTENT_STOP      →  nav_stop()                              │
│                      → "Navigation stopped"                 │
│                                                            │
│ INTENT_HOME      →  nav_start("home")                       │
│                      → "Heading home"                       │
│                                                            │
│ INTENT_HELP      →  speak_context_summary()                 │
│                      → "You are at {location}.              │
│                         Nearby: {objects}.                  │
│                         Say 'navigate to' to go somewhere"  │
│                                                            │
│ INTENT_DESCRIBE  →  scene_summarize()                       │
│                      → "You are in a {scene}.               │
│                         I see {count} objects:              │
│                         {list}. In front of you: {obj}"     │
│                                                            │
│ INTENT_READ      →  trigger_text_recognition()              │
│                      → "Text says: {text}"                  │
│                                                            │
│ INTENT_EMERGENCY →  activate_emergency_mode()               │
│                      → "Emergency mode activated.           │
│                         Alerting {contact}"                 │
│                      → Haptic: 3 long pulses                │
│                      → Send SMS to emergency contact        │
│                                                            │
│ INTENT_BATTERY   →  "Battery is at {level}%.               │
│                      Estimated {hours} hours remaining"     │
│                                                            │
│ INTENT_LOCATION  →  "You are near {place}.                 │
│                      Address: {address}                     │
│                      Facing {heading}"                      │
│                                                            │
│ INTENT_SILENT    →  toggle_silent_mode()                    │
│                      → "Silent mode on" / "Silent mode off" │
│                                                            │
│ INTENT_PRIVACY   →  toggle_privacy_mode()                   │
│                      → "Privacy mode on. Cameras disabled"  │
│                                                            │
│ INTENT_VOLUME    →  voice_set_volume(level)                │
│                      → "Volume set to {level}"              │
│                                                            │
│ INTENT_MEDICINE  →  get_next_medicine_reminder()            │
│                      → "Next medicine: {name} at {time}"    │
│                                                            │
│ INTENT_REMIND    →  schedule_reminder(time, description)    │
│                      → "Reminder set for {time}"            │
│                                                            │
└─────────────────────────────────────────────────────────────┘
```

---

## 9. Operating Modes

### 9.1 Walking Mode

```
WALKING MODE
Description: Active outdoor pedestrian navigation

Primary behaviors:
  - Obstacle detection at full rate (15 fps inference)
  - Navigation guidance active
  - Road crossing detection enabled
  - Turn-by-turn audio prompts every 30-60 seconds
  - Hazard alerts: immediate for Level 2-3, queued for Level 1

Audio cadence:
  - Obstacle alerts: immediate (< 1 m), within 1s (1-2 m), within 3s (2-3 m)
  - Nav prompts: every 30s if route active
  - No more than 1 prompt per 5 seconds (anti-spam)
  - Silence period after user command response: 10 seconds

Alzheimer's-specific:
  - Orientation reminder every 5 minutes: "You are walking on {street}"
  - If no movement for 2 minutes: "Are you okay? Say 'help' if needed"
  - Geo-fence boundary: "You are near the edge of your safe zone"

Hazard priority:
  Level 3: Vehicle, stairs, fall → SPEAK IMMEDIATELY (interrupt)
  Level 2: Obstacle < 1 m, curb → SPEAK within 1 second
  Level 1: Obstacle 1-3 m → SPEAK within 3 seconds (queue)
  Level 0: All else → SILENT
```

### 9.2 Indoor Mode

```
INDOOR MODE
Description: Inside building, no GPS-based navigation

Primary behaviors:
  - Obstacle detection at reduced rate (10 fps inference)
  - Door detection enabled
  - Stair detection enabled
  - Text reading available on demand
  - Indoor navigation via IMU dead-reckoning + landmark detection

Audio cadence:
  - Reduced prompts: only Level 2+ hazards spoken
  - No routine navigation updates
  - Building-specific: "Exit door on your left" once per door

Alzheimer's-specific:
  - Room identification: "You are in the kitchen"
  - If wandering detected (back-and-forth pattern): "Can I help you find something?"
  - Near bathroom: "The restroom is on your right"

Hazard priority:
  Level 3: Stairs, fall → SPEAK IMMEDIATELY
  Level 2: Obstacle < 0.5 m, drop-off → SPEAK within 1 second
  Level 1: Obstacle 0.5-2 m → SPEAK within 5 seconds
  Level 0: All else → SILENT
```

### 9.3 Road Crossing Mode

```
ROAD CROSSING MODE
Description: Approaching or crossing a pedestrian crosswalk

Primary behaviors:
  - Maximum alert rate (full 15 fps inference)
  - Traffic light detection enabled (future)
  - Vehicle detection at maximum range
  - Left/right look guidance
  - No other non-critical prompts during crossing

Audio sequence (automated):
  1. "Crosswalk ahead, 10 meters" (when detected at ~10 m)
  2. "Stop. Check for traffic" (when at crosswalk edge)
  3. "Road is clear. Cross now" (when no vehicles detected)
  4. "Vehicle approaching from left" (if vehicle detected)
  5. "Hurry. Vehicle approaching" (if vehicle close + user mid-crossing)

User commands during crossing:
  - "Is it clear?" → Current traffic assessment
  - "Help me cross" → Detailed guidance mode
  - "Stop" → Returns to curb

Alzheimer's-specific:
  - Verbose guidance: every step during crossing
  - Always hold user's hand (requires caregiver present)
  - "Wait for the green light" if traffic light detected
```

### 9.4 Shopping Mode

```
SHOPPING MODE
Description: Inside retail store

Primary behaviors:
  - Obstacle detection at reduced rate (5 fps)
  - Text reading: product labels, signs, prices
  - Aisle detection
  - Shopping list item locator (future)

Audio cadence:
  - Minimal obstacle alerts (store has predictable layouts)
  - Reading on demand only
  - "Aisle {number} on your {direction}" at each aisle
  - "You are near the {department} section"

Special:
  - "What's on my list?" → Reads shopping list
  - "Find {item}" → Navigate to aisle with item
  - "How much is this?" → Read price tag
```

### 9.5 Home Mode

```
HOME MODE
Description: Inside user's residence

Primary behaviors:
  - Obstacle detection at minimum rate (2 fps, power saving)
  - Known layout, navigation not active
  - Furniture location known (pre-mapped or learned)
  - Focus on Alzheimer's-specific features

Audio cadence:
  - Maximum silence. Only critical alerts.
  - Time and orientation reminders only if configured
  - "Your medicine is at 7:00 PM" (scheduled)

Alzheimer's-specific:
  - Orientation: "You are at home" every 30 min
  - Time: "It is {time}. {meal} time" at appropriate hours
  - Geo-fence: "Remember, do not leave without {caregiver}"
  - If unusual motion pattern (pacing): "Can I help you?"
  - Reminders: medication, meals, appointments
```

### 9.6 Hospital Mode

```
HOSPITAL MODE
Description: Inside hospital or clinic

Primary behaviors:
  - Standard obstacle detection (10 fps)
  - Special signs: department names, room numbers, directional signs
  - Quiet mode: lowest possible volume, shortest prompts
  - No navigation announcements (use whisper mode)

Audio:
  - Whisper-level volume (level 1 of 10)
  - Minimal words: "Room 204. Left."
  - No "beeps" or alert sounds
  - Text reading for signs only

Alzheimer's-specific:
  - "You are at the hospital. {caregiver} is with you."
  - Frequent reorientation to reduce anxiety
  - "The doctor will see you soon"
```

### 9.7 Emergency Mode

```
EMERGENCY MODE
Description: Fall detected or emergency triggered

Immediate actions (t = 0 ms):
  1. HALT all non-critical processing
  2. Play emergency tone (3 long beeps)
  3. "Emergency detected. Stay calm. Help is coming."
  4. Activate haptic: SOS pattern (3 long, 3 short, 3 long)
  5. Send SMS to emergency contact:
     "EMERGENCY: Smart glasses user may need help.
     Location: {lat}, {lon} ({address})
     Time: {timestamp}"

Continued actions:
  - Keep detecting: listen for voice commands
  - "I'm okay" → Cancel emergency, notify contact "False alarm"
  - "Help" → Keep emergency active, play periodic reassurance
  - GPS tracking active, send position updates every 60 seconds
  - If no response for 5 minutes → escalate (call emergency services)

Post-emergency:
  - Log full event to database
  - Return to previous operating mode
  - "Emergency mode deactivated. You are safe."
```

### 9.8 Silent Mode

```
SILENT MODE
Description: No audio output, haptic only

Behaviors:
  - All speech suppressed
  - Hazards: haptic feedback only
    - Level 3: 3 long pulses
    - Level 2: 2 medium pulses
    - Level 1: 1 short pulse
  - All non-critical events: silent
  - Voice commands still active (responses via haptic confirmation)
  - "Wake me" → Audio enabled

Use cases:
  - Library, movie theater, quiet restaurant
  - User wants privacy
  - Nighttime (when caregiver is sleeping)
```

---

## 10. Flowcharts

### 10.1 Main Decision Loop Flowchart

```
                    ┌──────────────────────┐
                    │    DECISION LOOP      │
                    │    (runs every        │
                    │     100 ms or on      │
                    │     event)            │
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
                    │ Collect all pending   │
                    │ inputs from message   │
                    │ bus                   │
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
               ┌───│ Any events pending?   │───No───┐
               │   └───────────┬──────────┘        │
               │               │ Yes                │
               │               v                    │
               │   ┌──────────────────────┐        │
               │   │ Classify event       │        │
               │   │ priority             │        │
               │   └───────────┬──────────┘        │
               │               │                    │
               │               v                    │
               │   ┌──────────────────────┐        │
               │   │ Push to priority      │        │
               │   │ queue                │        │
               │   └───────────┬──────────┘        │
               │               │                    │
               │               v                    │
               │   ┌──────────────────────┐        │
               ├──▶│ Pop highest priority  │        │
               │   │ event from queue      │        │
               │   └───────────┬──────────┘        │
               │               │                    │
               │               v                    │
               │   ┌──────────────────────┐        │
               │   │ Is SILENT mode?      │──Yes───▶│→ Haptic only
               │   └───────────┬──────────┘        │
               │               │ No                 │
               │               v                    │
               │   ┌──────────────────────┐        │
               │   │ Check: is duplicate? │──Yes───▶│→ Skip
               │   │ (same event < 10s)  │        │
               │   └───────────┬──────────┘        │
               │               │ No                 │
               │               v                    │
               │   ┌──────────────────────┐        │
               │   │ Check: cooldown?     │──No────▶│→ Skip (fed back)
               │   │ (not too frequent)   │        │
               │   └───────────┬──────────┘        │
               │               │ Yes                │
               │               v                    │
               │   ┌──────────────────────┐        │
               │   │ Check: user speaking?│──Yes───▶│→ Queue for later
               │   │ (VAD active)        │        │
               │   └───────────┬──────────┘        │
               │               │ No                 │
               │               v                    │
               │   ┌──────────────────────┐        │
               │   │ Format message for   │        │
               │   │ current mode          │        │
               │   └───────────┬──────────┘        │
               │               │                    │
               │               v                    │
               │   ┌──────────────────────┐        │
               │   │ Send to Voice Layer  │        │
               │   │ → TTS → Speaker      │        │
               │   │ If critical: +haptic │        │
               │   └───────────┬──────────┘        │
               │               │                    │
               └───────────────┘                    │
                                                    │
                    ┌───────────────────────────────┘
                    │
                    v
                    ┌──────────────────────┐
                    │ Age out stale entries │
                    │ (> 30s)              │
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
                    │ Update context       │
                    │ (IMU, GPS, AI)       │
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
                    │ Evaluate mode        │
                    │ transition           │
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
                    │ Check for Alzheimer's │
                    │ reminders / prompts   │
                    └───────────┬──────────┘
                                │
                                v
                    ┌──────────────────────┐
                    │ Return to loop start  │
                    └──────────────────────┘
```

### 10.2 Hazard Response Flowchart

```
              ┌─────────────────────────────┐
              │  NEW HAZARD EVENT           │
              │  obstacle_t from AI layer   │
              └─────────────┬───────────────┘
                            │
                            v
              ┌─────────────────────────────┐
              │  Calculate hazard score     │
              │  (distance + type + speed   │
              │   + context + lighting)     │
              └─────────────┬───────────────┘
                            │
                            v
              ┌─────────────────────────────┐
              │  Score ≥ 12?    ──Yes──▶ LEVEL 3: CRITICAL
              └─────────────┬───────────────┘
                            │ No
                            v
              ┌─────────────────────────────┐
              │  Score ≥ 7?     ──Yes──▶ LEVEL 2: WARNING
              └─────────────┬───────────────┘
                            │ No
                            v
              ┌─────────────────────────────┐
              │  Score ≥ 3?     ──Yes──▶ LEVEL 1: CAUTION
              └─────────────┬───────────────┘
                            │ No
                            v
                       LEVEL 0: INFO (log)
```

---

## 11. Pseudocode

### 11.1 Main Decision Loop

```
FUNCTION decision_engine_loop():
    INITIALIZE priority_queue
    INITIALIZE context
    LOAD state machine
    SET current_mode = IDLE

    LOOP:
        // 1. Collect all pending events from message bus
        events = message_bus_collect(MSG_TYPE_ALL, timeout_ms=0)

        // 2. Classify and queue each event
        FOR EACH event IN events:
            entry = classify_event(event)
            IF NOT is_duplicate(entry, priority_queue):
                priority_queue.push(entry)

        // 3. Process high-priority events first
        WHILE priority_queue.peek(entry) == SUCCESS:
            IF NOT should_speak(entry, context):
                priority_queue.pop()
                CONTINUE

            message = format_message(entry, context.current_mode)
            voice_speak(message, get_tone(entry))

            IF entry.priority == CRITICAL:
                haptic_alert(pattern=THREE_PULSES)

            priority_queue.pop()

        // 4. Context management
        update_context(context)
        check_mode_transition(context)

        // 5. Alzheimer's-specific checks
        IF context.mode == HOME OR context.mode == INDOOR:
            IF time_since_last_orientation > 30 minutes:
                voice_speak("You are at home. It is {time}.")
                reset_orientation_timer()

            IF medicine_reminder_due():
                voice_speak("Time for your medicine: {name}")

        // 6. Sleep check
        IF inactivity_timeout_exceeded(5 minutes) AND context.mode != EMERGENCY:
            voice_speak("Going to sleep. Say 'Hey Glass' to wake me.")
            enter_sleep_mode(context)

        WAIT(100 ms)  // or wake on event
    END LOOP
END FUNCTION
```

### 11.2 Mode Transition Evaluation

```
FUNCTION evaluate_mode_transition(ctx):
    new_mode = ctx.current_mode

    // Emergency has highest priority
    IF ctx.activity == FALLING OR ctx.emergency_command:
        new_mode = EMERGENCY
        ctx.previous_mode = ctx.current_mode
        CALL transition_to(EMERGENCY, ctx)
        RETURN

    // Scene-based transitions
    SWITCH ctx.scene:
        CASE outdoor_street:
            IF ctx.speed_mps > 1.0:
                new_mode = WALKING
        CASE crosswalk:
            IF ctx.speed_mps < 2.0:
                new_mode = ROAD_CROSSING
        CASE indoor_home:
            IF ctx.near_home:
                new_mode = HOME
            ELSE:
                new_mode = INDOOR
        CASE indoor_hospital:
            new_mode = HOSPITAL
        CASE indoor_store:
            new_mode = SHOPPING
        CASE indoor_office:
            new_mode = INDOOR
        CASE outdoor_park:
            new_mode = WALKING
        CASE outdoor_sidewalk:
            new_mode = WALKING
        ELSE:
            IF ctx.speed_mps > 1.0:
                new_mode = WALKING
            ELSE:
                new_mode = INDOOR

    // Silent mode is user-commanded, not automatic
    IF ctx.silent_mode_command AND new_mode != EMERGENCY:
        new_mode = SILENT

    IF new_mode != ctx.current_mode:
        CALL transition_to(new_mode, ctx)
END FUNCTION
```

---

## 12. UML Activity Diagrams

### 12.1 Decision Processing Activity

```
┌─────────┐     ┌────────┐     ┌────────┐     ┌────────┐     ┌──────────┐
│ Receive  │────▶│Classify│────▶│ Push   │────▶│ Wait   │────▶│ Process  │
│ Event    │     │Priority│     │Queue   │     │100ms   │     │ Highest  │
└─────────┘     └────────┘     └────────┘     └────────┘     └────┬─────┘
                                                                    │
                                                          ┌─────────┴─────────┐
                                                          │                   │
                                                          v                   v
                                                    ┌──────────┐      ┌──────────┐
                                                    │ Should   │──No──▶│ Skip &   │
                                                    │ Speak?   │      │ Pop      │
                                                    └────┬─────┘      └──────────┘
                                                         │ Yes
                                                         v
                                                    ┌──────────┐
                                                    │ Format   │
                                                    │ Message  │
                                                    └────┬─────┘
                                                         │
                                                         v
                                                    ┌──────────┐
                                                    │ Send to  │
                                                    │ Voice    │
                                                    │ Layer    │
                                                    └────┬─────┘
                                                         │
                                                         v
                                                    ┌──────────┐
                                                    │ Log      │
                                                    │ Decision │
                                                    └──────────┘
```

### 12.2 Emergency Response Activity

```
┌──────────────┐
│ Fall Detected│
│ (IMU > 2.5g) │
└──────┬───────┘
       │
       v
┌──────────────┐
│ Confirm with │
│ IMU + AI:    │
│ orientation  │
│ change +     │
│ no motion    │
└──────┬───────┘
       │
       v
┌─────────────────┐     ┌────────────────────┐
│ Level 3 Hazard  │────▶│ Activate Emergency │
│ Push to Queue   │     │ Mode               │
└─────────────────┘     └──────────┬──────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              v                    v                    v
     ┌───────────────┐   ┌──────────────────┐   ┌──────────────┐
     │ Play Alert    │   │ Send SMS to      │   │ Enable GPS   │
     │ Tone + Voice  │   │ Emergency Contact│   │ Tracking at  │
     │ "Emergency    │   │ with GPS coords  │   │ 1 Hz         │
     │  detected"    │   └──────────────────┘   └──────────────┘
     └───────┬───────┘
             │
             v
     ┌──────────────────────────────────────────────────┐
     │              WAIT FOR USER RESPONSE              │
     │  ┌──────────────┐                               │
     │  │ Listen for:  │                               │
     │  │ "I'm okay"   │───▶ Cancel emergency          │
     │  │ "Help"       │───▶ Keep active, remind after │
     │  │              │      30 seconds               │
     │  │ No response  │───▶ After 5 min: escalate     │
     │  │  for 5 min   │    → Call emergency services  │
     │  └──────────────┘                               │
     └──────────────────────────────────────────────────┘
             │
             v
     ┌──────────────────┐
     │ Return to        │
     │ previous mode    │
     └──────────────────┘
```

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior Systems Engineer | Initial draft |

---

*End of Document — DEC-ENG-001*
