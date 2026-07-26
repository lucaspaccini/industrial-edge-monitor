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

CREATE TABLE IF NOT EXISTS alert_rules
(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL CHECK (length(trim(name)) BETWEEN 1 AND 100),
    device_id TEXT NOT NULL,
    metric TEXT NOT NULL CHECK (metric IN ('temperature', 'humidity')),
    operator TEXT NOT NULL CHECK (operator IN ('greater_than', 'less_than')),
    threshold REAL NOT NULL,
    duration_seconds INTEGER NOT NULL CHECK (duration_seconds >= 0),
    hysteresis REAL NOT NULL CHECK (hysteresis >= 0),
    severity TEXT NOT NULL CHECK (severity IN ('info', 'warning', 'critical')),
    enabled INTEGER NOT NULL CHECK (enabled IN (0, 1)),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    archived_at TEXT,
    FOREIGN KEY (device_id) REFERENCES devices(device_id),
    CHECK (archived_at IS NULL OR enabled = 0)
);

CREATE TABLE IF NOT EXISTS alert_rule_states
(
    rule_id INTEGER PRIMARY KEY,
    state TEXT NOT NULL CHECK (state IN ('normal', 'pending', 'active')),
    pending_since TEXT,
    pending_extreme_value REAL,
    last_evaluated_at TEXT,
    last_value REAL,
    last_telemetry_id INTEGER,
    FOREIGN KEY (rule_id) REFERENCES alert_rules(id)
);

CREATE TABLE IF NOT EXISTS alert_events
(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_id INTEGER NOT NULL,
    device_id TEXT NOT NULL,
    rule_name TEXT NOT NULL,
    metric TEXT NOT NULL,
    operator TEXT NOT NULL,
    threshold REAL NOT NULL,
    hysteresis REAL NOT NULL,
    duration_seconds INTEGER NOT NULL,
    severity TEXT NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('active', 'resolved')),
    started_at TEXT NOT NULL,
    activated_at TEXT NOT NULL,
    resolved_at TEXT,
    resolution_reason TEXT CHECK (
        resolution_reason IS NULL OR resolution_reason IN (
            'condition_recovered', 'rule_disabled', 'rule_updated',
            'rule_archived'
        )
    ),
    activation_value REAL NOT NULL,
    last_value REAL NOT NULL,
    extreme_value REAL NOT NULL,
    FOREIGN KEY (rule_id) REFERENCES alert_rules(id)
);

CREATE INDEX IF NOT EXISTS idx_alert_rules_device
ON alert_rules(device_id, archived_at, enabled, id);

CREATE UNIQUE INDEX IF NOT EXISTS idx_alert_rules_active_name
ON alert_rules(device_id, name) WHERE archived_at IS NULL;

CREATE INDEX IF NOT EXISTS idx_alert_rule_states_state
ON alert_rule_states(state, rule_id);

CREATE INDEX IF NOT EXISTS idx_alert_events_device_time
ON alert_events(device_id, activated_at DESC, id DESC);

CREATE INDEX IF NOT EXISTS idx_alert_events_rule_time
ON alert_events(rule_id, activated_at DESC, id DESC);

CREATE INDEX IF NOT EXISTS idx_alert_events_status
ON alert_events(status, severity, activated_at DESC);

CREATE UNIQUE INDEX IF NOT EXISTS idx_alert_events_one_active_per_rule
ON alert_events(rule_id) WHERE status = 'active';
