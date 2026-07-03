CREATE TABLE IF NOT EXISTS telemetry
(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    temperature REAL NOT NULL,
    humidity REAL NOT NULL,
    machine_status TEXT NOT NULL
);