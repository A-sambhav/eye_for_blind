# Development Roadmap

**Document ID:** ROADMAP-001

**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients

**Author:** Senior Program Manager

**Revision:** 0.1

**Date:** 2026-07-27

---

## Table of Contents

1. Executive Summary
2. Timeline Overview
3. Phase 1: Hardware Bring-up (Months 1-2)
4. Phase 2: Sensor Drivers (Months 2-3)
5. Phase 3: Edge Impulse (Months 3-5)
6. Phase 4: Navigation (Months 4-6)
7. Phase 5: Decision Engine (Months 5-7)
8. Phase 6: Voice Assistant (Months 6-8)
9. Phase 7: Database & Integration (Months 7-9)
10. Phase 8: Testing (Months 8-11)
11. Phase 9: Optimization (Months 10-12)
12. Phase 10: Field Trials (Months 11-14)
13. Phase 11: Certification & Production (Months 12-18)
14. Gantt Chart
15. Milestones
16. Deliverables
17. Dependencies
18. Risk Analysis
19. Resource Planning

---

## 1. Executive Summary

The development roadmap spans 18 months from concept to production-ready TRL 7 system. The project follows 11 phases with overlapping timelines to maximize engineering efficiency. Critical path items: AI model training + optimization (months 3-5) and field trials (months 11-14). The team requires 5-7 FTE engineers across embedded, AI, and QA disciplines.

---

## 2. Timeline Overview

```
Month:   1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18
        ┌─────────────────────────────────────────────────────────┐
HW      │██████████│           │           │                      │
Bringup  ██████████                                               
SW      │██████████████████████████████████████████████████████████│
Drivers │█████████████│           │           │                   
Edge    │       │████████████████│    │      │                   
Impulse │       ████████████████                                   
Nav     │           │██████████████████│    │                     
Decision│               │██████████████████│                      
Voice   │                   │███████████████████│                 
DB/Int  │                        │██████████████████              
Testing │                             │██████████████████████████ 
Opt     │                                  │████████████████      
Field   │                                       │████████████████  
Cert    │                                            │████████████ 
        ┌─────────────────────────────────────────────────────────┐
```

---

## 3. Phase 1: Hardware Bring-up (Months 1-2)

### 3.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| PCB layout and fabrication | HW Engineer | 1-4 | PCB v1.0 |
| Component procurement | Supply Chain | 1-3 | BOM fulfilled |
| Board assembly | CM | 3-5 | 10 assembled boards |
| Power-on verification | HW Engineer | 5-6 | Power rails verified |
| Bootloader flashing | FW Engineer | 5-6 | Arduino Q boots |
| USB camera enumeration | FW Engineer | 6-8 | Depth camera streams |

### 3.2 Dependencies

- None (first phase)

---

## 4. Phase 2: Sensor Drivers (Months 2-3)

### 4.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| I2C IMU driver | FW Engineer | 5-7 | IMU data at 1 kHz |
| SPI GPS driver | FW Engineer | 6-8 | GPS NMEA/UBX parsing |
| UART ESP32 driver | FW Engineer | 7-9 | AT command interface |
| I2S Audio driver | FW Engineer | 7-9 | Audio in/out working |
| GPIO BMS driver | FW Engineer | 8-10 | BMS status monitoring |
| ADC Battery driver | FW Engineer | 8-9 | Battery level estimation |
| Driver unit tests | QA Engineer | 9-10 | All driver tests pass |

### 4.2 Dependencies

- Phase 1 (HW bring-up) complete

---

## 5. Phase 3: Edge Impulse (Months 3-5)

### 5.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| Dataset collection & labeling | AI Engineer | 9-12 | 10K labeled images |
| Object detection model training | AI Engineer | 11-13 | FOMO model, mAP ≥ 85% |
| Depth processing (direct) | AI Engineer | 12-13 | SGM integration |
| Face recognition model | AI Engineer | 13-15 | 20-face enrollment |
| Text recognition model | AI Engineer | 14-16 | CRNN + CTC decoder |
| Wake word model | AI Engineer | 13-15 | DS-CNN, FA < 0.1/hr |
| Command recognition model | AI Engineer | 14-16 | DS-CNN, 25 commands |
| Model quantization (int8) | AI Engineer | 15-16 | All models ≤ 2 MB |
| EON compiler integration | FW Engineer | 16-17 | Inference pipeline |

### 5.2 Dependencies

- Phase 2 (camera driver with frame capture) complete

---

## 6. Phase 4: Navigation (Months 4-6)

### 6.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| GPS-IMU EKF fusion | Nav Engineer | 14-16 | 15-state EKF |
| PDR implementation | Nav Engineer | 15-17 | Step detection + update |
| Route management | Nav Engineer | 16-18 | Waypoints + turn detection |
| Obstacle avoidance | Nav Engineer | 17-19 | Zone analysis + guidance |
| Stair/door detection | Nav Engineer | 18-20 | Depth-based detection |

### 6.2 Dependencies

- Phase 2 (IMU, GPS, camera drivers)
- Phase 3 (object detection for obstacle+door+stair)

---

## 7. Phase 5: Decision Engine (Months 5-7)

### 7.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| Priority queue implementation | SW Engineer | 18-20 | Queue with dedup + cooldown |
| State machine implementation | SW Engineer | 19-21 | All mode transitions |
| Context manager | SW Engineer | 20-22 | Context update + mode eval |
| Hazard classification | SW Engineer | 21-23 | Scoring + priority mapping |
| Dialog manager | SW Engineer | 22-24 | Multi-turn conversation |

### 7.2 Dependencies

- Phase 4 (navigation inputs for decision context)

---

## 8. Phase 6: Voice Assistant (Months 6-8)

### 8.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| MFCC + VAD pipeline | AI Engineer | 22-24 | 48 kHz real-time processing |
| Noise reduction (Wiener) | AI Engineer | 23-25 | ≥ 8 dB SNR improvement |
| Intent parser + entities | SW Engineer | 24-26 | 25 command patterns |
| TTS engine | SW Engineer | 25-27 | Concatenative synthesis |
| Dialog flow integration | SW Engineer | 26-28 | End-to-end voice interaction |

### 8.2 Dependencies

- Phase 2 (audio I2S driver)
- Phase 3 (wake word + command models)
- Phase 5 (dialog management)

---

## 9. Phase 7: Database & Integration (Months 7-9)

### 9.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| Flash storage driver | FW Engineer | 26-28 | QSPI read/write + wear-leveling |
| Config manager | FW Engineer | 27-29 | JSON config load/save |
| Face/route database | SW Engineer | 28-30 | Binary store + lookup |
| Log system | FW Engineer | 28-30 | Circular log buffer |
| Full system integration | All Engineers | 29-32 | All modules connected |
| Integration tests | QA Engineer | 30-33 | All IT scenarios pass |

### 9.2 Dependencies

- Phases 1-6 complete

---

## 10. Phase 8: Testing (Months 8-11)

### 10.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| Unit test completion | All Engineers | 30-34 | 800+ tests pass |
| Integration test pass | QA Engineer | 32-36 | 200+ tests pass |
| Hardware validation | HW Engineer | 32-36 | All HW tests pass |
| Latency measurement | QA Engineer | 34-36 | All LAT targets met |
| Power characterization | HW Engineer | 34-36 | Power profile complete |
| Stress testing | QA Engineer | 36-38 | 24-hour continuous |
| Bug fixing | All Engineers | 34-40 | All critical/high bugs fixed |

### 10.2 Dependencies

- Phase 7 (integration complete)

---

## 11. Phase 9: Optimization (Months 10-12)

### 11.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| CPU profiling + optimization | FW Engineer | 38-40 | CPU ≤ 90% at peak |
| Memory optimization | FW Engineer | 38-40 | SRAM ≤ 90% |
| Power optimization | HW Engineer | 39-41 | 8-hour battery target |
| Inference latency tuning | AI Engineer | 39-41 | All models within budget |
| Audio latency tuning | FW Engineer | 40-42 | End-to-end ≤ 200 ms |
| Boot time optimization | FW Engineer | 40-42 | Boot ≤ 10 s |
| Final code review | Tech Lead | 41-43 | All modules reviewed |

### 11.2 Dependencies

- Phase 8 (testing identifies optimization areas)

---

## 12. Phase 10: Field Trials (Months 11-14)

### 12.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| Alpha test (5 sighted) | QA Engineer | 42-44 | Bug reports + fixes |
| Beta test (5 VI users) | UX Researcher | 44-48 | Usability feedback |
| Beta test (5 Alzheimer's) | UX Researcher | 44-48 | Safety feedback |
| Gamma test (25 users) | QA + UX | 48-52 | Validation data |
| Bug fixes + iteration | All Engineers | 44-54 | All field issues resolved |

### 12.2 Dependencies

- Phase 9 (optimized system ready for field use)

---

## 13. Phase 11: Certification & Production (Months 12-18)

### 13.1 Tasks

| Task | Owner | Weeks | Deliverable |
|---|---|---|---|
| FCC/CE certification | Compliance | 46-52 | Certification passes |
| ISO 13485 audit | Quality | 46-52 | QMS certification |
| IEC 62368-1 safety test | Compliance | 48-54 | Safety certification |
| BOM finalization | Supply Chain | 48-54 | BOM cost ≤ $150 |
| Production PCB v2.0 | HW Engineer | 50-56 | Manufacturing release |
| Production test jig | Manufacturing | 52-56 | Automated test fixture |
| Pilot production (100 units) | Manufacturing | 54-58 | 100 units manufactured |
| Clinical trial prep | Medical | 56-60 | IRB approval + protocol |

### 13.2 Dependencies

- Phase 10 (field trials inform final hardware/software revision)

---

## 14. Gantt Chart

```
Task                        M1  M2  M3  M4  M5  M6  M7  M8  M9  M10 M11 M12 M13 M14 M15 M16 M17 M18
────                        ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──  ──
HW Bring-up               [████████]
PCB                       [██]
Procurement                [██]
Assembly                    [██]
POST                       [██]
Camera enum                 [██]

Sensor Drivers                  [████████████]
IMU driver                    [███]
GPS driver                       [███]
ESP32 driver                     [███]
Audio driver                     [███]
BMS/ADC                          [███]
Unit tests                          [███]

Edge Impulse                         [██████████████████]
Dataset                               [████]
Obj detection                         [██████]
Depth proc                            [███]
Face recog                              [████]
Text recog                               [████]
Wake word                                 [████]
Cmd word                                   [███]
Quantization                                [███]
EON integration                               [███]

Navigation                                       [████████████████]
EKF                                              [████]
PDR                                                [████]
Route mgmt                                          [████]
Obstacle avoid                                        [█████]
Stair/door                                              [█████]

Decision Eng                                              [█████████████]
Priority Q                                                  [████]
State machine                                                [████]
Context mgr                                                    [████]
Hazard                                                          [████]
Dialog                                                            [████]

Voice Assistant                                                   [████████████████]
MFCC/VAD                                                           [████]
Noise cancel                                                         [████]
Intent parser                                                          [████]
TTS engine                                                              [████]
Dialog flow                                                              [████]

DB & Integration                                                           [█████████████]
Flash driver                                                               [████]
Config mgr                                                                  [████]
Face/route DB                                                                 [████]
Log system                                                                     [████]
Integration                                                                      [████]
IT pass                                                                           [█████]

Testing                                                                               [████████████████]
Unit tests                                                                              [████]
HW validation                                                                           [█████]
Stress testing                                                                             [█████]
Bug fixing                                                                                   [████████]

Optimization                                                                                   [████████████]
CPU/memory opt                                                                                   [████]
Power opt                                                                                          [█████]
Latency tuning                                                                                      [█████]
Final review                                                                                          [█████]

Field Trials                                                                                              [████████████████]
Alpha (5)                                                                                                     [████]
Beta (10)                                                                                                        [██████]
Gamma (25)                                                                                                            [██████]
Bug fix                                                                                                                   [████████]

Cert & Production                                                                                                               [████████████████]
FCC/CE                                                                                                                              [████]
ISO 13485                                                                                                                           [████]
Safety                                                                                                                               [████]
PCB v2.0                                                                                                                              [████]
Pilot prod                                                                                                                             [████]
Clinical prep                                                                                                                           [████]
```

---

## 15. Milestones

| ID | Milestone | Date | Criteria |
|---|---|---|---|
| M-001 | HW Rev 1.0 assembled | Month 2 | All components soldered, power-on OK |
| M-002 | All sensor drivers working | Month 3 | IMU 1 kHz, GPS 10 Hz, camera 30 fps, audio I2S |
| M-003 | AI models trained (all) | Month 5 | All 6 models meet accuracy targets |
| M-004 | Navigation working | Month 6 | GPS route + obstacle avoidance functional |
| M-005 | Decision engine complete | Month 7 | All 10 modes + transitions verified |
| M-006 | Voice assistant complete | Month 8 | Wake word → command → TTS end-to-end |
| M-007 | System integration complete | Month 9 | All modules integrated and talking |
| M-008 | Testing complete | Month 11 | All 800+ unit, 200+ integration tests pass |
| M-009 | Optimization complete | Month 12 | All performance targets met |
| M-010 | Alpha field trial complete | Month 12 | Engineer feedback incorporated |
| M-011 | Beta field trial complete | Month 13 | User feedback incorporated |
| M-012 | Gamma field trial complete | Month 14 | Validation data collected |
| M-013 | Regulatory certification | Month 15 | FCC, CE, IEC 62368-1 passes |
| M-014 | Pilot production | Month 17 | 100 units manufactured |
| M-015 | TRL 7 achieved | Month 18 | System demonstrated in operational environment |

---

## 16. Deliverables

| Phase | Deliverable | Format | Recipient |
|---|---|---|---|
| 1 | PCB design files (Gerber, ODB++) | Electronic | Manufacturer |
| 1 | BOM + sourcing | Spreadsheet | Supply chain |
| 1 | 10 assembled test boards | Physical | Engineering |
| 2 | Driver source code | GitHub | Engineering |
| 2 | Driver API documentation | Markdown | Engineering |
| 3 | Trained Edge Impulse models | .eim files | Firmware |
| 3 | Model training report | PDF | AI team |
| 4 | Navigation source code | GitHub | Engineering |
| 4 | Navigation test log | PDF | QA |
| 5 | Decision engine source | GitHub | Engineering |
| 5 | State machine documentation | PDF | Engineering |
| 6 | Voice assistant source | GitHub | Engineering |
| 6 | Dialogue flow diagrams | PDF | Engineering |
| 7 | Database driver source | GitHub | Engineering |
| 7 | Full system firmware image | .bin | Engineering |
| 8 | Test report (all levels) | PDF | QA |
| 8 | Bug tracker (resolved) | Jira | Engineering |
| 9 | Optimization report | PDF | Engineering |
| 9 | Performance benchmark | PDF | Engineering |
| 10 | Field trial report | PDF | Product |
| 10 | User feedback analysis | PDF | Product |
| 11 | Certification documents | PDF | Regulatory |
| 11 | Manufacturing test fixture | Physical | Manufacturing |
| 11 | 100 production units | Physical | Distribution |
| + | System Design Document (SDD) | Markdown | All stakeholders |

---

## 17. Dependencies

### 17.1 External Dependencies

| Dependency | Source | Lead Time | Risk |
|---|---|---|---|
| Arduino UNO Q SBC | Manufacturer | 4-6 weeks | Low (standard part) |
| Intel RealSense D435i | Distributor | 2-4 weeks | Low |
| u-blox NEO-M9N | Distributor | 2-4 weeks | Low |
| QSPI flash W25Q128JV | Distributor | 2-4 weeks | Low |
| Bone conduction transducer | Specialty supplier | 4-8 weeks | Medium |
| Custom PCB fab | PCB manufacturer | 2-3 weeks | Low |
| Edge Impulse Enterprise | Edge Impulse | Immediate | Low (SaaS) |
| FreeRTOS | Open source | Immediate | None |

### 17.2 Internal Dependencies

```
Phase 1 (HW)  → Phase 2 (Drivers) → Phase 3 (AI) → Phase 4 (Nav)
                                                      ↓
                Phase 5 (Decision) ←──────────────────┘
                Phase 6 (Voice) → depends on Phase 5
                Phase 7 (DB/Int) → depends on all prior
                Phase 8 (Test) → depends on Phase 7
                Phase 9 (Opt) → depends on Phase 8
                Phase 10 (Field) → depends on Phase 9
                Phase 11 (Cert) → depends on Phase 10
```

**Critical path:** Phase 1 → 2 → 3 → 4 → 5 → 7 → 8 → 9 → 10 → 11 = 18 months

---

## 18. Risk Analysis

| ID | Risk | Probability | Impact | Mitigation | Contingency |
|---|---|---|---|---|---|
| R-001 | Depth camera power too high (2.5 W) | High | Critical (battery life) | Duty-cycle to 50%; select OAK-D Lite | Accept reduced battery life |
| R-002 | AI inference exceeds 200 ms budget | Medium | High (latency requirement) | Reduce FPS to 10; simplify model | Accept 15 FPS with warnings |
| R-003 | IMU drift makes indoor navigation unusable | Medium | High (indoor feature) | Landmark correction; reduce expectations | Outdoor-only navigation v1 |
| R-004 | Bone conduction audio too quiet | Medium | Medium (user satisfaction) | Higher-power transducer; volume boost | Add external earpiece option |
| R-005 | Flash endurance exceeded in field | Medium | High (device failure) | Wear-leveling; batch writes | Reduce log frequency |
| R-006 | FDA/MDR regulatory delay | Low | High (market entry) | Early engagement with notified body | Delay launch 3-6 months |
| R-007 | ESP32-C3 AT throughput too slow | Medium | Medium (OTA speed) | Increase baud rate; use SPI mode | Offload to companion app |
| R-008 | Target users cannot use voice interface | Medium | Critical (accessibility) | Haptic + button primary fallback | Redesign interface |
| R-009 | Edge Impulse EON compiler limitation | Low | High (model performance) | Early prototype testing; alternative: TFLM | Use TensorFlow Lite Micro |
| R-010 | BOM cost exceeds $150 | Medium | High (cost constraint) | Component substitution; volume pricing | Accept $180 max |

### 18.1 Risk Matrix

```
Probability
   High    │ R-001         │ R-005         │ R-008         │
           │               │               │               │
   Medium  │               │ R-002, R-004  │ R-003, R-010  │
           │               │ R-007         │ R-009         │
   Low     │               │               │ R-006         │
           │               │               │               │
           ────────────────┴───────────────┴───────────────
               Low            Medium           High
                                   Impact
```

---

## 19. Resource Planning

### 19.1 Team Composition

| Role | FTE | Months Active | Lead Recruited By |
|---|---|---|---|
| Embedded Firmware Engineer | 2 | 1-12 | Month 1 |
| Senior AI/ML Engineer | 1 | 3-8 | Month 3 |
| Navigation Engineer | 1 | 4-10 | Month 4 |
| Hardware Engineer | 1 | 1-6, 12-18 | Month 1 |
| QA/Test Engineer | 1 | 4-18 | Month 4 |
| UX Researcher | 0.5 | 10-14 | Month 10 |
| Program Manager | 0.5 | 1-18 | Month 1 |
| **Total peak FTE** | **6.5** | **Months 6-10** | |

### 19.2 Budget Estimate

| Category | Cost | Notes |
|---|---|---|
| Engineering salaries (18 months) | $1,200,000 | 6.5 FTE average |
| Hardware (prototypes + test) | $50,000 | 10+ iterations |
| Edge Impulse Enterprise | $30,000 | 18 months |
| Certification (FCC, CE, IEC) | $80,000 | Including testing lab |
| Tooling + NRE for production | $50,000 | Injection mold + test jig |
| Pilot production (100 units) | $25,000 | $250/unit at low volume |
| Field trial logistics | $30,000 | Participant compensation, travel |
| Contingency (20%) | $293,000 | |
| **Total** | **$1,758,000** | |

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior Program Manager | Initial draft |

---

*End of Document — ROADMAP-001*
