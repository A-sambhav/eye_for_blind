# Database Design Document

**Document ID:** DB-ARCH-001

**Product:** AI-Powered Smart Glasses for Visually Impaired Users and Alzheimer's Patients

**Author:** Senior Data Engineer

**Revision:** 0.1

**Date:** 2026-07-27

---

## Table of Contents

1. Executive Summary
2. Storage Architecture
3. ER Diagram
4. Tables
5. Relationships
6. Indexes
7. Triggers
8. Views
9. SQL Schema
10. Table Descriptions
11. Optimization Recommendations

---

## 1. Executive Summary

The smart glasses use flash-based structured storage for persistent data. Given the resource constraints (no full SQLite), the system uses a custom lightweight key-value store and binary file format over external QSPI flash (W25Q128JV, 16 MB). The database layer manages configuration, face embeddings, routes, reminders, and logs. Design prioritizes write endurance (limited flash cycles) and read speed.

---

## 2. Storage Architecture

```
+---------------------------------------------------------------------+
|                    STORAGE ARCHITECTURE                              |
+---------------------------------------------------------------------+
|                                                                     |
|  QSPI FLASH (W25Q128JV, 16 MB)                                     |
|  ┌────────────────────────────────────────────────────────────────┐ |
|  |  Region              |  Size    |  Type     |  Endurance      | |
|  |──────────────────────+──────────+───────────+─────────────────| |
|  |  AI Models           |  3.0 MB  |  Read     |  Rarely written | |
|  |  TTS Phoneme Samples |  8.0 MB  |  Read     |  Never written  | |
|  |  System Config       | 64 KB    |  JSON     |  ~100K writes   | |
|  |  Face Database       | 256 KB   |  Binary   |  ~10K writes    | |
|  |  Routes/Locations    | 256 KB   |  Binary   |  ~10K writes    | |
|  |  Reminders           | 64 KB    |  JSON     |  ~10K writes    | |
|  |  Emergency Contacts  | 16 KB    |  JSON     |  ~1K writes     | |
|  |  Navigation History  | 512 KB   |  Binary   |  ~100K writes   | |
|  |  System Logs         | 1.0 MB   |  Binary   |  ~500K writes   | |
|  |  AI Inference Logs   | 512 KB   |  Binary   |  ~500K writes   | |
|  |  Spare / Future      | 2.0 MB   |  —        |  —              | |
|  |  Total               | 16 MB    |           |                 | |
|  └────────────────────────────────────────────────────────────────┘ |
|                                                                     |
|  SOFTWARE LAYER                                                     |
|  ┌────────────────────────────────────────────────────────────────┐ |
|  |  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐  | |
|  |  │ Flash    │  │ Config   │  │ DB       │  │ Log          │  | |
|  |  │ Storage  │  │ Manager  │  │ Manager  │  │ Writer       │  | |
|  |  │ (driver) │  │ (JSON)   │  │ (binary) │  │ (circular)   │  | |
|  |  └──────────┘  └──────────┘  └──────────┘  └──────────────┘  | |
|  └────────────────────────────────────────────────────────────────┘ |
|                                                                     |
+---------------------------------------------------------------------+
```

---

## 3. ER Diagram

```
+------------------+       +-------------------+       +------------------+
|    CONFIG        |       |     FACES         |       |   EMERGENCY_     |
|------------------|       |-------------------|       |   CONTACTS       |
| config_key (PK)  |       | face_id (PK)      |       |------------------|
| config_value     |       | name              |       | contact_id (PK)  |
| updated_at       |       | embedding_hash[8] |       | name             |
|                  |       | created_at        |       | phone            |
|                  |       | last_seen         |       | relationship     |
|                  |       | thumbnail_ref     |       | notify_on_fall   |
|                  |       |                   |       | sms_consent      |
+------------------+       +-------------------+       +--------+---------+
                                                                  |
                                                                  |
+------------------+       +-------------------+                  |
|    REMINDERS     |       |    ROUTES         |                  |
|------------------|       |-------------------|                  |
| reminder_id (PK) |       | route_id (PK)     |                  |
| hour             |       | name              |                  |
| minute           |       | type              |  (home, hospital) |
| day_of_week      |       | waypoints (blob)  |                  |
| day_of_month     |       | created_at        |                  |
| description      |       | last_used         |                  |
| active           |       | favorite          |                  |
| geo_lat          |       +-------------------+                  |
| geo_lon          |                                              |
| geo_radius_m     |       +-------------------+                  |
| medicine_flag    |       | NAV_HISTORY       |                  |
+------------------+       |-------------------|                  |
                           | entry_id (PK)     |                  |
+------------------+       | timestamp         |                  |
|   SYSTEM_LOGS    |       | latitude          |                  |
|------------------|       | longitude         |                  |
| log_id (PK)      |       | altitude          |                  |
| timestamp        |       | heading           |                  |
| level            |       | speed             |                  |
| module           |       | accuracy          |                  |
| message          |       | source (GPS/IMU)  |                  |
+------------------+       +-------------------+                  |

+------------------+       +------------------+
|   AI_LOGS        |       |   SPEECH_LOGS    |
|------------------|       |------------------|
| log_id (PK)      |       | log_id (PK)      |
| timestamp        |       | timestamp        |
| model_name       |       | direction        | (in/out)
| inference_time_ms|       | text             |
| confidence       |       | intent           |
| class_id         |       | confidence       |
| object_count     |       | wake_word_used   |
+------------------+       +------------------+

+------------------+
|   BATTERY_LOGS   |
|------------------|
| log_id (PK)      |
| timestamp        |
| voltage          |
| current_ma       |
| level_pct        |
| temperature      |
| charging         |
+------------------+
```

---

## 4. Tables

### 4.1 Config Table

| Column | Type | Description |
|---|---|---|
| config_key | TEXT (32) | Primary key |
| config_value | TEXT (256) | Value (JSON-encoded) |
| updated_at | INTEGER | Unix timestamp |

### 4.2 Faces Table

| Column | Type | Description |
|---|---|---|
| face_id | INTEGER (PK) | Auto-increment |
| name | TEXT (64) | Person's name |
| embedding_hash[8] | BLOB (256) | 8 × 32-bit ints = irreversible hash |
| created_at | INTEGER | Unix timestamp |
| last_seen | INTEGER | Last detection timestamp |
| thumbnail_ref | INTEGER | Reserved for future image reference |

### 4.3 Emergency Contacts Table

| Column | Type | Description |
|---|---|---|
| contact_id | INTEGER (PK) | Auto-increment |
| name | TEXT (64) | Contact name |
| phone | TEXT (20) | Phone number |
| relationship | TEXT (32) | "Spouse", "Child", "Doctor" |
| notify_on_fall | INTEGER (bool) | 1 = send SMS on fall |
| sms_consent | INTEGER (bool) | 1 = user consented |

### 4.4 Reminders Table

| Column | Type | Description |
|---|---|---|
| reminder_id | INTEGER (PK) | Auto-increment |
| hour | INTEGER | 0-23 |
| minute | INTEGER | 0-59 |
| day_of_week | INTEGER | Bitmask: 1=Mon, 2=Tue, 4=Wed, ... |
| day_of_month | INTEGER | 0=every, 1-31 |
| description | TEXT (128) | Reminder text |
| active | INTEGER (bool) | 1 = active |
| geo_lat | REAL | Optional geo-trigger |
| geo_lon | REAL | Optional geo-trigger |
| geo_radius_m | REAL | Geo-fence radius |
| medicine_flag | INTEGER (bool) | 1 = is medicine reminder |

### 4.5 Routes Table

| Column | Type | Description |
|---|---|---|
| route_id | INTEGER (PK) | Auto-increment |
| name | TEXT (64) | "Home", "Hospital", etc. |
| type | INTEGER | 0=user, 1=frequent, 2=pre-loaded |
| waypoints | BLOB | Binary: waypoint_t[128] |
| created_at | INTEGER | Unix timestamp |
| last_used | INTEGER | Last navigation timestamp |
| favorite | INTEGER (bool) | 1 = pinned |

### 4.6 Nav History Table

| Column | Type | Description |
|---|---|---|
| entry_id | INTEGER (PK) | Auto-increment |
| timestamp | INTEGER | Unix timestamp |
| latitude | REAL | WGS84 |
| longitude | REAL | WGS84 |
| altitude | REAL | Meters |
| heading | REAL | Degrees 0-360 |
| speed | REAL | m/s |
| accuracy | REAL | Meters CEP |
| source | INTEGER | 0=GPS, 1=IMU, 2=fused |

### 4.7 System Logs Table

| Column | Type | Description |
|---|---|---|
| log_id | INTEGER (PK) | Auto-increment |
| timestamp | INTEGER | Unix timestamp |
| level | INTEGER | 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG |
| module | TEXT (16) | "AI", "NAV", "VOICE", etc. |
| message | TEXT (256) | Log message |

### 4.8 AI Logs Table

| Column | Type | Description |
|---|---|---|
| log_id | INTEGER (PK) | Auto-increment |
| timestamp | INTEGER | Unix timestamp |
| model_name | TEXT (32) | "object_detection", "face" |
| inference_time_ms | INTEGER | Inference duration |
| confidence | REAL | Max confidence |
| class_id | INTEGER | Detected class |
| object_count | INTEGER | Number of objects |

### 4.9 Speech Logs Table

| Column | Type | Description |
|---|---|---|
| log_id | INTEGER (PK) | Auto-increment |
| timestamp | INTEGER | Unix timestamp |
| direction | INTEGER | 0=user→system, 1=system→user |
| text | TEXT (256) | Transcribed text |
| intent | INTEGER | Intent enum value |
| confidence | REAL | Recognition confidence |
| wake_word_used | INTEGER (bool) | Was wake word used? |

### 4.10 Battery Logs Table

| Column | Type | Description |
|---|---|---|
| log_id | INTEGER (PK) | Auto-increment |
| timestamp | INTEGER | Unix timestamp |
| voltage | REAL | Battery voltage |
| current_ma | INTEGER | Current draw mA |
| level_pct | INTEGER | 0-100 |
| temperature | REAL | Battery temperature °C |
| charging | INTEGER (bool) | 1 = charging |

---

## 5. Relationships

```
faces(face_id)                  ← no direct FK relations (standalone)
emergency_contacts(contact_id)  ← referenced by config (emergency_contact_id)
reminders(reminder_id)           ← standalone, referenced by config
routes(route_id)                ← standalone, referenced by config
nav_history(entry_id)           ← standalone, circular buffer
system_logs(log_id)             ← standalone, circular buffer
ai_logs(log_id)                 ← standalone, circular buffer
speech_logs(log_id)             ← standalone, circular buffer
battery_logs(log_id)            ← standalone, circular buffer

No foreign key constraints (performance). Application-level referential integrity.
```

---

## 6. Indexes

| Table | Index | Column(s) | Type | Purpose |
|---|---|---|---|---|
| nav_history | idx_nav_timestamp | timestamp | ASC | Time-range queries |
| system_logs | idx_log_timestamp | timestamp | ASC | Time-range queries |
| system_logs | idx_log_level | level | ASC | Error filtering |
| ai_logs | idx_ai_timestamp | timestamp | ASC | Time-range queries |
| speech_logs | idx_speech_timestamp | timestamp | ASC | Time-range queries |
| battery_logs | idx_battery_timestamp | timestamp | ASC | Power analysis |

---

## 7. Triggers

No triggers are used. All application logic is in C code to minimize flash writes and avoid unexpected behavior during power loss.

---

## 8. Views

No views are used. Log tables are read directly by diagnostic tools via BLE/UART.

---

## 9. SQL Schema

```sql
-- Note: SQLite is not used on-device (too heavy).
-- Schema is for reference/documentation. On-device storage
-- uses custom binary format with equivalent structure.

-- Config
CREATE TABLE config (
    config_key   TEXT PRIMARY KEY,
    config_value TEXT NOT NULL,
    updated_at   INTEGER NOT NULL DEFAULT (strftime('%s','now'))
);

-- Faces
CREATE TABLE faces (
    face_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name             TEXT NOT NULL,
    embedding_hash   BLOB NOT NULL,  -- 8 × uint32 = 32 bytes
    created_at       INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    last_seen        INTEGER,
    thumbnail_ref    INTEGER DEFAULT 0
);

-- Emergency Contacts
CREATE TABLE emergency_contacts (
    contact_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    name           TEXT NOT NULL,
    phone          TEXT NOT NULL,
    relationship   TEXT,
    notify_on_fall INTEGER DEFAULT 1,
    sms_consent    INTEGER DEFAULT 0
);

-- Reminders
CREATE TABLE reminders (
    reminder_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    hour           INTEGER NOT NULL CHECK(hour >= 0 AND hour <= 23),
    minute         INTEGER NOT NULL CHECK(minute >= 0 AND minute <= 59),
    day_of_week    INTEGER DEFAULT 127,  -- All days
    day_of_month   INTEGER DEFAULT 0,    -- Every day
    description    TEXT NOT NULL,
    active         INTEGER DEFAULT 1,
    geo_lat        REAL,
    geo_lon        REAL,
    geo_radius_m   REAL DEFAULT 50.0,
    medicine_flag  INTEGER DEFAULT 0
);

-- Routes
CREATE TABLE routes (
    route_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    name           TEXT NOT NULL,
    type           INTEGER DEFAULT 0,
    waypoints      BLOB,
    created_at     INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    last_used      INTEGER,
    favorite       INTEGER DEFAULT 0
);

-- Navigation History (circular: keep last 10000 entries)
CREATE TABLE nav_history (
    entry_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   INTEGER NOT NULL,
    latitude    REAL NOT NULL,
    longitude   REAL NOT NULL,
    altitude    REAL,
    heading     REAL,
    speed       REAL,
    accuracy    REAL,
    source      INTEGER DEFAULT 0
);

-- System Logs (circular: keep last 5000 entries)
CREATE TABLE system_logs (
    log_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   INTEGER NOT NULL,
    level       INTEGER NOT NULL,
    module      TEXT NOT NULL,
    message     TEXT NOT NULL
);

-- AI Logs (circular: keep last 5000 entries)
CREATE TABLE ai_logs (
    log_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp          INTEGER NOT NULL,
    model_name         TEXT NOT NULL,
    inference_time_ms  INTEGER,
    confidence         REAL,
    class_id           INTEGER,
    object_count       INTEGER
);

-- Speech Logs (circular: keep last 2000 entries)
CREATE TABLE speech_logs (
    log_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp       INTEGER NOT NULL,
    direction       INTEGER NOT NULL,
    text            TEXT NOT NULL,
    intent          INTEGER,
    confidence      REAL,
    wake_word_used  INTEGER DEFAULT 0
);

-- Battery Logs (circular: keep last 2000 entries)
CREATE TABLE battery_logs (
    log_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   INTEGER NOT NULL,
    voltage     REAL,
    current_ma  INTEGER,
    level_pct   INTEGER,
    temperature REAL,
    charging    INTEGER DEFAULT 0
);
```

---

## 10. Table Descriptions

### 10.1 Config

Stores all user-configurable settings as key-value pairs. Keys include:

| Key | Example Value | Description |
|---|---|---|
| volume | "7" | Audio volume 0-10 |
| wake_word | "hey glass" | Custom wake phrase |
| language | "en" | Language code |
| privacy_mode | "false" | Privacy mode state |
| navigation_voice | "concise" | "concise" or "verbose" |
| home_lat | "40.7128" | Home latitude |
| home_lon | "-74.0060" | Home longitude |
| home_address | "123 Main St" | Home address text |
| user_height_m | "1.75" | For step length |
| geo_fence_enabled | "true" | Safe zone feature |
| geo_fence_radius | "500" | Safe zone radius meters |
| fall_detection | "true" | Fall detection enabled |
| emergency_contact_id | "1" | Primary emergency contact |
| last_updated | "1700000000" | Config version |

### 10.2 Faces

Stores enrolled face information. The embedding_hash is an irreversible hash of the face embedding vector (generated by Edge Impulse face recognition model). Raw images are never stored. Face detection announces "Hello {name}" when a known face is recognized.

### 10.3 Emergency Contacts

Stores up to 20 emergency contacts. Used for fall detection alerts and user voice command "Call [contact]". SMS consent must be explicitly granted per contact.

### 10.4 Reminders

Supports time-based and geo-fence-based reminders. Medicine_flag enables special handling (repeat until confirmed). Geo-triggered reminders fire when entering/exiting a location (e.g., "Buy milk when near grocery store").

### 10.5 Routes

Stores frequently used routes. Pre-loaded routes include home, hospital, and pharmacy. User-created routes are synced from companion app via BLE. Waypoints stored as binary blob of up to 128 waypoints.

### 10.6 Nav History

Circular buffer of position history (last 10,000 entries). Used for:
- "Where have I been?" queries
- Caregiver review of patient movement
- GPS-denied path reconstruction
- Analyzing wandering patterns (Alzheimer's)

### 10.7 System Logs

Circular buffer (last 5,000 entries). ERROR level logs are always persisted. WARN/INFO logs persist until circular buffer wraps. DEBUG logs are never persisted.

### 10.8 AI Logs

Performance tracking for AI models. Used for:
- Monitoring inference time degradation
- Accuracy analysis
- Model update decisions

### 10.9 Speech Logs

Records all voice interactions (user commands and system responses). Used for debugging voice recognition issues and improving intent parsing.

### 10.10 Battery Logs

Records battery metrics every 5 minutes. Used for:
- Battery health monitoring
- Usage pattern analysis
- Runtime estimation calibration

---

## 11. Optimization Recommendations

| ID | Recommendation | Rationale | Priority |
|---|---|---|---|
| DB-OPT-001 | Use wear-leveling flash storage driver | QSPI flash has ~100K write cycles per sector; wear-leveling extends life 10× | High |
| DB-OPT-002 | Buffer writes in RAM; batch-flush every 60 seconds | Reduces flash write count by ~60× for logs | High |
| DB-OPT-003 | Use circular buffers for all log tables | Eliminates garbage collection; constant write time | High |
| DB-OPT-004 | Pre-allocate file regions at format time | Prevents fragmentation and out-of-space errors | Medium |
| DB-OPT-005 | CRC32 on every sector for corruption detection | Flash can develop bit errors over lifetime | Medium |
| DB-OPT-006 | Compress old logs (gzip on companion app, not on-device) | Frees space for more recent logs | Low |
| DB-OPT-007 | Reserve 10% spare space in each region | Flash needs free space for wear-leveling | Medium |
| DB-OPT-008 | Cache frequently read config in RAM (copy on boot) | Config read is on every boot + many runtime lookups | High |
| DB-OPT-009 | Use atomic sector swap for config writes | Prevents corruption on power loss during write | High |
| DB-OPT-010 | Throttle nav_history writes to 0.2 Hz when stationary | Reduces unnecessary writes by 80% | Medium |

### 11.1 Write Endurance Budget

| Table | Writes/day | Annual Writes | Estimated Life (100K cycles) |
|---|---|---|---|
| Config | 10 | 3,650 | 27 years |
| Faces | 1 | 365 | 273 years |
| Routes | 0.5 | 182 | 548 years |
| Nav History | 2,880 (0.2 Hz × 4 hrs walking) | 1,051,200 | ~35 days ❗ |
| System Logs | 500 | 182,500 | 200 days ❗ |
| AI Logs | 500 | 182,500 | 200 days ❗ |
| Speech Logs | 100 | 36,500 | 2.7 years |
| Battery Logs | 288 (5 min interval) | 105,120 | ~347 days |

**Critical finding:** Nav History, System Logs, and AI Logs will exceed flash endurance within 1 year if written at full rate. **Mitigation:** Use SRAM buffer with batch flash flush (reduce writes by 60×), and reduce log frequency.

---

## Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 0.1 | 2026-07-27 | Senior Data Engineer | Initial draft |

---

*End of Document — DB-ARCH-001*
