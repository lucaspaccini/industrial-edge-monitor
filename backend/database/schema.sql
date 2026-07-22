CREATE TABLE IF NOT EXISTS telemetry
(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL DEFAULT 'legacy-device',
    timestamp TEXT NOT NULL,
    temperature REAL NOT NULL,
    humidity REAL NOT NULL,
    machine_status TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS devices
(
    device_id TEXT PRIMARY KEY,
    reported_availability TEXT NOT NULL DEFAULT 'offline'
        CHECK (reported_availability IN ('online', 'offline')),
    last_seen TEXT,
    availability_updated_at TEXT
);

CREATE TABLE IF NOT EXISTS device_health_current
(
    device_id TEXT PRIMARY KEY,
    payload TEXT NOT NULL,
    device_timestamp TEXT,
    status TEXT NOT NULL CHECK (status IN ('healthy', 'degraded')),
    availability TEXT NOT NULL CHECK (availability IN ('online', 'offline')),
    components TEXT NOT NULL,
    counters TEXT NOT NULL,
    metrics TEXT NOT NULL,
    received_at TEXT NOT NULL,
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);

CREATE INDEX IF NOT EXISTS idx_telemetry_device_timestamp
ON telemetry(device_id, timestamp DESC);

CREATE INDEX IF NOT EXISTS idx_devices_last_seen ON devices(last_seen);
