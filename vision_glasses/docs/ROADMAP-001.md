# Development Roadmap

**Document ID:** ROADMAP-001
**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients
**Author:** Senior Program Manager
**Revision:** 0.2
**Date:** 2026-07-28

---

## Table of Contents

1. Executive Summary
2. Timeline Overview
3. Phase 1: Hardware Abstraction Layer (Months 1-2)
4. Phase 2: Middleware (Months 2-3)
5. Phase 3: Perception (Months 3-5)
6. Phase 4: Decision Engine (Months 4-6)
7. Phase 5: Navigation (Months 5-7)
8. Phase 6: Voice & Accessibility (Months 6-9)
9. Phase 7: System Integration (Months 8-10)
10. Phase 8: Optimization & Field Testing (Months 10-14)
11. Phase 9: Certification & Production (Months 12-18)
12. Gantt Chart
13. Milestones
14. Deliverables
15. Dependencies
16. Risk Analysis
17. Resource Planning

---

## 1. Executive Summary

The development roadmap spans 18 months from concept to production-ready TRL 7 system. The project follows 9 phases with overlapping timelines, prioritized bottom-up: hardware drivers first, then middleware infrastructure, then perception AI, then decision logic, then navigation, then voice, then integration, optimization, and finally certification. The team requires 5-7 FTE engineers across embedded, AI, and QA disciplines.

**Architect's implementation order:**
1. HAL (drivers)
2. Middleware (message bus, config, logging, diagnostics)
3. Perception (Edge Impulse, depth, object tracking)
4. Decision Engine (hazard priority, context management)
5. Navigation (path planning, obstacle avoidance)
6. Voice & Accessibility (STT, TTS, reminders)
7. System Integration (all modules end-to-end)
8. Optimization & Field Testing (latency, power, user trials)

---

## 2. Timeline Overview

```
Month:   1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18
         ┌─────────────────────────────────────────────────────────┐
HAL      │████████████│                                            │
Middleware│   ████████████│                                         │
Perception│      ████████████████│                                  │
DecEng   │           ████████████████│                              │
Nav      │                ████████████████│                         │
Voice    │                     ██████████████████████│              │
Integ    │                          ████████████████████│           │
Opt+Field│                                ████████████████████████  │
Cert     │                                         ████████████████│
         ┌─────────────────────────────────────────────────────────┐
```

---

## 3. Phase 1: Hardware Abstraction Layer (Months 1-2)

### 3.1 Purpose
Implement all low-level hardware drivers for sensors and actuators. Every driver provides a clean HAL API so upper layers never touch registers directly.

### 3.2 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| PCB layout and fabrication | HW Engineer | 1-4 | PCB v1.0 |
| Component procurement | Supply Chain | 1-3 | BOM fulfilled |
| Board assembly | CM | 3-5 | 10 assembled boards |
| Power-on verification | HW Engineer | 5-6 | Power rails verified |
| Bootloader flashing | FW Engineer | 5-6 | Arduino Q boots |
| Camera driver (OV5640, CSI-2) | FW Engineer | 5-8 | 640x480 @ 30 FPS raw frames |
| Depth camera driver | FW Engineer | 6-8 | Depth stream at 30 FPS |
| IMU driver (BMI270, I2C) | FW Engineer | 5-7 | Accel + gyro @ 100 Hz |
| GPS driver (ZED-F9P, UART) | FW Engineer | 6-8 | NMEA/UBX parsing, 10 Hz |
| Battery fuel gauge (BQ27750, I2C) | FW Engineer | 7-8 | SoC, voltage, current, temp |
| Audio I2S driver (microphone + speaker) | FW Engineer | 7-9 | 16 kHz capture + playback |
| Button + vibration motor GPIO | FW Engineer | 8-9 | Interrupt-driven input, PWM haptic |
| HAL API documentation | FW Engineer | 9-10 | All driver APIs documented |
| Driver unit tests | FW Engineer | 9-10 | All HAL tests pass |

### 3.3 Dependencies
- None (first phase)

---

## 4. Phase 2: Middleware (Months 2-3)

### 4.1 Purpose
Build the infrastructure layer that all modules depend on: inter-task message bus, persistent configuration, structured logging, and system diagnostics. Every module built after this phase uses these services.

### 4.2 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| FreeRTOS queue wrapper (message bus) | FW Engineer | 8-10 | Typed queues with timeout + ISR support |
| Inter-module communication matrix | FW Engineer | 9-10 | 26 message routes configured |
| Configuration Manager | FW Engineer | 9-11 | YAML/binary config load/save, typed getters/setters |
| Logging Manager (ring buffer + DB flush) | FW Engineer | 10-12 | 4-level logging, rate limiting, 256-entry ring |
| Diagnostics Manager | FW Engineer | 11-13 | Module health polling, self-tests, perf collection |
| Watchdog Manager (HW + SW) | FW Engineer | 11-13 | IWDG + per-task SW watchdog |
| System Manager (boot/shutdown/mode) | FW Engineer | 12-14 | Init/shutdown sequences, mode management |
| Middleware integration tests | QA Engineer | 13-14 | All middleware passes stress tests |

### 4.3 Dependencies
- Phase 1 (HAL) complete — middleware tests require real hardware

---

## 5. Phase 3: Perception (Months 3-5)

### 5.1 Purpose
Implement all AI perception pipelines: Edge Impulse runtime integration, monocular depth estimation, object detection, and object tracking across frames.

### 5.2 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| Dataset collection & labeling | AI Engineer | 9-12 | 10K labeled images (indoor/outdoor) |
| Edge Impulse EON runtime integration | FW Engineer | 12-14 | Model load, arena mgmt, inference API |
| Monocular depth model training | AI Engineer | 12-14 | Depth estimation, MAE < 15% |
| Depth Processing module | FW Engineer | 13-15 | Greyscale, inference, temporal filter, downsample |
| Object detection model training | AI Engineer | 13-15 | FOMO/SSD, mAP >= 85% |
| Object Detection module | FW Engineer | 14-16 | Preprocess, infer, NMS, depth fusion |
| Scene Understanding module | FW Engineer | 15-17 | Scene graph, ground plane, free space, hazard ID |
| Object Tracking module (SORT + Kalman) | FW Engineer | 16-18 | IoU association, velocity estimation, stale removal |
| Model quantization (int8) | AI Engineer | 16-18 | All models <= 2 MB, EON-compiled |
| Perception end-to-end test | QA Engineer | 17-19 | Camera to tracking pipeline at 10+ FPS |

### 5.3 Dependencies
- Phase 1 (camera, depth camera HAL)
- Phase 2 (message bus, config, logging)

---

## 6. Phase 4: Decision Engine (Months 4-6)

### 6.1 Purpose
Implement the central arbitration module that receives inputs from perception, context, and voice, prioritizes events, and selects actions (speech, navigation override, emergency response). Built before Navigation so Navigation can receive commands from it.

### 6.2 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| Event queue + priority sorter | SW Engineer | 16-18 | Binary max-heap, 16-entry |
| Context Manager | SW Engineer | 17-19 | Scene history, user state, location integration |
| Hazard Priority Manager | SW Engineer | 18-20 | Severity scoring, TTI, dedup, priority queue |
| Decision Engine state machine | SW Engineer | 19-21 | Collect, prioritize, select, execute cycle |
| Action executors (speak, nav override, alert) | SW Engineer | 20-22 | SpeechReq, NavOverride, EmergencyMsg dispatch |
| Speech throttle + cooldown | SW Engineer | 21-22 | Max N announcements/minute |
| Decision Engine unit tests | QA Engineer | 22-23 | All 6 input types, priority, throttle, timeout |

### 6.3 Dependencies
- Phase 3 (perception provides hazard list, scene descriptions, tracked objects)

---

## 7. Phase 5: Navigation (Months 5-7)

### 7.1 Purpose
Implement path planning, turn-by-turn guidance, and obstacle avoidance. Navigation receives commands from the Decision Engine and reports status back. Built after Decision Engine so the priority architecture is already in place.

### 7.2 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| GPS-IMU position fusion | Nav Engineer | 20-22 | Weighted complementary filter |
| Waypoint + route management | Nav Engineer | 21-23 | Waypoint list, progress, arrival detection |
| Path Planner (A* on traversability grid) | Nav Engineer | 22-24 | 160x160 grid, A*, path smoothing |
| Obstacle Avoidance module | Nav Engineer | 23-25 | Clearance check, avoidance vector, resume |
| Navigation instruction generator | Nav Engineer | 24-26 | Turn, continue, arrived speech cues |
| Navigation Engine integration | FW Engineer | 25-27 | NavPlanMsg, NavOverrideMsg handling |
| Nav override from Decision Engine | FW Engineer | 25-27 | Hazard avoidance, reroute on command |
| Navigation unit + integration tests | QA Engineer | 26-28 | Route, replan, override, off-route detection |

### 7.3 Dependencies
- Phase 1 (GPS, IMU HAL)
- Phase 3 (depth map for free space, object tracking for obstacles)
- Phase 4 (receives NavOverrideMsg from Decision Engine)

---

## 8. Phase 6: Voice & Accessibility (Months 6-9)

### 8.1 Purpose
Implement voice interaction (wake word, command recognition, speech synthesis), reminder management, memory assistance, and emergency detection. This phase delivers the user-facing accessibility features.

### 8.2 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| Wake word model + integration | AI Engineer | 24-26 | Keyword spotting, FA < 0.1/hr |
| Voice command model (25 intents) | AI Engineer | 25-27 | Command recognition, accuracy > 90% |
| Voice Recognition module | FW Engineer | 26-28 | VAD, MFCC, inference loop, cmd dispatch |
| TTS engine integration (Piper) | FW Engineer | 27-29 | Text to 16 kHz PCM, queue, play |
| Speech Synthesis module | FW Engineer | 28-30 | Priority queue, interrupt, volume, mute |
| Reminder Manager | SW Engineer | 28-30 | Time + location reminders, recurrence, DB persistence |
| Memory Assistant (face recognition) | AI + SW | 29-32 | Face enrollment, matching, memory recall |
| Emergency Manager (fall + wandering) | FW Engineer | 30-33 | IMU-based fall detection, geofence wandering, SOS button |
| Voice + accessibility unit tests | QA Engineer | 32-34 | All voice, reminder, emergency tests pass |

### 8.3 Dependencies
- Phase 1 (audio I2S driver for mic + speaker)
- Phase 2 (logging, config)
- Phase 3 (face recognition model)
- Phase 4 (Decision Engine receives VoiceCmd, EmergencyMsg, ReminderMsg)

---

## 9. Phase 7: System Integration (Months 8-10)

### 9.1 Purpose
Connect all 29 modules into a single firmware image. Validate end-to-end data flow from camera to speech output. Verify initialization order, shutdown order, and inter-module communication.

### 9.2 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| Database Manager (SQLite) | FW Engineer | 30-32 | Schema creation, CRUD, backup, WAL |
| Application Manager | SW Engineer | 31-33 | Blind assist + Alzheimer use cases, user prefs |
| System mode switching | FW Engineer | 32-34 | Mode transitions, module reconfiguration |
| Full init/shutdown sequence validation | FW Engineer | 33-35 | Boot to RUNNING < 3 s, shutdown < 1 s |
| End-to-end comm matrix verification | QA Engineer | 33-35 | All 26 message routes verified |
| Integration test pass | QA Engineer | 34-36 | 200+ integration tests pass |
| Stress test (24-hour continuous) | QA Engineer | 35-37 | No crashes, no watchdog resets |

### 9.3 Dependencies
- Phases 1-6 complete (all modules implemented)

---

## 10. Phase 8: Optimization & Field Testing (Months 10-14)

### 10.1 Purpose
Profile and optimize the system for latency, power, and memory. Conduct progressive field testing with sighted engineers, visually impaired users, and Alzheimer's patients.

### 10.2 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| CPU profiling + optimization | FW Engineer | 38-40 | CPU <= 90% at peak load |
| Memory optimization | FW Engineer | 38-40 | SRAM <= 90%, no fragmentation |
| Inference latency tuning | AI Engineer | 39-41 | All models within LLD time budgets |
| Power optimization | HW Engineer | 39-41 | 8-hour battery life target |
| Audio latency tuning | FW Engineer | 40-42 | End-to-end voice <= 500 ms |
| Boot time optimization | FW Engineer | 40-42 | Boot <= 3 s |
| Alpha test (5 sighted engineers) | QA Engineer | 44-46 | Bug reports + fixes |
| Beta test (5 VI users) | UX Researcher | 46-48 | Usability + safety feedback |
| Beta test (5 Alzheimer's patients) | UX Researcher | 46-48 | Cognitive assistance feedback |
| Gamma test (25 users) | QA + UX | 48-52 | Validation dataset |
| Bug fixes + iteration | All Engineers | 44-54 | All field issues resolved |
| Final code review | Tech Lead | 50-52 | All modules reviewed |

### 10.3 Dependencies
- Phase 7 (integrated system ready for profiling and field use)

---

## 11. Phase 9: Certification & Production (Months 12-18)

### 11.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| FCC/CE certification | Compliance | 46-52 | Certification passes |
| ISO 13485 audit | Quality | 46-52 | QMS certification |
| IEC 62368-1 safety test | Compliance | 48-54 | Safety certification |
| BOM finalization | Supply Chain | 48-54 | BOM cost <= $150 |
| Production PCB v2.0 | HW Engineer | 50-56 | Manufacturing release |
| Production test jig | Manufacturing | 52-56 | Automated test fixture |
| Pilot production (100 units) | Manufacturing | 54-58 | 100 units manufactured |
| Clinical trial prep | Medical | 56-60 | IRB approval + protocol |

### 11.2 Dependencies
- Phase 8 (field trials inform final hardware/software revision)

---

## 12. Gantt Chart

```
Task                        M1  M2  M3  M4  M5  M6  M7  M8  M9  M10 M11 M12 M13 M14 M15 M16 M17 M18
────                        ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──

PHASE 1: HAL               [████████████]
PCB + procurement          [████]
Assembly                   [██]
POST                       [██]
Camera driver               [████]
Depth cam driver             [████]
IMU driver                  [████]
GPS driver                   [████]
Battery driver                [███]
Audio I2S                     [████]
Button + haptic                 [███]
HAL tests                         [██]

PHASE 2: Middleware              [████████████]
Message bus                     [████]
Config manager                     [████]
Logging manager                     [████]
Diagnostics                           [████]
Watchdog                               [████]
System manager                            [████]
Middleware tests                            [██]

PHASE 3: Perception                        [████████████████]
Dataset                                    [████]
EI runtime                                 [████]
Depth model                                [████]
Depth module                                 [████]
OD model                                    [████]
OD module                                     [████]
Scene Understanding                            [████]
Object Tracking                                  [████]
Quantization                                       [████]
Perception tests                                      [██]

PHASE 4: DecEng                                          [████████████████]
Event queue                                              [████]
Context mgr                                                [████]
Hazard priority                                             [████]
DecEng state machine                                          [████]
Action executors                                                [████]
Throttle                                                          [███]
DecEng tests                                                        [██]

PHASE 5: Navigation                                                      [████████████████]
GPS-IMU fusion                                                           [████]
Waypoint mgr                                                               [████]
Path Planner                                                                [████]
Obstacle Avoidance                                                            [████]
Nav instructions                                                                [████]
Nav integration                                                                  [████]
Nav tests                                                                           [██]

PHASE 6: Voice & Access                                                                  [████████████████████████]
Wake word model                                                                            [████]
Command model                                                                                [████]
Voice Recognition                                                                              [████]
TTS engine                                                                                      [████]
Speech Synthesis                                                                                  [████]
Reminder Manager                                                                                   [████]
Memory Assistant                                                                                      [██████]
Emergency Manager                                                                                       [██████]
Voice tests                                                                                                [██]

PHASE 7: Integration                                                                                            [████████████]
Database Manager                                                                                                 [████]
Application Manager                                                                                                [████]
Mode switching                                                                                                      [████]
Init/shutdown validation                                                                                              [███]
Comm matrix verification                                                                                               [███]
Integration tests                                                                                                       [████]
Stress test                                                                                                               [███]

PHASE 8: Opt & Field                                                                                                               [████████████████████]
CPU/memory opt                                                                                                                        [████]
Inference tuning                                                                                                                       [████]
Power opt                                                                                                                              [████]
Audio/boot latency                                                                                                                      [████]
Alpha trial (5)                                                                                                                          [████]
Beta trial (10)                                                                                                                            [██████]
Gamma trial (25)                                                                                                                              [██████]
Bug fixes                                                                                                                                    [████████]
Code review                                                                                                                                   [███]

PHASE 9: Cert & Prod                                                                                                                                   [████████████████]
FCC/CE                                                                                                                                                   [████]
ISO 13485                                                                                                                                                [████]
Safety                                                                                                                                                   [████]
PCB v2.0                                                                                                                                                  [████]
Prod test jig                                                                                                                                             [████]
Pilot prod (100)                                                                                                                                          [████]
Clinical prep                                                                                                                                             [████]
```

---

## 13. Milestones

| ID | Milestone | Date | Criteria |
|---|---|---|---|
| M-001 | HW Rev 1.0 assembled | Month 2 | All components soldered, power-on OK |
| M-002 | All HAL drivers working | Month 2.5 | Camera 30 FPS, IMU 100 Hz, GPS 10 Hz, audio I2S, BMS |
| M-003 | Middleware operational | Month 3 | Message bus, config, logging, diagnostics all passing tests |
| M-004 | Perception pipeline working | Month 5 | Camera-to-tracking at 10+ FPS, all models int8 quantized |
| M-005 | Decision Engine complete | Month 6 | All 6 input types prioritized, actions dispatched |
| M-006 | Navigation functional | Month 7 | Path planned, obstacle avoidance active, voice guidance |
| M-007 | Voice + Accessibility complete | Month 9 | Wake word, commands, TTS, reminders, fall detection E2E |
| M-008 | System integration complete | Month 10 | All 29 modules connected, 200+ integration tests pass |
| M-009 | Optimization complete | Month 11 | All performance targets met (CPU, memory, power, latency) |
| M-010 | Alpha field trial complete | Month 12 | Engineer feedback incorporated |
| M-011 | Beta field trial complete | Month 13 | User feedback incorporated |
| M-012 | Gamma field trial complete | Month 14 | Validation data collected |
| M-013 | Regulatory certification | Month 15 | FCC, CE, IEC 62368-1 passes |
| M-014 | Pilot production | Month 17 | 100 units manufactured |
| M-015 | TRL 7 achieved | Month 18 | System demonstrated in operational environment |

---

## 14. Deliverables

| Phase | Deliverable | Format | Recipient |
|---|---|---|---|
| 1 | PCB design files (Gerber, ODB++) | Electronic | Manufacturer |
| 1 | BOM + sourcing | Spreadsheet | Supply chain |
| 1 | 10 assembled test boards | Physical | Engineering |
| 1 | HAL driver source code + API docs | GitHub + Markdown | Engineering |
| 2 | Middleware source code (message bus, config, log, diag, wdog) | GitHub | Engineering |
| 2 | Middleware API documentation | Markdown | Engineering |
| 3 | Trained Edge Impulse models (4) | .eim files | Firmware |
| 3 | Perception module source code | GitHub | Engineering |
| 3 | Model training report | PDF | AI team |
| 4 | Decision Engine source code | GitHub | Engineering |
| 4 | State machine + priority documentation | PDF | Engineering |
| 5 | Navigation source code | GitHub | Engineering |
| 5 | Navigation test log | PDF | QA |
| 6 | Voice + accessibility source code | GitHub | Engineering |
| 6 | Dialogue flow diagrams | PDF | Engineering |
| 7 | Full system firmware image | .bin | Engineering |
| 7 | Integration test report | PDF | QA |
| 8 | Optimization report | PDF | Engineering |
| 8 | Performance benchmark | PDF | Engineering |
| 8 | Field trial report | PDF | Product |
| 8 | User feedback analysis | PDF | Product |
| 9 | Certification documents | PDF | Regulatory |
| 9 | Manufacturing test fixture | Physical | Manufacturing |
| 9 | 100 production units | Physical | Distribution |
| + | Low-Level Design Document (LLD-001) | Markdown | Engineering |

---

## 15. Dependencies

### 15.1 External Dependencies

| Dependency | Source | Lead Time | Risk |
|---|---|---|---|
| Arduino UNO Q SBC | Manufacturer | 4-6 weeks | Low (standard part) |
| OV5640 camera sensor | Distributor | 2-4 weeks | Low |
| BMI270 IMU | Distributor | 2-4 weeks | Low |
| ZED-F9P GPS | Distributor | 2-4 weeks | Low |
| BQ27750 fuel gauge | Distributor | 2-4 weeks | Low |
| Bone conduction transducer | Specialty supplier | 4-8 weeks | Medium |
| Custom PCB fab | PCB manufacturer | 2-3 weeks | Low |
| Edge Impulse Enterprise | Edge Impulse | Immediate | Low (SaaS) |
| FreeRTOS | Open source | Immediate | None |
| Piper TTS | Open source | Immediate | None |

### 15.2 Internal Dependencies

```
Phase 1 (HAL)       → Phase 2 (Middleware)
Phase 1 + 2         → Phase 3 (Perception)
Phase 3             → Phase 4 (Decision Engine)
Phase 4             → Phase 5 (Navigation)
Phase 1 + 3 + 4     → Phase 6 (Voice & Accessibility)
Phases 1-6          → Phase 7 (System Integration)
Phase 7             → Phase 8 (Optimization & Field Testing)
Phase 8             → Phase 9 (Certification & Production)
```

**Critical path:** Phase 1 → 2 → 3 → 4 → 5 → 7 → 8 → 9 = 18 months  
**Non-critical (parallel):** Phase 6 runs alongside Phase 5 with shared dependencies on Phases 1, 3, 4

---

## 16. Risk Analysis

| ID | Risk | Probability | Impact | Mitigation | Contingency |
|---|---|---|---|---|---|
| R-001 | Camera + depth processing exceeds 33 ms budget | Medium | High (latency) | Reduce FPS to 15, optimize inference | Accept 15 FPS with degraded experience |
| R-002 | Edge Impulse EON runtime cannot run all models | Low | High | Early prototype validation with EON | Fallback to TensorFlow Lite Micro |
| R-003 | IMU drift degrades indoor navigation | Medium | Medium | Use landmark correction from camera | Outdoor-only navigation in v1 |
| R-004 | Bone conduction audio too quiet in noisy environments | Medium | Medium | Higher-power transducer, adaptive volume | Add external earpiece option |
| R-005 | Flash endurance exceeded in field | Medium | High | Wear-leveling, batch writes, log pruning | Reduce log retention to 1 day |
| R-006 | FDA/MDR regulatory delay | Low | High | Early engagement with notified body | Delay launch 3-6 months |
| R-007 | Fall detection false positive rate exceeds 5% | Medium | Medium | Two-stage verify (impact + immobility) | Require voice confirmation |
| R-008 | Target users cannot use voice interface | Medium | Critical | Haptic + button primary fallback | Add companion app control |
| R-009 | Middleware message bus becomes bottleneck | Low | Medium | Profile queue depths early, increase as needed | Priority-boost critical queues |
| R-010 | BOM cost exceeds $150 | Medium | High | Component substitution, volume pricing | Accept $180 max |

### 16.1 Risk Matrix

```
Probability
   High    │               │               │               │
           │               │ R-005         │ R-001         │
   Medium  │               │ R-003, R-004  │ R-002, R-007  │
           │               │ R-010         │ R-008, R-009  │
   Low     │               │               │ R-006         │
           │               │               │               │
           ────────────────┴───────────────┴───────────────
               Low            Medium           High
                                   Impact
```

---

## 17. Resource Planning

### 17.1 Team Composition

| Role | FTE | Months Active | Lead Recruited By |
|---|---|---|---|
| Embedded Firmware Engineer | 2 | 1-18 | Month 1 |
| Senior AI/ML Engineer | 1 | 3-9 | Month 3 |
| Navigation Engineer | 1 | 5-7 | Month 5 |
| Hardware Engineer | 1 | 1-6, 12-18 | Month 1 |
| QA/Test Engineer | 1 | 3-18 | Month 3 |
| UX Researcher | 0.5 | 10-14 | Month 10 |
| SW Engineer (application) | 1 | 4-10 | Month 4 |
| Program Manager | 0.5 | 1-18 | Month 1 |
| **Total peak FTE** | **7** | **Months 6-10** | |

### 17.2 Budget Estimate

| Category | Cost | Notes |
|---|---|---|
| Engineering salaries (18 months) | $1,400,000 | 7 FTE average |
| Hardware (prototypes + test) | $50,000 | 10+ iterations |
| Edge Impulse Enterprise | $30,000 | 18 months |
| Certification (FCC, CE, IEC) | $80,000 | Including testing lab |
| Tooling + NRE for production | $50,000 | Injection mold + test jig |
| Pilot production (100 units) | $25,000 | $250/unit at low volume |
| Field trial logistics | $30,000 | Participant compensation, travel |
| Contingency (20%) | $333,000 | |
| **Total** | **$1,998,000** | |

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior Program Manager | Initial draft |
| 0.2 | 2026-07-28 | Senior Program Manager | Restructured to architect's implementation order: HAL → Middleware → Perception → DecEng → Nav → Voice → Integration → Optimization → Certification |

---

*End of Document — ROADMAP-001*
