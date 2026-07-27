# Hardware Architecture Document

**Document ID:** HW-ARCH-001

**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients

**Author:** Senior Embedded Hardware Architect

**Revision:** 0.1

**Date:** 2026-07-27

---

## Table of Contents

1. Executive Summary
2. Hardware Overview
3. Power Architecture
4. Communication Architecture
5. PCB Block Diagram
6. Power Rails
7. USB Connections
8. GPIO Allocation
9. I2C Devices
10. SPI Devices
11. UART Devices
12. Battery Monitoring
13. Charging System
14. Voltage Regulation
15. Component Placement
16. Thermal Management
17. Hardware Module Deep-Dive
18. Hardware Bottlenecks
19. Suggested Improvements
20. Bill of Materials Summary

---

## 1. Executive Summary

The smart glasses hardware is built around the Arduino UNO Q SBC as the central compute platform. The system integrates 11 peripheral modules: depth camera, RGB camera, GPS, 9-axis IMU, bone conduction speaker, microphone, battery, BMS, charging circuit, WiFi, and Bluetooth. The architecture is designed for minimal power consumption, real-time sensor fusion, and edge AI inference using Edge Impulse.

---

## 2. Hardware Overview

### 2.1 System Block Diagram

```
+---------------------------------------------------------------------+
|                        SMART GLASSES SYSTEM                          |
+---------------------------------------------------------------------+
|                                                                      |
|  +------------------+    +------------------+    +----------------+  |
|  |   DEPTH CAMERA   |    |   RGB CAMERA     |    |    GPS MODULE  |  |
|  |  Intel RealSense  |    |   OV5640 (opt.)  |    |   u-blox NEO-  |  |
|  |      D435i       |    |                  |    |      M9N      |  |
|  +--------+---------+    +--------+---------+    +-------+--------+  |
|           |                       |                        |         |
|           | USB                   | DVP/CSI                | UART    |
|           |                       |                        |         |
|           +-----------+-----------+----------+-------------+         |
|                       |                      |                       |
|                       v                      v                       |
|              +--------+----------------------+--------+              |
|              |         ARDUINO UNO Q SBC              |              |
|              |  - ARM Cortex-M7 @ 600 MHz             |              |
|              |  - 2 MB Flash / 1 MB SRAM              |              |
|              |  - Edge Impulse Runtime                |              |
|              |  - USB 2.0 OTG                         |              |
|              +--+----------+----------+----------+---+              |
|                 |          |          |          |                    |
|        I2C------+  SPI-----+  UART----+  GPIO----+                    |
|                 |          |          |          |                    |
|  +--------------+  +-------+--+  +----+-------+  +----------------+ |
|  |  9-AXIS IMU |  |  BONE     |  |  MICROPHONE|  |  WIFI/BT      | |
|  |  ICM-20948  |  |  CONDUCTION|  |  SPH0645   |  |  ESP32-C3     | |
|  |             |  |  SPEAKER  |  |  I2S MEMS  |  |               | |
|  +-------------+  +----------+  +------------+  +----------------+ |
|                                                                      |
|  +----------------------------------------------------------------+ |
|  |                    POWER MANAGEMENT                             | |
|  |  +----------+  +----------+  +----------+  +-----------------+ | |
|  |  | Li-Ion   |  |   BMS    |  | CHARGER  |  | VOLTAGE REGS    | | |
|  |  | 3000 mAh |  |  BQ297xx |  | TP4056   |  | 3.3V/1.8V/1.2V  | | |
|  |  +----------+  +----------+  +----------+  +-----------------+ | |
|  +----------------------------------------------------------------+ |
+---------------------------------------------------------------------+
```

---

## 3. Power Architecture

### 3.1 Power Tree

```
                         +-------------------+
                         |   Li-Ion Battery  |
                         |    3.7 V / 3000   |
                         |       mAh         |
                         +--------+----------+
                                  |
                                  v
                         +-------------------+
                         |       BMS         |
                         |   BQ297xx         |
                         |  OVP/UVP/OTC/SCP  |
                         +--------+----------+
                                  |
                     +------------+------------+
                     |                         |
                     v                         v
              +------------+           +---------------+
              |  TP4056    |           |  Power Switch |
              |  Charger   |           |  (Load Switch)|
              |  5V USB -> |           +-------+-------+
              |  4.2V out  |                   |
              +------------+                   v
                                        +---------------------+
                                        |   3.3V LDO Regulator |
                                        |   (Main System Rail) |
                                        +----------+----------+
                                                   |
                          +------------------------+------------------------+
                          |                        |                        |
                          v                        v                        v
                  +-------------+          +--------------+          +--------------+
                  | 1.8V LDO    |          | 1.2V LDO     |          | 5V Boost     |
                  | (IMU, GPS)  |          | (Core Logic)  |          | (USB Host)   |
                  +-------------+          +--------------+          +--------------+
```

### 3.2 Power Budget

| Component | Voltage | Max Current | Max Power | Duty Cycle | Avg Power |
|---|---|---|---|---|---|
| Arduino UNO Q SBC | 3.3 V | 500 mA | 1.65 W | 100% | 1.65 W |
| Depth Camera (D435i) | 5.0 V | 500 mA | 2.50 W | 80% | 2.00 W |
| RGB Camera (OV5640) | 3.3 V | 150 mA | 0.50 W | 30% | 0.15 W |
| GPS (u-blox M9N) | 3.3 V | 67 mA | 0.22 W | 100% | 0.22 W |
| 9-axis IMU (ICM-20948) | 3.3 V | 3 mA | 0.01 W | 100% | 0.01 W |
| Bone Conduction Speaker | 3.3 V | 200 mA | 0.66 W | 25% | 0.17 W |
| Microphone (SPH0645) | 3.3 V | 1.5 mA | 0.005 W | 100% | 0.005 W |
| WiFi/BT (ESP32-C3) | 3.3 V | 250 mA | 0.83 W | 20% | 0.17 W |
| BMS + Charger | 3.7 V | 2 mA | 0.01 W | 100% | 0.01 W |
| **Total** | | | | | **4.39 W** |

**Battery Life Calculation:**
- Battery capacity: 3000 mAh @ 3.7 V = 11.1 Wh
- Average system power: 4.39 W
- Estimated runtime: 11.1 Wh / 4.39 W ≈ **2.5 hours**

**Note:** This reveals a critical bottleneck. The depth camera (D435i) dominates power consumption. A lower-power depth sensor or duty-cycling strategy is required to meet the 8-hour target.

---

## 4. Communication Architecture

### 4.1 Interconnect Diagram

```
                        +------------------+
                        |  ARDUINO UNO Q   |
                        |                  |
                        |  I2C1: IMU       |
                        |  I2C2: ---       |
                        |  SPI1: GPS       |
                        |  SPI2: ---       |
                        |  UART0: Debug    |
                        |  UART1: WiFi/BT  |
                        |  UART2: GPS (alt)|
                        |  USB1: Depth Cam |
                        |  USB2: RGB Cam   |
                        |  I2S: Mic/Speaker|
                        |  GPIO: BMS, LEDs |
                        +------------------+
                               |   |   |   |
              +----------------+   |   +----+-----------+
              |                    |                    |
              v                    v                    v
        +-----------+       +-----------+        +-----------+
        | ICM-20948 |       | ESP32-C3  |        | SPH0645   |
        | (IMU)     |       | (WiFi/BT) |        | (Mic)     |
        | I2C addr  |       | UART 115.2|        | I2S       |
        | 0x68      |       |           |        |           |
        +-----------+       +-----------+        +-----------+
```

### 4.2 Bus Speed Configuration

| Bus | Peripheral | Speed | Purpose |
|---|---|---|---|
| I2C1 | ICM-20948 | 400 kHz | IMU data streaming |
| SPI1 | u-blox M9N (via SPI) | 10 MHz | GPS position data |
| UART1 | ESP32-C3 | 115200 bps | WiFi/BT AT commands |
| USB1 | Intel D435i | USB 2.0 HS | Depth + RGB stream |
| USB2 | OV5640 | USB 2.0 FS | Optional RGB stream |
| I2S | SPH0645 + MAX98357 | 48 kHz | Audio in/out |

---

## 5. PCB Block Diagram

```
+-------------------------------------------------------------+
|                        PCB LAYOUT                            |
+-------------------------------------------------------------+
|                                                             |
|  +-------------+  +-------------+  +---------------------+  |
|  | PWR SECTION |  | ARDUINO Q   |  | USB HUB (optional)  |  |
|  | TP4056      |  | SOM MODULE  |  | USB2514B            |  |
|  | BQ297xx     |  |             |  |                     |  |
|  | Regulators  |  |             |  |                     |  |
|  +------+------+  +------+------+  +----------+----------+  |
|         |                |                    |              |
|         +--------+-------+--------------------+              |
|                  |                                           |
|         +--------+--------+                                  |
|         |   SENSOR BANK   |                                  |
|         | IMU | GPS | Mic |                                  |
|         +--------+--------+                                  |
|                  |                                           |
|         +--------+--------+                                  |
|         |  CONNECTIVITY   |                                  |
|         |   ESP32-C3      |                                  |
|         |   (WiFi/BT)     |                                  |
|         +-----------------+                                  |
|                                                             |
|  +-------------------+  +----------------------------+      |
|  | AUDIO AMP (I2S)   |  | EXT CONNECTORS             |      |
|  | MAX98357 -> BCS   |  | J1: Cameras | J2: Debug    |      |
|  +-------------------+  +----------------------------+      |
+-------------------------------------------------------------+
```

---

## 6. Power Rails

| Rail ID | Voltage | Max Current | Source | Used By |
|---|---|---|---|---|
| VIN | 3.7 V (3.0-4.2 V) | 2 A | Battery | BMS input |
| VUSB | 5.0 V | 1 A | USB-C charger | TP4056 input |
| VCHG | 4.2 V | 1 A | TP4056 output | Battery charge |
| VSYS | 3.7 V | 2 A | BMS output | System power switch |
| 3V3_A | 3.3 V | 800 mA | 3.3V LDO | Arduino Q, IMU, GPS, ESP32 |
| 3V3_B | 3.3 V | 200 mA | 3.3V LDO | RGB camera, audio |
| 1V8 | 1.8 V | 100 mA | 1.8V LDO | IMU digital, GPS core |
| 1V2 | 1.2 V | 50 mA | 1.2V LDO | Reserved |
| 5V_BST | 5.0 V | 500 mA | Boost converter | Depth camera, USB host |

---

## 7. USB Connections

| Port | Connector | Protocol | Device | Speed |
|---|---|---|---|---|
| USB1 | USB-C (device) | USB 2.0 HS | Depth camera D435i | 480 Mbps |
| USB2 | USB-C (device) | USB 2.0 FS | RGB camera OV5640 | 12 Mbps |
| USB3 | USB-C (host) | USB 2.0 FS | Debug/programming | 12 Mbps |

---

## 8. GPIO Allocation

### 8.1 Arduino UNO Q GPIO Map

| Pin | Function | Connected To | Direction | Notes |
|---|---|---|---|---|
| D0/D1 | UART0 RX/TX | USB-Serial bridge (FTDI) | I/O | Debug console |
| D2 | INT1 | IMU ICM-20948 (INT) | IN | Data ready interrupt |
| D3 | INT2 | ESP32-C3 (GPIO0) | IN | WiFi status |
| D4 | BMS_ALERT | BQ297xx (Alert) | IN | Battery fault |
| D5 | BMS_STAT | BQ297xx (STAT) | IN | Charging status |
| D6 | CHG_EN | TP4056 (CE) | OUT | Enable charger |
| D7 | BOOST_EN | Boost converter (EN) | OUT | Enable 5V boost |
| D8 | LED_GREEN | Power indicator LED | OUT | Active low |
| D9 | LED_RED | Status indicator LED | OUT | PWM dimmable |
| D10 | CS_GPS | GPS module (CS) | OUT | SPI chip select |
| D11 | MOSI | SPI bus | OUT | Shared SPI |
| D12 | MISO | SPI bus | IN | Shared SPI |
| D13 | SCK | SPI bus | OUT | Shared SPI |

### 8.2 I2C Bus 1 (400 kHz)

| Pin | Signal | Device | Address |
|---|---|---|---|
| SCL1 | SCL | ICM-20948 | 0x68 |
| SDA1 | SDA | ICM-20948 | 0x69 (AD0=high) |

### 8.3 I2S Bus

| Pin | Signal | Device |
|---|---|---|
| WS | Word Select | SPH0645 (mic), MAX98357 (speaker) |
| BCK | Bit Clock | SPH0645, MAX98357 |
| DIN | Data In | MAX98357 |
| DOUT | Data Out | SPH0645 |

---

## 9. I2C Devices

| Device | Bus | Address | Register Map | Data Rate | Interrupt |
|---|---|---|---|---|---|
| ICM-20948 (IMU) | I2C1 | 0x68 | 128 regs | 1 kHz accel/gyro, 100 Hz mag | INT1 (D2) |
| BQ297xx (BMS) | — | — | Dedicated pins | N/A | ALERT (D4) |

**Note:** I2C bus is dedicated to IMU only to avoid contention. The IMU streams at high rate (1 kHz) and cannot share the bus with slower devices.

---

## 10. SPI Devices

| Device | Bus | CS Pin | Speed | Mode | Data Size |
|---|---|---|---|---|---|
| u-blox NEO-M9N (GPS) | SPI1 | D10 | 10 MHz | Mode 0 | 8-bit |
| ESP32-C3 (WiFi/BT) | — | — | — | UART-based | — |

**Note:** GPS uses SPI for higher throughput than UART. The ESP32-C3 communicates via UART AT command set to offload WiFi/BT stack.

---

## 11. UART Devices

| Port | TX Pin | RX Pin | Baud | Device | Flow Control |
|---|---|---|---|---|---|
| UART0 | D0 | D1 | 115200 | USB-Serial (debug) | None |
| UART1 | TX1 | RX1 | 115200 | ESP32-C3 (WiFi/BT) | None |
| UART2 | TX2 | RX2 | 9600 | GPS (fallback) | None |

**UART0** — Debug console (115200, 8N1). Used during development only. Disabled in production firmware per SCR-007.

**UART1** — Primary communication channel to ESP32-C3 coprocessor. AT command set over 115200 baud.

**UART2** — Fallback GPS interface. Active only if SPI GPS communication fails.

---

## 12. Battery Monitoring

### 12.1 Monitoring Scheme

```
+------------------+
|    Battery       |
|  3.7V Li-Ion    |
|  (2S or 1S)     +----> BMS BQ297xx ----> ALERT pin ---> Arduino Q (D4)
|                  |         |                               (fault detect)
|                  |         +----> STAT pin ----> Arduino Q (D5)
|                  |                               (charging status)
+--------+---------+
         |
         +-----> Voltage Divider (R1=100k, R2=47k)
                        |
                        v
                 Arduino Q ADC (A0)
                    0-3.3V scaled
                      Battery level estimation
```

### 12.2 ADC Channels

| ADC Pin | Signal | Range | Resolution | Purpose |
|---|---|---|---|---|
| A0 | Battery voltage (div by 4.13) | 0-4.2V → 0-1.02V ADC | 12-bit (0-4095) | Battery level estimation |
| A1 | IMU temp (optional) | 0-3.3V | 12-bit | Thermal monitoring |

### 12.3 Battery Voltage Lookup Table

| ADC Reading | Scaled Voltage | Battery Voltage | Charge Level |
|---|---|---|---|
| 3100 | 2.50 V | 4.20 V | 100% |
| 3000 | 2.42 V | 4.05 V | 80% |
| 2900 | 2.34 V | 3.92 V | 60% |
| 2750 | 2.22 V | 3.72 V | 40% |
| 2600 | 2.10 V | 3.52 V | 20% |
| 2500 | 2.02 V | 3.38 V | 10% |
| 2400 | 1.94 V | 3.25 V | 0% (cutoff) |

---

## 13. Charging System

### 13.1 Charger Circuit

```
USB 5V ----> TP4056 (Li-Ion Charger)
                |
                +-----> CHG_EN (D6) control
                |
                +-----> STAT pins (LED indicators)
                |
                +-----> BAT pin -> Battery + BMS
                |
                +-----> PROG pin -> R_PROG (1.2k = 1A charge)
```

### 13.2 Charging Profile

| Phase | Voltage | Current | Duration |
|---|---|---|---|
| Precondition | < 3.0 V | 100 mA | Until 3.0 V |
| Constant Current | 3.0-4.2 V | 1 A | ~2.5 hours |
| Constant Voltage | 4.2 V | Tapering | ~30 min |
| Termination | 4.2 V | < 0.1C | Auto-stop |

**Charge time (0-80%):** ~90 minutes (meets PR-010)

---

## 14. Voltage Regulation

### 14.1 Regulator Selection

| Rail | Regulator Type | Part Number | Vin Range | Vout | Iout max | Efficiency | Quiescent |
|---|---|---|---|---|---|---|---|
| 3V3_A | LDO | TPS79333 | 2.7-5.5 V | 3.3 V | 200 mA | 85% @ 100 mA | 170 µA |
| 3V3_B | LDO | TPS79333 | 2.7-5.5 V | 3.3 V | 200 mA | 85% @ 100 mA | 170 µA |
| 1V8 | LDO | TPS79318 | 2.7-5.5 V | 1.8 V | 100 mA | 80% @ 50 mA | 170 µA |
| 1V2 | LDO | TPS79312 | 2.7-5.5 V | 1.2 V | 100 mA | 75% @ 30 mA | 170 µA |
| 5V_BST | Boost | TPS61023 | 2.5-5.5 V | 5.0 V | 1500 mA | 93% peak | 15 µA |

### 14.2 Power Sequencing

```
Startup Sequence:
1. Battery connected → BMS enables VSYS (t < 1 ms)
2. VSYS → 3V3_A, 3V3_B LDOs enable (t < 5 ms)
3. 3V3_A → Arduino Q boots (t < 100 ms)
4. Arduino Q sets BOOST_EN high → 5V_BST active (t < 2 ms)
5. 5V_BST → Depth camera powers on (t < 50 ms)
6. Arduino Q enables I2C/SPI/UART peripherals sequentially

Shutdown Sequence:
1. Arduino Q detects low battery (ADC reading < 2400)
2. Audio alert: "Battery low. Shutting down."
3. Save system state to flash (t < 10 ms)
4. Disable 5V_BST (t < 1 ms)
5. BMS disconnects load (t < 1 ms)
```

---

## 15. Component Placement

### 15.1 Mechanical Layout

```
+--------------------------------------------------------+
|                    FRONT VIEW                           |
|                                                         |
|  +--------+  +----------+  +----------+  +--------+    |
|  | Depth  |  | RGB Cam  |  | GPS Ant  |  |  IMU   |    |
|  | Camera |  | (lower)  |  | (top)    |  | (temp) |    |
|  | Center |  | center   |  |          |  | center |    |
|  +--------+  +----------+  +----------+  +--------+    |
|                                                         |
|  +-------+     +-------------------+     +----------+  |
|  |  BMS   |     |   Arduino UNO Q  |     |  ESP32   |  |
|  | + Bat  |     |   (center PCB)   |     |  (right) |  |
|  | (left) |     |                   |     |          |  |
|  +-------+     +-------------------+     +----------+  |
|                                                         |
|  +----------+  +----------+  +-----------------------+  |
|  | Microph  |  | Speaker  |  |  USB-C / Power        |  |
|  | (left)   |  | (right)  |  |  (right temple)       |  |
|  +----------+  +----------+  +-----------------------+  |
+--------------------------------------------------------+
```

### 15.2 Placement Rules

| Component | Rule | Rationale |
|---|---|---|
| Depth camera | Center of frame, forward-facing | Optimal FOV, stereo baseline |
| RGB camera | Below depth camera, slight downward tilt | Text/object reading perspective |
| GPS antenna | Top of frame, clear sky view | Maximize GNSS signal reception |
| IMU | Center of mass, rigidly mounted | Minimize vibration-induced noise |
| Battery | Left temple (counterweight) | Balance right-side electronics |
| Arduino Q | Center PCB, shielded | Central hub reduces trace lengths |
| ESP32-C3 | Right temple | Antenna away from body, clear path |
| Microphone | Left temple, near mouth area | Voice pickup optimization |
| Bone conduction speaker | Right temple, behind ear | Transducer contact with temporal bone |
| BMS | Adjacent to battery | Minimize high-current path |

---

## 16. Thermal Management

### 16.1 Heat Sources

| Component | Max Power | Heat Dissipation | Cooling Method |
|---|---|---|---|
| Arduino Q (CPU) | 1.65 W | 1.65 W | Copper pour + thermal vias |
| Depth camera | 2.50 W | 2.50 W | Active duty cycling (80% → 50%) |
| ESP32-C3 | 0.83 W | 0.83 W | Passive (low power) |
| 5V Boost converter | 0.50 W | 0.50 W | Copper pour |
| LDO regulators | 0.30 W | 0.30 W | Passive (low dropout) |

### 16.2 Thermal Limits

| Location | Max Surface Temp | Standard |
|---|---|---|
| Skin contact surfaces | 43°C | IEC 62368-1 |
| Internal PCB ambient | 60°C | Component derating |
| Battery surface | 55°C | BMS cutoff at 60°C |
| Depth camera housing | 50°C | Manufacturer spec |

### 16.3 Mitigation Strategies

1. **Duty-cycling depth camera:** Run at 50% duty (15 fps actual, 30 fps sensor) to reduce average power to 1.25 W
2. **Copper pour:** At least 2 oz copper on both layers for heat spreading
3. **Thermal vias:** Matrix of vias under Arduino Q and boost converter
4. **Standoff gap:** 2 mm gap between PCB and skin-facing surface
5. **Thermal cutoff:** BMS disconnects at 60°C battery temperature

---

## 17. Hardware Module Deep-Dive

### 17.1 Arduino UNO Q SBC

| Parameter | Value |
|---|---|
| MCU | ARM Cortex-M7 @ 600 MHz |
| Flash | 2 MB (usable for firmware + AI model) |
| SRAM | 1 MB (critical for AI inference buffers) |
| USB OTG | Yes (2x USB 2.0) |
| I2C | 2 buses |
| SPI | 2 buses |
| UART | 3 ports |
| I2S | 1 port |
| ADC | 12-bit, 8 channels |
| Operating Voltage | 3.3 V |

### 17.2 Depth Camera — Intel RealSense D435i

| Parameter | Value |
|---|---|
| Depth Technology | Active IR stereoscopic |
| Depth Resolution | 1280 × 720 (downscaled to 640 × 360) |
| Depth FPS | 30 fps (90 fps max) |
| Depth Range | 0.2-8 m |
| RGB Resolution | 1920 × 1080 |
| RGB FPS | 30 fps |
| IMU | Built-in Bosch BMI055 |
| Interface | USB 3.0 (USB 2.0 fallback) |
| Power | 2.5 W typical |
| Dimensions | 25 × 25 × 11 mm |

### 17.3 GPS Module — u-blox NEO-M9N

| Parameter | Value |
|---|---|
| GNSS | GPS + GLONASS + Galileo + BeiDou |
| Position Accuracy | 1.5 m CEP |
| Time-To-First-Fix (cold) | 26 s |
| Time-To-First-Fix (hot) | 2 s |
| Update Rate | 10 Hz (configurable) |
| Interface | SPI / UART / I2C |
| Power | 67 mA @ 3.3 V |
| Protocols | NMEA 0183 / UBX |

### 17.4 9-Axis IMU — ICM-20948

| Parameter | Value |
|---|---|
| Accelerometer | ±2/±4/±8/±16 g |
| Gyroscope | ±250/±500/±1000/±2000°/s |
| Magnetometer | ±4900 µT |
| Output Rate | 9-axis: 1 kHz |
| Interface | I2C (400 kHz) / SPI (7 MHz) |
| FIFO | 512 bytes |
| Power | 3 mA @ 3.3 V (full mode) |

### 17.5 Bone Conduction Speaker

| Parameter | Value |
|---|---|
| Type | Piezoelectric transducer |
| Frequency Response | 300 Hz - 8 kHz |
| Sound Pressure Level | 60-80 dB SPL (adjustable) |
| Impedance | 8 Ω |
| Max Power | 200 mW |
| Amplifier | MAX98357 (I2S input, 3.2W output) |

### 17.6 Microphone — SPH0645LU4H MEMS

| Parameter | Value |
|---|---|
| Type | Bottom-port MEMS, I2S output |
| Sensitivity | -26 dBFS @ 1 kHz, 94 dB SPL |
| SNR | 64 dBA |
| Frequency Response | 100 Hz - 10 kHz |
| Interface | I2S @ 48 kHz sample rate |
| Power | 1.5 mA @ 3.3 V |

### 17.7 Battery + BMS

| Parameter | Value |
|---|---|
| Chemistry | Li-Ion NMC |
| Capacity | 3000 mAh (11.1 Wh) |
| Nominal Voltage | 3.7 V |
| Charge Voltage | 4.2 V ± 1% |
| Discharge Cutoff | 3.0 V |
| Max Charge Current | 1.5 A |
| Max Discharge Current | 3 A |
| BMS IC | BQ297xx (OVP, UVP, OCP, SCP, OTP) |

### 17.8 WiFi/BT Module — ESP32-C3

| Parameter | Value |
|---|---|
| MCU | RISC-V 32-bit @ 160 MHz |
| WiFi | 802.11 b/g/n (2.4 GHz) |
| Bluetooth | BLE 5.0 |
| Flash | 4 MB |
| SRAM | 400 KB |
| Interface | UART (AT command set) |
| Power | 250 mA TX peak, 50 mA idle |

---

## 18. Hardware Bottlenecks

| ID | Bottleneck | Impact | Severity | Mitigation |
|---|---|---|---|---|
| HWB-001 | Depth camera power (2.5 W) | Battery life < 3 hours | Critical | Duty-cycle to 50%, use lower-power sensor |
| HWB-002 | USB bandwidth sharing | Depth + RGB simultaneous capture limited | High | Dedicate USB1 to depth, USB2 to RGB, reduce RGB usage |
| HWB-003 | Arduino Q Flash (2 MB) | AI model + firmware combined limit | High | Use external QSPI flash for model storage |
| HWB-004 | Arduino Q SRAM (1 MB) | Inference buffer + audio buffer contention | Medium | Optimize buffer sizes, use double-buffering |
| HWB-005 | UART1 baud 115200 | WiFi/BT throughput limited to ~11.5 KB/s | Medium | Increase baud to 921600, use flow control |
| HWB-006 | I2C bus single device | No room for expansion | Low | Add I2C mux or dedicate SPI for future devices |
| HWB-007 | Single SPI bus | GPS + future display contention | Low | Add dedicated SPI bus or use UART for GPS |
| HWB-008 | Thermal density | Skin contact temperature near limit | Medium | Reduce depth camera duty cycle, improve heat spreading |

---

## 19. Suggested Improvements

| ID | Improvement | Impact | Effort | Priority |
|---|---|---|---|---|
| IMP-001 | Replace D435i with OAK-D Lite (1.5 W depth AI camera) | -40% power, -$50 cost, on-device NN | Medium | High |
| IMP-002 | Add external QSPI flash (W25Q128JV, 128 Mbit) for AI model | Eliminates Flash bottleneck | Low | High |
| IMP-003 | Add PSRAM (16 MB) for inference buffer expansion | Eliminates SRAM bottleneck | Medium | High |
| IMP-004 | Increase UART1 baud to 921600 with hardware flow control | 8x WiFi/BT throughput | Low | Medium |
| IMP-005 | Add I2C mux (TCA9548A) for future I2C expansion | Future-proofing | Low | Medium |
| IMP-006 | Use TPS61093 boost (higher efficiency) | +5% efficiency on 5V rail | Low | Medium |
| IMP-007 | Add lidar sensor (VL53L1X) for close-range obstacle detection | Improved close-range accuracy | Medium | Medium |
| IMP-008 | Replace TP4056 with BQ25890 for USB PD charging | Faster charging, power path management | Medium | Medium |
| IMP-009 | Add haptic motor (ERM) for discreet feedback | Improved accessibility | Low | Low |
| IMP-010 | Use flex PCB for temple sections | Better weight distribution, smaller form factor | High | Low |

---

## 20. Bill of Materials Summary

| Item | Part Number | Qty | Unit Cost (1000) | Total Cost |
|---|---|---|---|---|
| Arduino UNO Q SBC | Arduino UNO Q | 1 | $35.00 | $35.00 |
| Depth Camera | Intel RealSense D435i | 1 | $45.00 | $45.00 |
| RGB Camera | OV5640 module | 1 | $8.00 | $8.00 |
| GPS Module | u-blox NEO-M9N | 1 | $18.00 | $18.00 |
| 9-axis IMU | ICM-20948 | 1 | $4.50 | $4.50 |
| Bone Conduction Speaker | BCT-10 | 1 | $6.00 | $6.00 |
| MEMS Microphone | SPH0645LU4H | 1 | $2.50 | $2.50 |
| Battery | 3000 mAh Li-Ion (103450) | 1 | $8.00 | $8.00 |
| BMS IC | BQ297xx | 1 | $0.80 | $0.80 |
| Charger IC | TP4056 | 1 | $0.50 | $0.50 |
| WiFi/BT Module | ESP32-C3 | 1 | $2.50 | $2.50 |
| LDO 3.3V (×2) | TPS79333 | 2 | $0.40 | $0.80 |
| LDO 1.8V | TPS79318 | 1 | $0.40 | $0.40 |
| Boost Converter | TPS61023 | 1 | $1.50 | $1.50 |
| Audio Amp | MAX98357 | 1 | $2.00 | $2.00 |
| USB-C Connector | USB4105 | 1 | $0.50 | $0.50 |
| Passive components | Resistors, caps, inductors | — | $3.00 | $3.00 |
| PCB | 4-layer FR4 | 1 | $2.50 | $2.50 |
| **Total BOM** | | | | **$141.50** |

BOM meets the $150 cost constraint (CON-001) at 10,000-unit volume.

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior Embedded Hardware Architect | Initial draft |

---

*End of Document — HW-ARCH-001*
