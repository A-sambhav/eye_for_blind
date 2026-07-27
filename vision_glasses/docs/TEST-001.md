# Testing Strategy Document

**Document ID:** TEST-001

**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients

**Author:** Senior QA Engineer

**Revision:** 0.1

**Date:** 2026-07-27

---

## Table of Contents

1. Executive Summary
2. Test Strategy Overview
3. Unit Testing
4. Integration Testing
5. Hardware Testing
6. Sensor Testing
7. AI Testing
8. Navigation Testing
9. Voice Testing
10. Power Testing
11. Battery Testing
12. Thermal Testing
13. Latency Testing
14. Stress Testing
15. Field Testing
16. Accessibility Testing
17. Test Cases
18. Validation Criteria
19. Performance Metrics
20. Acceptance Criteria

---

## 1. Executive Summary

The testing strategy follows a V-model approach: unit tests for individual modules, integration tests for module interactions, system tests for end-to-end scenarios, and field trials with target users. Tests are automated at the unit and integration levels, semi-automated at the system level, and manual for field trials. Continuous integration runs unit tests on every commit.

---

## 2. Test Strategy Overview

```
V-MODEL TESTING
┌─────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Requirements          →    Acceptance Tests (AC)               │
│  (SRS-001)                   (field trials, user validation)    │
│       │                              ▲                          │
│       v                              │                          │
│  Architecture           →    System Tests (ST)                  │
│  (Design Docs)                 (end-to-end scenarios)          │
│       │                              ▲                          │
│       v                              │                          │
│  Module Design          →    Integration Tests (IT)             │
│  (SW-ARCH, HW-ARCH)          (module interaction)               │
│       │                              ▲                          │
│       v                              │                          │
│  Implementation         →    Unit Tests (UT)                   │
│  (Code)                        (individual functions)           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘

TEST PYRAMID
         ┌───────┐
         │  E2E  │  ← Few (10-20 manual field tests)
        ┌┴───────┴┐
        │ System  │  ← Medium (50 automated system tests)
       ┌┴─────────┴┐
       │ Integration│ ← More (200 automated tests)
      ┌┴───────────┴┐
      │   Unit      │ ← Most (1000+ automated tests)
      └─────────────┘
```

---

## 3. Unit Testing

### 3.1 Framework

| Tool | Ceedling (Unity + CMock) |
|---|---|
| Language | C |
| Host platform | Linux (x86_64) |
| Target platform | ARM Cortex-M7 (simulated via unit test host) |
| Coverage tool | gcov / lcov |
| CI | GitHub Actions (on push, PR) |

### 3.2 Unit Test Modules

| Module | Test Count | Key Tests |
|---|---|---|
| drivers/i2c_imu_drv | 40 | init, read, self_test, error recovery, timeout |
| drivers/spi_gps_drv | 35 | init, read_ubx, parse_ubx, checksum, timeout |
| drivers/uart_esp_drv | 30 | init, send_at, parse_response, buffer overflow |
| drivers/i2s_audio_drv | 25 | init, start_dma, stop_dma, buffer_full |
| hal/camera_hal | 20 | init, start_stream, frame_callback, error |
| hal/gps_hal | 35 | parse_nmea, parse_ubx, coord_conversion, accuracy |
| hal/imu_hal | 30 | orientation_estimation, step_detection, fall_detection |
| middleware/message_bus | 50 | publish, subscribe, unsubscribe, priority, queue_full |
| middleware/timer_service | 20 | create_timer, cancel_timer, overflow |
| ai/object_detector | 25 | detection_parsing, nms, confidence_filtering |
| ai/depth_processor | 30 | edge_detection, distance_estimation, zone_analysis |
| decision/priority_queue | 40 | push, pop, peek, ordering, dedup, cooldown |
| decision/state_machine | 60 | all_transitions, invalid_transitions, reset |
| decision/context_manager | 45 | all context fields, update, edge cases |
| navigation/gps_navigator | 35 | haversine, bearing, waypoint_progress, turn_detection |
| navigation/imu_navigator | 30 | step_detect, pdr_update, drift, landing_correction |
| navigation/obstacle_avoider | 35 | zone_classification, path_selection, multi_obstacle |
| voice/intent_parser | 40 | all intents, entity_extraction, edge_cases |
| voice/wake_word | 20 | confidence_threshold, false_accept_rejection |
| voice/tts_engine | 25 | phoneme_lookup, concatenation, prosody |
| database/flash_storage | 30 | read, write, crc, wear_leveling, full_storage |
| database/config_manager | 25 | load, save, validate, defaults, corruption |
| **Total** | **~800** | |

### 3.3 Unit Test Example

```c
// test_priority_queue.c

#include "unity.h"
#include "priority_queue.h"

static priority_queue_t q;

void setUp(void) {
    priority_queue_init(&q);
}

void tearDown(void) {
    // No cleanup needed
}

void test_push_and_pop_highest(void) {
    priority_queue_entry_t e1 = {
        .priority = PRIORITY_ROUTINE,
        .event_type = EVENT_NAV_UPDATE,
        .message = "Continue straight",
        .timestamp_ms = 1000
    };
    priority_queue_entry_t e2 = {
        .priority = PRIORITY_CRITICAL,
        .event_type = EVENT_FALL,
        .message = "Fall detected",
        .timestamp_ms = 1005
    };

    TEST_ASSERT_EQUAL(0, priority_queue_push(&q, &e1));
    TEST_ASSERT_EQUAL(0, priority_queue_push(&q, &e2));

    priority_queue_entry_t out;
    TEST_ASSERT_EQUAL(0, priority_queue_pop_highest(&q, &out));
    TEST_ASSERT_EQUAL(PRIORITY_CRITICAL, out.priority);
    TEST_ASSERT_EQUAL(EVENT_FALL, out.event_type);
}

void test_deduplication(void) {
    priority_queue_entry_t e1 = {
        .priority = PRIORITY_IMPORTANT,
        .event_type = EVENT_OBSTACLE,
        .message = "Chair, 1.5m",
        .timestamp_ms = 1000,
        .distance_m = 1.5f
    };
    priority_queue_entry_t e2 = {
        .priority = PRIORITY_IMPORTANT,
        .event_type = EVENT_OBSTACLE,
        .message = "Chair, 1.5m",
        .timestamp_ms = 1005,
        .distance_m = 1.5f
    };

    priority_queue_push(&q, &e1);
    TEST_ASSERT_TRUE(is_duplicate(&e2));  // Same type, same distance, < 10s
}
```

---

## 4. Integration Testing

### 4.1 Integration Test Scenarios

| Test ID | Scenario | Modules Involved | Duration |
|---|---|---|---|
| IT-001 | IMU → Decision: Fall detection | imu_task → decision_task → audio_out | Automated |
| IT-002 | Camera → AI → Decision: Obstacle | camera_task → ai_task → decision → voice | Automated |
| IT-003 | GPS → Nav → Decision: Turn alert | gps_task → nav_task → decision → voice | Automated |
| IT-004 | Mic → Voice → Decision: Command | audio_in → voice → decision → nav | Automated |
| IT-005 | BMS → Battery → Decision: Low power | bms → battery_task → decision → shutdown | Automated |
| IT-006 | ESP32 → WiFi: OTA update | wifi_task → flash_storage → bootloader | Semi-auto |
| IT-007 | Full pipeline: Walk to destination | All modules | Manual |
| IT-008 | Full pipeline: Emergency fall | All safety modules | Manual |

### 4.2 Integration Test Environment

```
Host PC ─── USB ─── Arduino UNO Q ─── All peripherals connected
                          │
                    Mock sensors:
                    - IMU simulator (I2C commands via USB)
                    - GPS simulator (NMEA/UBX via USB)
                    - Camera simulator (test images via USB)
                    - Audio simulator (WAV files via SD card)
```

---

## 5. Hardware Testing

| Test ID | Test | Method | Pass Criteria |
|---|---|---|---|
| HW-001 | Power-on self-test (POST) | Automated firmware | All peripherals report OK |
| HW-002 | I2C bus integrity | Write/read test pattern | 100% success, 10K iterations |
| HW-003 | SPI bus integrity | Write/read test pattern | 100% success, 10K iterations |
| HW-004 | USB camera enumeration | Plug camera, check descriptor | Enumeration < 1s |
| HW-005 | Audio loopback | Play → record → compare | THD < 1% |
| HW-006 | Button response | Press 1000 times | 100% detection |
| HW-007 | LED functionality | All colors, brightness | Visible in daylight |
| HW-008 | Battery charge/discharge | Full cycle 3× | Capacity ≥ 90% rated |
| HW-009 | BMS protection | Overcurrent, undervoltage | Disconnect within 10 ms |
| HW-010 | Drop test | 1 m onto concrete, 6 orientations | No damage, continues operation |

---

## 6. Sensor Testing

| Sensor | Test ID | Test | Pass Criteria |
|---|---|---|---|
| IMU | SNS-001 | Accelerometer accuracy | ±0.1 m/s² at 1g |
| IMU | SNS-002 | Gyroscope drift | < 1°/s after 1 min (uncorrected) |
| IMU | SNS-003 | Magnetometer calibration | Compass accuracy ±5° after calibration |
| IMU | SNS-004 | Fall detection sensitivity | 100% detection at ≥ 2.5g impact |
| GPS | SNS-005 | Cold start TTFF | < 60 s |
| GPS | SNS-006 | Hot start TTFF | < 5 s |
| GPS | SNS-007 | Position accuracy | < 2.5 m CEP open sky |
| GPS | SNS-008 | Position accuracy urban | < 10 m CEP (urban canyon) |
| Depth | SNS-009 | Depth accuracy 0.5-3 m | ±5 cm |
| Depth | SNS-010 | Depth accuracy 3-8 m | ±15 cm |
| Depth | SNS-011 | Obstacle detection range | ≥ 5 m for 30 cm objects |
| Microphone | SNS-012 | SNR | ≥ 64 dBA |

---

## 7. AI Testing

| Test ID | Test | Method | Pass Criteria |
|---|---|---|---|
| AI-001 | Object detection accuracy | 1000 labeled test images | mAP@0.5 ≥ 85% |
| AI-002 | Object detection latency | 1000 frames timed | ≤ 120 ms per inference |
| AI-003 | False positive rate | 1000 frames without objects | ≤ 5 false alarms per 1000 frames |
| AI-004 | Face recognition accuracy | 20 enrolled faces × 10 trials | ≥ 95% top-1 |
| AI-005 | Face false accept rate | 50 unknown faces | ≤ 1 false accept |
| AI-006 | Text reading accuracy | 500 text samples | CER ≤ 5% |
| AI-007 | Scene classification accuracy | 500 images, 20 classes | ≥ 90% top-1 |
| AI-008 | Wake word detection | 500 wake + 5000 non-wake | FA ≤ 0.1/hr, FR ≤ 1% |
| AI-009 | Command recognition | 500 commands × 25 types | ≥ 95% accuracy |
| AI-010 | Model load time | Cold boot | ≤ 200 ms per model |

---

## 8. Navigation Testing

| Test ID | Test | Method | Pass Criteria |
|---|---|---|---|
| NAV-001 | Route following accuracy | Walk 1 km route, compare to GPS log | Within 10 m of route 95% of time |
| NAV-002 | Turn detection | 50 turns at intersections | ≥ 95% detected within 10 m |
| NAV-003 | Off-route detection | Deliberately deviate 50 m | Detected within 10 s |
| NAV-004 | Re-route time | Off-route → new guidance | ≤ 5 s |
| NAV-005 | Indoor PDR drift | Walk 100 m indoors | Error < 10 m with 1 landmark |
| NAV-006 | Stair detection | 20 staircases (10 up, 10 down) | ≥ 90% detection |
| NAV-007 | Door detection | 50 doors | ≥ 85% detection |
| NAV-008 | Obstacle avoidance | 50 obstacles on path | 100% collision alerts before 1 m |
| NAV-009 | Crosswalk detection | 20 crosswalks | ≥ 80% detection |

---

## 9. Voice Testing

| Test ID | Test | Method | Pass Criteria |
|---|---|---|---|
| VOICE-001 | Wake word detection (quiet) | 100 wake word utterances | ≥ 99% detection |
| VOICE-002 | Wake word detection (65 dB) | 100 wake word utterances | ≥ 95% detection |
| VOICE-003 | Wake word false accept | 10 hours non-wake audio | ≤ 1 false accept |
| VOICE-004 | Command accuracy (quiet) | 500 commands, 25 intents | ≥ 98% |
| VOICE-005 | Command accuracy (65 dB) | 500 commands, 25 intents | ≥ 90% |
| VOICE-006 | Noise reduction | Record at 65 dB ambient | SNR improvement ≥ 8 dB |
| VOICE-007 | TTS intelligibility | 100 phrases, 5 listeners | ≥ 95% word recognition |
| VOICE-008 | End-to-end latency | Wake word → response audio | ≤ 200 ms |
| VOICE-009 | Multi-turn dialogue | 20 multi-turn conversations | 100% correct state transitions |

---

## 10. Power Testing

| Test ID | Test | Method | Pass Criteria |
|---|---|---|---|
| PWR-001 | Active power consumption | Measure current with all sensors active | ≤ 4.5 W |
| PWR-002 | Idle power consumption | No navigation, object detection at 2 fps | ≤ 1.5 W |
| PWR-003 | Sleep power consumption | Deep sleep, IMU wake | ≤ 0.05 W |
| PWR-004 | Battery life (active) | Continuous use, all sensors | ≥ 2.5 hours |
| PWR-005 | Battery life (mixed use) | 4 hrs walking, 2 hrs idle, 2 hrs standby | ≥ 8 hours |
| PWR-006 | Charge time | 0% to 80% | ≤ 90 minutes |
| PWR-007 | Charge time | 0% to 100% | ≤ 2.5 hours |

---

## 11. Battery Testing

| Test ID | Test | Method | Pass Criteria |
|---|---|---|---|
| BAT-001 | Capacity | Full discharge at 0.5C | ≥ 90% rated (2700 mAh) |
| BAT-002 | Voltage accuracy | Measure at ADC vs multimeter | ±50 mV |
| BAT-003 | Level estimation | Compare to coulomb counter | ±5% accuracy |
| BAT-004 | Under-voltage cutoff | Discharge to 3.0V | BMS disconnects |
| BAT-005 | Over-voltage protection | Charge to 4.25V | BMS disconnects charger |
| BAT-006 | Over-temperature cutoff | Heat battery to 60°C | Disconnect within 1°C |

---

## 12. Thermal Testing

| Test ID | Test | Method | Pass Criteria |
|---|---|---|---|
| TH-001 | Skin contact temperature | Operate 1 hour, measure surface | < 43°C at all points |
| TH-002 | Component temperature | Operate 1 hour, measure PCB | < 60°C at hotspots |
| TH-003 | Battery temperature | Charge + discharge simultaneously | < 55°C |
| TH-004 | Cold start | -10°C environment | Normal operation within 2 min |
| TH-005 | Hot start | 50°C environment | Normal operation, reduced FPS |

---

## 13. Latency Testing

| Test ID | Test | Target | Method |
|---|---|---|---|
| LAT-001 | Obstacle detection → audio | ≤ 200 ms | Automated timestamp capture |
| LAT-002 | Face recognition → name | ≤ 2 s | Timed from face appearance |
| LAT-003 | Wake word → command accept | ≤ 800 ms | Voice command loop |
| LAT-004 | GPS fix (cold) | ≤ 60 s | Scripted GPS simulation |
| LAT-005 | GPS fix (hot) | ≤ 5 s | Scripted GPS simulation |
| LAT-006 | System boot | ≤ 15 s | Power-on to "System ready" |
| LAT-007 | Shutdown | ≤ 70 ms | Shutdown trigger to off |
| LAT-008 | Fall detection | ≤ 500 ms | IMU impulse+decision |
| LAT-009 | I2C IMU read | ≤ 1 ms | Logic analyzer |

---

## 14. Stress Testing

| Test ID | Test | Method | Duration |
|---|---|---|---|
| STR-001 | Continuous operation | Run all features at max rate | 24 hours |
| STR-002 | Memory stress | Allocate/free repeatedly | 1 hour |
| STR-003 | Flash wear | Write/erase cycles on test sector | Continuous |
| STR-004 | USB camera reconnect | Plug/unplug 1000× | 1 hour |
| STR-005 | BLE reconnect | Disconnect/reconnect 100× | 30 min |
| STR-006 | WiFi reconnect | Disconnect/reconnect 100× | 30 min |
| STR-007 | Voice command flood | 100 commands in 10 seconds | 1 min |
| STR-008 | Obstacle flood | 50 objects detected simultaneously | Stress test |
| STR-009 | Multi-task contention | All tasks at max rate | 1 hour |
| STR-010 | Watchdog recovery | Force crash, measure reboot time | 20 cycles |

---

## 15. Field Testing

### 15.1 Field Trial Protocol

| Phase | Duration | Participants | Focus |
|---|---|---|---|
| Alpha | 2 weeks | 5 sighted engineers | Basic functionality, safety |
| Beta | 4 weeks | 5 visually impaired | Usability, real-world issues |
| Beta-Alz | 4 weeks | 5 Alzheimer's patients + caregivers | Safety, wandering prevention |
| Gamma | 8 weeks | 15 visually impaired + 10 Alzheimer's | Full validation |
| Clinical | 12 weeks | 50 users total | Regulatory data collection |

### 15.2 Field Test Scenarios

| Scenario | Description | Duration | Success Criteria |
|---|---|---|---|
| Daily commute | Walk to bus stop, ride, walk to work | 2 hours | No safety incidents |
| Grocery shopping | Navigate store, find items | 1 hour | Complete without collision |
| Park walk | Navigate park paths, avoid obstacles | 1 hour | Complete route safely |
| Home living | Move around home, avoid furniture | 4 hours | No falls or collisions |
| Hospital visit | Navigate hospital corridors, find room | 2 hours | Arrive at correct room |
| Emergency test | Simulated fall, check response | 30 min | Emergency contact notified |
| Wandering prevention | Alzheimer's patient approaches geo-fence | 1 hour | Alert triggered before exit |

---

## 16. Accessibility Testing

| Test ID | Test | Method | Pass Criteria |
|---|---|---|---|
| ACC-001 | Setup without vision | User unboxes and wears device | < 10 min with no vision |
| ACC-002 | First navigation | User walks 50 m with audio guidance | Completes without collision |
| ACC-003 | Voice command learning | New user learns 5 commands | < 5 min to proficiency |
| ACC-004 | Alzheimer's orientation | User receives orientation prompt | Understands "You are at home" |
| ACC-005 | Silent mode adequacy | User navigates with haptic only | Perceives all obstacle alerts |
| ACC-006 | Volume accessibility | User in hearing range | Understands speech at max volume |
| ACC-007 | Button accessibility | User with limited dexterity | Presses button successfully |
| ACC-008 | Cognitive load | User walks while receiving guidance | Does not feel overwhelmed |

---

## 17. Test Cases (Sample)

### 17.1 TC-001: Obstacle Detection and Alert

```
ID:         TC-001
Title:      Obstacle detection triggers audio alert
Priority:   Critical
Precondition: System booted, user in walking mode

Steps:
  1. Place chair 1.5 m in front of user
  2. Wait 3 seconds
  3. Remove chair
  4. Repeat with chair at 0.5 m

Expected Results:
  1. Within 200 ms: "Chair, 1.5 meters, center" (or similar)
  2. Within 200 ms: "Chair, 0.5 meters. Stop." (urgent tone)
  3. When chair removed: No further alert (cooldown)
```

### 17.2 TC-002: Navigation Turn Guidance

```
ID:         TC-002
Title:      Turn-by-turn guidance for a route
Priority:   High
Precondition: Route loaded, GPS fix active

Steps:
  1. Start navigation to waypoint 100 m away
  2. Walk straight for 50 m
  3. Approach a right turn at 50 m mark
  4. Execute right turn

Expected Results:
  1. "Navigating to {destination}. 100 meters."
  2. "Continue straight" (once, not repeated)
  3. "In 50 meters, turn right"
  4. "In 20 meters, turn right"
  5. "Turn right now"
  6. "Continue straight on {street}"
```

### 17.3 TC-003: Fall Detection and Emergency

```
ID:         TC-003
Title:      Fall detection triggers emergency protocol
Priority:   Critical
Precondition: System worn and active

Steps:
  1. Simulate fall: rapid acceleration > 2.5g followed by still
  2. Wait 1 second
  3. Say "I'm okay"

Expected Results:
  1. Within 500 ms: "Emergency detected" + haptic SOS
  2. SMS sent to emergency contact with GPS coordinates
  3. "Are you okay? Say 'I'm okay' to cancel."
  4. After "I'm okay": "Emergency cancelled."
```

---

## 18. Validation Criteria

| Requirement | Validation Method | Criteria |
|---|---|---|
| FR-001 (obstacle detection) | Test TC-001 | 100% detection at ≥ 0.5 m |
| FR-002 (GPS navigation) | Field test | Arrive within 10 m of destination |
| FR-003 (alert system) | Test TC-001 | Alert within 200 ms |
| FR-007 (navigation) | Field test | Turn guidance accurate at all distances |
| FR-018 (fall detection) | Test TC-003 | Detection within 500 ms |
| NFR-001 (latency) | LAT-001 | ≤ 200 ms end-to-end |
| NFR-004 (MTBF) | STR-001 | ≥ 2000 hours demonstrated |
| PR-001 (object detection) | AI-002 | ≤ 120 ms inference |
| PR-008 (battery) | PWR-005 | ≥ 8 hours mixed use |
| SFR-001 (audio level) | Audio measurement | ≤ 85 dB SPL |
| SCR-001 (encryption) | Code review | AES-256 verified |

---

## 19. Performance Metrics

| Metric | Target | Measurement Method | Frequency |
|---|---|---|---|
| Object detection accuracy | mAP ≥ 85% | AI-001 | Per model update |
| Inference latency | ≤ 120 ms | AI-002 | Per commit |
| Obstacle alert latency | ≤ 200 ms | LAT-001 | Per release |
| Wake word detection | ≥ 99% (quiet) | VOICE-001 | Per release |
| Command accuracy | ≥ 98% (quiet) | VOICE-004 | Per release |
| GPS position error | ≤ 2.5 m CEP | SNS-007 | Per hardware revision |
| Fall detection | ≤ 500 ms | TC-003 | Per release |
| System boot time | ≤ 15 s | LAT-006 | Per release |
| Battery life (mixed) | ≥ 8 hours | PWR-005 | Per hardware revision |
| Audio output level | ≤ 85 dB SPL | — | Per hardware revision |
| MTBF | ≥ 2000 hours | STR-001 | Continuous |
| Memory usage | ≤ 90% SRAM | Static analysis | Per commit |
| CPU usage | ≤ 95% peak | STR-009 | Per release |

---

## 20. Acceptance Criteria

The system is accepted when all of the following are met:

| ID | Criterion | Verification | Status |
|---|---|---|---|
| AC-001 | All FR-001 through FR-026 pass | Test report | |
| AC-002 | All NFR-001 through NFR-012 satisfy targets | Test report | |
| AC-003 | All PR-001 through PR-014 meet thresholds | Performance report | |
| AC-004 | All HR-001 through HR-013 functional | Hardware inspection | |
| AC-005 | All SR-001 through SR-010 implemented | Code review | |
| AC-006 | All SFR-001 through SFR-010 pass safety | Safety certificate | |
| AC-007 | All SCR-001 through SCR-007 pass security | Penetration test | |
| AC-008 | All PVR-001 through PVR-007 pass privacy | Privacy review | |
| AC-009 | All AR-001 through AR-008 pass accessibility | User study ≥ 10 participants | |
| AC-010 | MTBF ≥ 2000 hours | Life test | |
| AC-011 | Battery life ≥ 8 hours mixed | PWR-005 | |
| AC-012 | 1 m drop test pass | HW-010 | |
| AC-013 | Obstacle alert ≤ 200 ms | LAT-001 | |
| AC-014 | 100% of unit tests pass | CI pipeline | |
| AC-015 | 100% of integration tests pass | CI pipeline | |
| AC-016 | Field trial completed with ≤ 5 safety incidents | Field report | |
| AC-017 | All critical and high bugs resolved | Bug tracker | |

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior QA Engineer | Initial draft |

---

*End of Document — TEST-001*
