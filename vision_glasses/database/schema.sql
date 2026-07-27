-- VISION+ Database Schema
-- SQLite database for persistent storage

PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

-- User preferences and settings
CREATE TABLE IF NOT EXISTS user_preferences (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL,
    value TEXT NOT NULL,
    data_type TEXT DEFAULT 'string',
    description TEXT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Known faces for Alzheimer's mode (with consent)
CREATE TABLE IF NOT EXISTS known_faces (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    relationship TEXT,
    embedding BLOB NOT NULL,
    confidence_threshold REAL DEFAULT 0.7,
    consent_given BOOLEAN DEFAULT 1,
    consent_date TIMESTAMP,
    last_seen TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Medication schedule for Alzheimer's mode
CREATE TABLE IF NOT EXISTS medication_schedule (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    dosage TEXT,
    frequency TEXT NOT NULL,  -- daily, weekly, custom cron
    times TEXT NOT NULL,      -- JSON array of HH:MM times
    days_of_week TEXT,        -- JSON array [0-6] for weekly
    start_date DATE,
    end_date DATE,
    instructions TEXT,
    active BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Medication log for adherence tracking
CREATE TABLE IF NOT EXISTS medication_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    medication_id INTEGER NOT NULL,
    scheduled_time TIMESTAMP NOT NULL,
    taken_time TIMESTAMP,
    status TEXT DEFAULT 'pending',  -- pending, taken, missed, snoozed
    notes TEXT,
    FOREIGN KEY (medication_id) REFERENCES medication_schedule(id)
);

-- Appointments and calendar events
CREATE TABLE IF NOT EXISTS appointments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    description TEXT,
    location TEXT,
    start_time TIMESTAMP NOT NULL,
    end_time TIMESTAMP,
    recurrence TEXT,  -- none, daily, weekly, monthly, yearly
    reminder_minutes_before INTEGER DEFAULT 30,
    alert_given BOOLEAN DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Navigation history and favorites
CREATE TABLE IF NOT EXISTS navigation_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    destination_name TEXT,
    destination_lat REAL,
    destination_lon REAL,
    start_lat REAL,
    start_lon REAL,
    distance_m REAL,
    duration_s INTEGER,
    mode TEXT,  -- walking, transit, etc.
    completed BOOLEAN DEFAULT 0,
    started_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    completed_at TIMESTAMP
);

-- Favorite/saved locations
CREATE TABLE IF NOT EXISTS saved_locations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    latitude REAL NOT NULL,
    longitude REAL NOT NULL,
    address TEXT,
    category TEXT,  -- home, work, hospital, pharmacy, etc.
    notes TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Emergency contacts
CREATE TABLE IF NOT EXISTS emergency_contacts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    phone TEXT NOT NULL,
    relationship TEXT,
    priority INTEGER DEFAULT 1,
    notify_on_fall BOOLEAN DEFAULT 1,
    notify_on_wander BOOLEAN DEFAULT 1,
    notify_on_emergency BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Emergency events log
CREATE TABLE IF NOT EXISTS emergency_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_type TEXT NOT NULL,  -- fall, wander, emergency_button, low_battery
    latitude REAL,
    longitude REAL,
    details TEXT,
    contacts_notified TEXT,  -- JSON array of contact IDs
    resolved BOOLEAN DEFAULT 0,
    resolved_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Voice settings
CREATE TABLE IF NOT EXISTS voice_settings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tts_engine TEXT DEFAULT 'piper',
    tts_voice TEXT DEFAULT 'en_US-lessac-medium',
    tts_speed REAL DEFAULT 1.0,
    tts_volume REAL DEFAULT 0.8,
    stt_engine TEXT DEFAULT 'vosk',
    stt_model TEXT DEFAULT 'vosk-model-small-en-us-0.15',
    wake_word TEXT DEFAULT 'vision',
    language TEXT DEFAULT 'en-US',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Device settings
CREATE TABLE IF NOT EXISTS device_settings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL,
    value TEXT NOT NULL,
    description TEXT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Detection and navigation logs (for analytics/debugging)
CREATE TABLE IF NOT EXISTS detection_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    object_class TEXT NOT NULL,
    confidence REAL,
    distance_m REAL,
    x_min INTEGER,
    y_min INTEGER,
    x_max INTEGER,
    y_max INTEGER,
    hazard_level TEXT,  -- critical, high, medium, low
    action_taken TEXT,
    gps_lat REAL,
    gps_lon REAL
);

-- Fall detection events
CREATE TABLE IF NOT EXISTS fall_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    impact_g REAL,
    freefall_duration_ms INTEGER,
    orientation_change_deg REAL,
    immobility_duration_s INTEGER,
    false_positive BOOLEAN DEFAULT 0,
    emergency_triggered BOOLEAN DEFAULT 0,
    latitude REAL,
    longitude REAL
);

-- System events and errors
CREATE TABLE IF NOT EXISTS system_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    level TEXT NOT NULL,  -- DEBUG, INFO, WARNING, ERROR, CRITICAL
    module TEXT NOT NULL,
    message TEXT NOT NULL,
    details TEXT
);

-- Power/battery logs
CREATE TABLE IF NOT EXISTS battery_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    voltage REAL,
    current REAL,
    percentage INTEGER,
    temperature REAL,
    charging BOOLEAN,
    power_mode TEXT
);

-- Performance metrics
CREATE TABLE IF NOT EXISTS performance_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    module TEXT NOT NULL,
    metric_name TEXT NOT NULL,
    value REAL NOT NULL,
    unit TEXT
);

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_detection_log_timestamp ON detection_log(timestamp);
CREATE INDEX IF NOT EXISTS idx_detection_log_class ON detection_log(object_class);
CREATE INDEX IF NOT EXISTS idx_fall_events_timestamp ON fall_events(timestamp);
CREATE INDEX IF NOT EXISTS idx_system_logs_timestamp ON system_logs(timestamp);
CREATE INDEX IF NOT EXISTS idx_system_logs_level ON system_logs(level);
CREATE INDEX IF NOT EXISTS idx_battery_log_timestamp ON battery_log(timestamp);
CREATE INDEX IF NOT EXISTS idx_performance_timestamp ON performance_metrics(timestamp);
CREATE INDEX IF NOT EXISTS idx_medication_schedule_time ON medication_schedule(times);
CREATE INDEX IF NOT EXISTS idx_appointments_time ON appointments(start_time);

-- Triggers for updated_at
CREATE TRIGGER IF NOT EXISTS update_known_faces_timestamp
AFTER UPDATE ON known_faces
BEGIN
    UPDATE known_faces SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS update_medication_schedule_timestamp
AFTER UPDATE ON medication_schedule
BEGIN
    UPDATE medication_schedule SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS update_appointments_timestamp
AFTER UPDATE ON appointments
BEGIN
    UPDATE appointments SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id;
END;

-- Default user preferences
INSERT OR IGNORE INTO user_preferences (key, value, data_type, description) VALUES
    ('mode', 'blind_assist', 'string', 'Operating mode: blind_assist, alzheimer_assist, dual_mode'),
    ('audio_volume', '0.8', 'float', 'Audio output volume 0.0-1.0'),
    ('tts_speed', '1.0', 'float', 'TTS speech rate'),
    ('hazard_audio_cooldown_ms', '3000', 'integer', 'Minimum time between hazard announcements'),
    ('max_announcements_per_minute', '10', 'integer', 'Maximum audio announcements per minute'),
    ('gps_update_interval_ms', '200', 'integer', 'GPS update interval in milliseconds'),
    ('fall_detection_enabled', '1', 'boolean', 'Enable fall detection'),
    ('alzheimer_mode_enabled', '0', 'boolean', 'Enable Alzheimer assistance mode'),
    ('geofence_enabled', '0', 'boolean', 'Enable geofence monitoring'),
    ('safe_zone_radius_m', '500', 'integer', 'Safe zone radius in meters'),
    ('low_battery_threshold', '20', 'integer', 'Low battery warning threshold %'),
    ('critical_battery_threshold', '10', 'integer', 'Critical battery threshold %'),
    ('wake_word', 'vision', 'string', 'Wake word for voice commands'),
    ('language', 'en-US', 'string', 'System language'),
    ('vibration_enabled', '1', 'boolean', 'Enable haptic feedback'),
    ('bone_conduction_gain', '0.7', 'float', 'Bone conduction speaker gain'),
    ('indoor_mode_distance_m', '20', 'integer', 'Switch to indoor mode within this distance of destination'),
    ('obstacle_announce_distance_m', '2.0', 'float', 'Announce obstacles within this distance'),
    ('critical_distance_m', '0.5', 'float', 'Critical hazard distance threshold'),
    ('high_distance_m', '1.0', 'float', 'High priority hazard distance threshold'),
    ('medium_distance_m', '2.0', 'float', 'Medium priority hazard distance threshold'),
    ('low_distance_m', '4.0', 'float', 'Low priority hazard distance threshold');

-- Default voice settings
INSERT OR IGNORE INTO voice_settings (tts_engine, tts_voice, tts_speed, tts_volume, stt_engine, stt_model, wake_word, language)
VALUES ('piper', 'en_US-lessac-medium', 1.0, 0.8, 'vosk', 'vosk-model-small-en-us-0.15', 'vision', 'en-US');
