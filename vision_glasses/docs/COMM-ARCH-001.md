# Communication Architecture Document

**Document ID:** COMM-ARCH-001

**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients

**Author:** Senior Embedded Systems Engineer

**Revision:** 0.1

**Date:** 2026-07-27

---

## Table of Contents

1. Executive Summary
2. Communication Overview
3. I2C
4. SPI
5. UART
6. USB
7. Bluetooth (BLE)
8. WiFi
9. Internal IPC
10. Thread Communication
11. Shared Memory
12. Queues
13. Publish Subscribe
14. Timing Diagrams
15. Protocol Diagrams
16. Data Packets
17. Synchronization
18. Communication Priorities

---

## 1. Executive Summary

The communication architecture covers three domains: (1) internal chip-to-chip buses (I2C, SPI, UART, USB), (2) external wireless (BLE, WiFi), and (3) internal inter-process communication (IPC) between FreeRTOS tasks. Priority-based bus arbitration ensures safety-critical sensor data (fall detection, obstacle alerts) always has lowest latency regardless of other bus traffic.

---

## 2. Communication Overview

```
+---------------------------------------------------------------------+
|                    COMMUNICATION ARCHITECTURE                        |
+---------------------------------------------------------------------+
|                                                                     |
|  EXTERNAL WIRELESS                                                  |
|  ┌──────────┐    ┌──────────┐                                      |
|  │ BLE 5.0  │    │ WiFi 802.11                                      |
|  │ (ESP32)  │    │ b/g/n    │                                      |
|  │ - Phone  │    │ - OTA    │                                      |
|  │ - Audio  │    │ - Cloud  │                                      |
|  └────┬─────┘    └────┬─────┘                                      |
|       │UART1          │UART1                                       |
|       └────────┬──────┘                                            |
|                │                                                    |
|  ┌─────────────┴─────────────────────────────────────────────────┐ |
|  │                    ARDUINO UNO Q (Cortex-M7)                  │ |
|  │                                                               │ |
|  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌─────────┐    │ |
|  │  │ UART0  │ │ UART1  │ │ I2C1   │ │ SPI1   │ │ USB OTG │    │ |
|  │  │ Debug  │ │ ESP32  │ │ IMU    │ │ GPS    │ │ Camera  │    │ |
|  │  └────────┘ └────────┘ └────────┘ └────────┘ └─────────┘    │ |
|  │                                                               │ |
|  │  ┌──────────────────────────────────────────────────────────┐ │ |
|  │  │              INTERNAL IPC (FreeRTOS)                    │ │ |
|  │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌─────────┐   │ │ |
|  │  │  │ Message  │ │ Task     │ │ Event    │ │ Shared  │   │ │ |
|  │  │  │ Queue    │ │ Notify   │ │ Group    │ │ Memory  │   │ │ |
|  │  │  └──────────┘ └──────────┘ └──────────┘ └─────────┘   │ │ |
|  │  └──────────────────────────────────────────────────────────┘ │ |
|  └───────────────────────────────────────────────────────────────┘ |
|                                                                     |
|  CHIP-TO-CHIP BUSSES                                                |
|  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐         |
|  │ I2C      │  │ SPI      │  │ UART     │  │ USB      │         |
|  │ 400 kHz  │  │ 10 MHz   │  │ 115.2k   │  │ USB 2.0  │         |
|  │ IMU      │  │ GPS      │  │ ESP32    │  │ Camera   │         |
|  │          │  │          │  │ Debug    │  │          │         |
|  └──────────┘  └──────────┘  └──────────┘  └──────────┘         |
+---------------------------------------------------------------------+
```

---

## 3. I2C

### 3.1 Configuration

| Parameter | Value |
|---|---|
| Bus | I2C1 (dedicated) |
| Speed | 400 kHz (Fast Mode) |
| Devices | ICM-20948 (9-axis IMU) |
| Pull-ups | 4.7 kΩ to 3.3V |
| Voltage level | 3.3 V |
| Addressing | 7-bit |

### 3.2 IMU Communication Protocol

```
Master: Arduino UNO Q (Cortex-M7)
Slave:  ICM-20948 (addr 0x68)

Initialization:
  START → 0xD0 (write) → ACK → Reg 0x06 (user bank) → 0x20 → STOP
  START → 0xD0 (write) → ACK → Reg 0x00 (power mgmt) → 0x01 → STOP

Data Read (interrupt-driven):
  IMU raises INT1 (D2) → ISR triggers
  ┌────────────────────────────────────────────┐
  │ START → 0xD0 (write) → ACK → Reg 0x2D     │
  │         → STOP                             │
  │ START → 0xD1 (read) → ACK → Data[0]       │
  │         → ACK → Data[1] ... → NAK → STOP  │
  └────────────────────────────────────────────┘
  Data: 14 bytes (accel 6 + gyro 6 + temp 2)

Data rate: 1 kHz (interrupt) or polled at 1 kHz
FIFO: 512 bytes (512 samples × 1 byte each in FIFO mode)
```

### 3.3 I2C Timing

```
SCL: ┌────┐    ┌────┐    ┌────┐    ┌────┐
     │    │    │    │    │    │    │    │
     └────┴────┴────┴────┴────┴────┴────┴────
SDA: ──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──
       │  │  │  │  │  │  │  │  │  │  │  │
       └──┘  └──┘  └──┘  └──┘  └──┘  └──┘
       │  │                          │  │
       │  └── Data bit (MSB first)   └── ACK/NACK
       └──── SCL rising edge = sample data

t_SCL = 2.5 µs (400 kHz)
t_HD:STA = 0.6 µs (hold start)
t_SU:STA = 0.6 µs (setup start)
t_HD:DAT = 0 µs (hold data)
t_SU:DAT = 0.1 µs (setup data)
t_BUF = 1.3 µs (bus free)
```

---

## 4. SPI

### 4.1 Configuration

| Parameter | Value |
|---|---|
| Bus | SPI1 (dedicated) |
| Speed | 10 MHz |
| Mode | Mode 0 (CPOL=0, CPHA=0) |
| Bit order | MSB first |
| Chip select | Active low, GPIO D10 |
| Devices | u-blox NEO-M9N (GPS) |

### 4.2 GPS SPI Protocol

```
Master: Arduino UNO Q (Cortex-M7)
Slave:  u-blox NEO-M9N (CS on D10)

UBX Protocol (u-blox binary):
  Sync: 0xB5 0x62
  Class: 1 byte (e.g., 0x01 = NAV)
  ID:    1 byte (e.g., 0x02 = POSLLH)
  Length: 2 bytes (little-endian)
  Payload: N bytes
  CK_A, CK_B: 2 bytes (Fletcher checksum)

Example — Poll NAV-POSLLH message:
  CS = LOW (select GPS)
  0xB5 0x62 0x01 0x02 0x00 0x00 0x03 0x0A (8 bytes)
  CS = HIGH (deselect GPS)

  Wait 5 ms
  CS = LOW
  Read: 0xB5 0x62 0x01 0x02 0x1C 0x00 [28 bytes payload] [CK_A] [CK_B]
  CS = HIGH

  Payload (28 bytes):
    iTOW:       4 bytes (GPS time of week)
    lon:        4 bytes (1e-7 degrees)
    lat:        4 bytes (1e-7 degrees)
    height:     4 bytes (mm above ellipsoid)
    hMSL:       4 bytes (mm above mean sea level)
    hAcc:       4 bytes (mm accuracy)
    vAcc:       4 bytes (mm accuracy)

Polling rate: 10 Hz (100 ms interval)
```

### 4.3 SPI Timing Diagram

```
SCK:  ┌──────┐    ┌──────┐    ┌──────┐    ┌──────
      │      │    │      │    │      │    │
      └──────┴────┴──────┴────┴──────┴────┴──────
MOSI: ──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌─────
        │  │  │  │  │  │  │  │  │  │  │  │
        └──┘  └──┘  └──┘  └──┘  └──┘  └──┘
MISO: ──────┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐
            │  │  │  │  │  │  │  │  │  │  │
            └──┘  └──┘  └──┘  └──┘  └──┘  └──
CS:   ┌────────────────────────────────────────
      │
      └────────────────────────────────────────
      │   │   │   │   │   │   │   │   │   │
      │   │   │   │   │   │   │   │   │   │
      MOSI bit   MISO bit   SCK edge = sample
      (D10 CS)   (D12 MISO)  (falling edge)
      (D11 MOSI)            (rising edge = data valid)

t_SCK = 100 ns (10 MHz)
t_CS = 50 ns (setup before SCK)
t_CS_HOLD = 50 ns (hold after last SCK)
```

---

## 5. UART

### 5.1 Configuration

| Port | Baud | TX | RX | Device | Flow Control |
|---|---|---|---|---|---|
| UART0 | 115200 | D0 | D1 | USB-Serial (FTDI) | None |
| UART1 | 115200 | TX1 | RX1 | ESP32-C3 (WiFi/BT) | None (RTS/CTS planned) |
| UART2 | 9600 | TX2 | RX2 | GPS (fallback) | None |

### 5.2 ESP32-C3 AT Command Set

```
Format: AT+<CMD>=<params>\r\n
Response: \r\n<OK/ERROR>\r\n

WiFi Commands:
  AT+CWMODE=1               → Station mode
  AT+CWJAP="SSID","pass"    → Connect to WiFi
  AT+CIPSTART="TCP",ip,port → TCP connection
  AT+CIPSEND=<len>          → Send data
  AT+CIPCLOSE               → Close connection

BLE Commands:
  AT+BLEINIT=2              → BLE server mode
  AT+BLEGATTSSRVCRE        → Create GATT service
  AT+BLEGATTSCHAR=...       → Add characteristic
  AT+BLEADVSTART            → Start advertising
  AT+BLEDATARECV            → Data received from phone

Arduino Q → ESP32 data flow:
  Arduino: "AT+CWJAP=\"HomeWifi\",\"password123\"\r\n"
  ESP32:   "\r\nOK\r\n"

  Arduino: "AT+BLEDATARECV?\r\n"
  ESP32:   "\r\n+BLEDATARECV:0,\"HELLO\"\r\n\r\nOK\r\n"
```

### 5.3 UART Frame Format

```
Start bit (0) ─ Data bits (LSB first) ─ Parity (N) ─ Stop bit (1)

Bit time at 115200 baud: 8.68 µs

Byte "A" (0x41 = 0100_0001):
    ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐
    │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │
    └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──
    │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │  │
    Start  1  0  0  0  0  0  1  0  Stop Start   ...
```

---

## 6. USB

### 6.1 Configuration

| Port | Type | Speed | Device | Endpoints |
|---|---|---|---|---|
| USB1 | OTG host | USB 2.0 HS (480 Mbps) | Intel D435i depth camera | 3 (control, isochronous, bulk) |
| USB2 | OTG host | USB 2.0 FS (12 Mbps) | OV5640 RGB camera | 2 (control, bulk) |

### 6.2 Depth Camera USB Stream

```
Configuration:
  Interface 0: Control (endpoint 0)
  Interface 1: Isochronous (endpoint 1) — depth stream
    - bInterval = 1 (125 µs microframes)
    - wMaxPacketSize = 1024 bytes
    - 30 packets per frame → 30 KB per frame
    - 30 fps → ~900 KB/s (7.2 Mbps)
  Interface 2: Bulk (endpoint 2) — control/metadata

Data flow:
  USB Host Controller: DMA → Double buffer A/B
  Buffer full → interrupt → camera_task processes
```

### 6.3 USB Power

```
USB1 (depth camera): 5V @ 500 mA = 2.5 W
USB2 (RGB camera):   5V @ 150 mA = 0.75 W
Total USB power:     5V @ 650 mA = 3.25 W

Source: TPS61023 boost converter from battery
Max boost output: 5V @ 1.5 A (7.5 W) — sufficient
```

---

## 7. Bluetooth (BLE)

### 7.1 BLE Profile

```
Device: Smart Glasses (Peripheral)
Phone: Central

GATT Services:
  ┌───────────────────────────────────────────────┐
  │ Service: Glasses Control (UUID: xxxxxx-XXXX)  │
  │ ├── Characteristic: Commands (write)          │
  │ │   - "navigate:home"                         │
  │ │   - "set_reminder:19:00:medicine"           │
  │ │   - "upload_logs"                           │
  │ ├── Characteristic: Status (read/notify)      │
  │ │   - "battery:85"                            │
  │ │   - "state:navigating"                      │
  │ │   - "fall_detected"                         │
  │ └── Characteristic: Route Data (write long)   │
  │     - Binary route waypoints                  │
  │                                               │
  │ Service: Audio (UUID: xxxxxx-XXXX)            │
  │ ├── Characteristic: Audio Stream (notify)     │
  │ │   - 16-bit PCM @ 16 kHz (downsampled)      │
  │ └── Characteristic: Volume (write)            │
  │     - 0-10                                    │
  └───────────────────────────────────────────────┘

Connection parameters:
  Connection interval: 30-50 ms
  Slave latency: 0
  Supervision timeout: 2 s
  PHY: LE 1M (or LE 2M for audio)

Range: ~10 m (indoor), ~30 m (line-of-sight)
```

### 7.2 BLE Pairing Process

```
1. Boot: ESP32-C3 starts BLE advertising
   - Advertising packet: "SmartGlass-XXXX"
   - No MITM protection initially (for accessibility)

2. Phone app scans → finds SmartGlass-XXXX
   → Sends pairing request

3. User must confirm:
   - Voice: "Pair with phone? Say 'yes' to confirm"
   - User: "Yes"
   - OR press physical button within 10 seconds

4. BLE pairing completes (Just Works or Passkey on phone)

5. Bonding: credentials stored for auto-reconnect
```

---

## 8. WiFi

### 8.1 WiFi Configuration

| Parameter | Value |
|---|---|
| Standard | 802.11 b/g/n (2.4 GHz) |
| Range | 30 m line-of-sight |
| Mode | Station (client) |
| Security | WPA2-PSK / WPA3 |
| Power | TX: 250 mA peak, RX: 80 mA, Sleep: 5 µA |

### 8.2 WiFi Use Cases

| Use Case | Frequency | Data | Protocol |
|---|---|---|---|
| OTA firmware update | Monthly | ~512 KB | HTTP(S) download |
| AI model update | Quarterly | ~3 MB | HTTP(S) download |
| Log upload | On demand | ~100 KB | HTTP(S) upload |
| Emergency SMS relay | On event | ~1 KB | HTTP(S) → Twilio API |
| Time sync | Daily | ~0.5 KB | NTP |

### 8.3 WiFi Connection Flow

```
Arduino Q                ESP32-C3
    │                      │
    │ AT+CWJAP="SSID","pw" │
    ├─────────────────────▶│
    │                      ├─── Connect to AP
    │                      │   (3-15 seconds)
    │◀─────────────────────┤ OK / ERROR
    │                      │
    │ AT+CIPSTART="TCP",   │
    │   "ota.example.com",80│
    ├─────────────────────▶│
    │                      ├─── TCP connect
    │◀─────────────────────┤ CONNECT
    │                      │
    │ AT+CIPSEND=512       │
    ├─────────────────────▶│
    │ "GET /firmware.bin"  │─── HTTP request
    │ data                 │
    │◀─────────────────────┤ +IPD,len:data
    │                      │
    │ (Receive firmware    │
    │  chunk by chunk)     │
    │                      │
    │ [After full RX]      │
    │ Verify CRC, reboot   │
    │ into bootloader      │
    └──────────────────────┘
```

---

## 9. Internal IPC

### 9.1 IPC Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     IPC MECHANISMS                               │
├────────────┬──────────────────┬────────────┬────────────────────┤
│ Mechanism  │ Use Case         │ Data Size  │ Latency            │
├────────────┼──────────────────┼────────────┼────────────────────┤
│ Queue      │ Task-to-task msg │ ≤ 128 B    │ ~50 µs (queue)    │
│ Event Group│ Synchronization  │ 32 flags   │ ~10 µs (set/wait) │
│ Task Notify │ ISR-to-task     │ 1 uint32   │ ~5 µs              │
│ Semaphore  │ Resource sharing │ N/A        │ ~15 µs             │
│ Mutex      │ Data protection  │ N/A        │ ~20 µs (with prio) │
│ Stream     │ Large data       │ Variable   │ ~100 µs            │
│ Buffer     │ (audio)         │            │                    │
│ Message Bus │ Pub/Sub events │ ≤ 256 B    │ ~30 µs (publish)  │
└────────────┴──────────────────┴────────────┴────────────────────┘
```

---

## 10. Thread Communication

### 10.1 Inter-Task Data Flow

```
┌────────────┐   Queue    ┌──────────────┐   Queue    ┌──────────┐
│ imu_task   │───────────▶│ AI Inference │───────────▶│ decision │
│ (priority 4)│           │ (priority 4) │           │ (pri 4)  │
└────────────┘            └──────────────┘           └────┬─────┘
                                                          │Queue
┌────────────┐   Queue    ┌──────────────┐               │
│ camera_task│───────────▶│ AI Inference │               │
│ (priority 4)│           └──────────────┘               │
└────────────┘                                           │
                                                          v
┌────────────┐   Queue    ┌──────────────┐   Queue    ┌──────────┐
│ audio_task │───────────▶│ voice_task   │───────────▶│ decision │
│ (priority 3)│           │ (priority 3) │           └──────────┘
└────────────┘            └──────────────┘
                                                          │Queue
┌────────────┐   Queue    ┌──────────────┐               │
│ gps_task   │───────────▶│ nav_task     │───────────────┘
│ (priority 3)│           │ (priority 3) │
└────────────┘            └──────────────┘
```

---

## 11. Shared Memory

### 11.1 Shared Data Regions

| Region | Size | Protected By | Written By | Read By |
|---|---|---|---|---|
| Latest IMU data | 64 B | Mutex | imu_task | All readers |
| Latest GPS data | 128 B | Mutex | gps_task | nav_task, decision |
| Latest depth frame | 450 KB | Double buffer | camera_task | ai_task |
| Latest AI results | 1 KB | Mutex | ai_postprocess | decision, voice |
| Audio output buffer | 96 KB | Double buffer | voice_task | audio_out_task |
| System config | 16 KB | Mutex | settings_app | All modules |
| Context struct | 256 B | Mutex | decision_task | All apps |

### 11.2 Double Buffer Strategy

```
Depth Frame (450 KB):
  ┌────────────┐    ┌────────────┐
  │ Buffer A   │    │ Buffer B   │
  │ (USB DMA)  │    │ (USB DMA)  │
  └──────┬─────┘    └──────┬─────┘
         │                 │
    camera_task:      ai_task:
    fill A while      process B
    B is processing   while A fills
         │                 │
         └────────┬────────┘
                  │
                  v
         ┌────────────────┐
         │ Swap pointer   │
         │ (atomic write) │
         └────────────────┘
```

---

## 12. Queues

### 12.1 Queue Configuration

| Queue Name | Length | Item Size | Type | Purpose |
|---|---|---|---|---|
| q_imu_data | 32 | 64 B | Queue | IMU samples to AI + decision |
| q_camera_frame | 2 | 4 B (ptr) | Queue | Frame buffer pointers |
| q_ai_result | 16 | 128 B | Queue | Detection results |
| q_gps_data | 10 | 128 B | Queue | GPS position updates |
| q_nav_cmd | 8 | 64 B | Queue | Navigation commands |
| q_voice_cmd | 8 | 64 B | Queue | Parsed voice commands |
| q_audio_out | 16 | 512 B | Queue | TTS audio chunks |
| q_bms_event | 4 | 16 B | Queue | BMS fault events |
| q_system_event | 16 | 32 B | Queue | System events (shutdown) |
| q_log | 64 | 128 B | Queue | Log messages |
| q_wifi_cmd | 8 | 128 B | Queue | WiFi AT commands |
| q_wifi_resp | 8 | 256 B | Queue | WiFi responses |

---

## 13. Publish Subscribe

### 13.1 Subscriber Registry

```
┌─────────────────────────────────────────────────────────────┐
│                    MESSAGE BUS SUBSCRIBERS                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ MSG_IMU_DATA → [ai_task, decision_task, safety_task]        │
│                                                             │
│ MSG_GPS_POSITION → [nav_task, safety_task, decision_task]   │
│                                                             │
│ MSG_DEPTH_FRAME → [ai_task]                                 │
│                                                             │
│ MSG_OBJECT_DETECTED → [decision_task, nav_task]             │
│                                                             │
│ MSG_OBSTACLE_ALERT → [decision_task, safety_task]           │
│                                                             │
│ MSG_FACE_RECOGNIZED → [voice_task, decision_task]           │
│                                                             │
│ MSG_TEXT_READ → [voice_task]                                │
│                                                             │
│ MSG_VOICE_COMMAND → [decision_task]                         │
│                                                             │
│ MSG_VOICE_WAKE_WORD → [voice_task, decision_task]           │
│                                                             │
│ MSG_NAV_DIRECTION → [decision_task, voice_task]             │
│                                                             │
│ MSG_BATTERY_LEVEL → [decision_task, battery_task]           │
│                                                             │
│ MSG_BMS_FAULT → [safety_task, decision_task]                │
│                                                             │
│ MSG_FALL_DETECTED → [safety_task, decision_task,            │
│                      nav_task]                              │
│                                                             │
│ MSG_GEO_ALERT → [safety_task, decision_task]                │
│                                                             │
│ MSG_SYSTEM_ERROR → [log_task, decision_task]                │
│                                                             │
│ MSG_SYSTEM_SHUTDOWN → [all tasks]                           │
│                                                             │
│ MSG_CONFIG_CHANGE → [all tasks]                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 14. Timing Diagrams

### 14.1 Complete Sensor-to-Speech Timing

```
Time (ms):    0    50    100    150    200    250    300    350    400
             │     │      │      │      │      │      │      │      │
Camera:      [Capture]──[USB xfer]──[Preproc]──[Inference]──[Postproc]
             │     │      │      │      │      │      │      │      │
             │  IMU:  [sample] [sample] [sample] [sample] [sample]
             │     │      │      │      │      │      │      │      │
             │  GPS:        [position read]
             │     │      │      │      │      │      │      │      │
Decision:    │     │      │      │          [eval] [queue] [format]
             │     │      │      │      │      │      │      │      │
Voice:       │     │      │      │      │      │      │ [TTS] [I2S]
             │     │      │      │      │      │      │      │      │
Speaker:     │     │      │      │      │      │      │      │ [AUDIO]
             │     │      │      │      │      │      │      │      │
             ├─────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┤
             │             TOTAL LATENCY: ~200 ms                    │
```

---

## 15. Protocol Diagrams

### 15.1 Message Bus Protocol

```
Publisher                     Message Bus                    Subscriber
    │                            │                              │
    │  msg_bus_publish(          │                              │
    │    MSG_OBSTACLE_ALERT,     │                              │
    │    &obstacle,              │                              │
    │    sizeof(obstacle))       │                              │
    ├────────────────────────────▶                              │
    │                            │                              │
    │                            │  msg_bus_dispatch():         │
    │                            │  for each subscriber:        │
    │                            │    xQueueSend(q, &msg, 0)   │
    │                            ├──────────────────────────────│
    │                            │                              │
    │                            │  (subscriber task wakes)     │
    │                            │                              │
    │                            │  msg_bus_receive(            │
    │                            │    &msg, timeout)            │
    │                            │◀─────────────────────────────│
    │                            │                              │
    │                            │  process obstacle alert      │
    │                            │  → speak "Obstacle ahead"   │
```

---

## 16. Data Packets

### 16.1 Message Bus Packet Format

```c
// Maximum message size: 256 bytes
typedef struct __attribute__((packed)) {
    uint32_t     magic;          // 0x474C5353 ("GLSS")
    uint16_t     crc16;          // CRC-16-IBM of payload
    uint8_t      message_type;   // MSG_xxx enum
    uint8_t      priority;       // 0-3
    uint32_t     timestamp_ms;   // System tick at publish
    uint16_t     payload_len;    // Bytes of payload (0-240)
    uint8_t      payload[240];   // Message data
    uint8_t      _pad;           // Padding to 4-byte align
} __attribute__((packed)) message_bus_packet_t;
// Total: 256 bytes
```

### 16.2 Obstacle Alert Packet Example

```c
// Actual payload for MSG_OBSTACLE_ALERT
typedef struct __attribute__((packed)) {
    uint8_t      class_id;       // CLASS_PERSON = 0, CLASS_STAIRS = 1, ...
    float        distance_m;     // 1.23 m
    float        angle_deg;      // -15.5 (left of center)
    bool         is_moving;      // false
    uint8_t      hazard_level;   // 2 (HAZARD_LEVEL_2)
    float        estimated_time_s; // Time to impact at current speed
} obstacle_payload_t;
// Size: 16 bytes
```

---

## 17. Synchronization

### 17.1 Mutex Usage

| Mutex | Protects | Priority Inheritance |
|---|---|---|
| mtx_imu_data | Latest IMU read | Yes |
| mtx_gps_data | Latest GPS position | Yes |
| mtx_ai_results | Latest detection list | Yes |
| mtx_config | System configuration | Yes |
| mtx_context | Context struct | Yes |
| mtx_flash | Flash memory access | Yes |
| mtx_voice | TTS engine state | Yes |

All mutexes use priority inheritance (`mutex_type_priority_inheritance`) to prevent priority inversion.

### 17.2 Critical Sections

```c
// ISR-safe data read: critical section for multi-word data
imu_data_t safe_read_imu(void) {
    imu_data_t data;
    taskENTER_CRITICAL();
    memcpy(&data, &latest_imu, sizeof(imu_data_t));
    taskEXIT_CRITICAL();
    return data;
}

// Mutex-protected config write
int safe_write_config(config_key_t key, const char* value) {
    if (xSemaphoreTake(mtx_config, pdMS_TO_TICKS(100)) == pdTRUE) {
        int result = flash_write(key, value);
        xSemaphoreGive(mtx_config);
        return result;
    }
    return -1;  // Timeout
}
```

---

## 18. Communication Priorities

### 18.1 Bus Arbitration Rules

| Priority | Bus | Messages | Max Latency |
|---|---|---|---|
| 1 (highest) | I2C | IMU data (fall detection) | 1 ms |
| 2 | Internal IPC | Critical alerts (fall, hazard) | 5 ms |
| 3 | USB | Depth frame (obstacle detection) | 10 ms |
| 4 | UART1 | Emergency SMS (BLE relay) | 100 ms |
| 5 | SPI | GPS position | 10 ms |
| 6 | Internal IPC | Navigation updates | 100 ms |
| 7 | I2S | Audio output | 1 ms (DMA) |
| 8 | UART1 | Voice commands | 200 ms |
| 9 | UART0 | Debug logs | Best effort |
| 10 (lowest) | WiFi | OTA, log upload | Best effort |

### 18.2 Priority Inheritance Example

```
Scenario: Low-priority log_task holds mtx_flash.
          High-priority safety_task needs mtx_flash for fall log.

Without PI:           With PI:
  log_task (pri 1)      log_task (pri 1 → temporarily pri 4)
    holds mtx_flash        holds mtx_flash (inherits safety's priority)
  safety (pri 4)         safety (pri 4)
    blocked on mtx_flash    blocked on mtx_flash
  medium (pri 2)         medium (pri 2)
    can preempt log_task    is blocked by log_task (now pri 4)
    → safety starved        → safety gets flash sooner
```

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior Embedded Systems Engineer | Initial draft |

---

*End of Document — COMM-ARCH-001*
