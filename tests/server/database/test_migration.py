import sqlite3

from backend.core.config import settings
from backend.database.init_db import initialize_database


def test_existing_telemetry_is_migrated_idempotently(tmp_path, monkeypatch):
    database = tmp_path / "legacy" / "legacy.db"
    database.parent.mkdir()
    with sqlite3.connect(database) as conn:
        conn.execute(
            "CREATE TABLE telemetry(id INTEGER PRIMARY KEY, timestamp TEXT NOT NULL, "
            "temperature REAL NOT NULL, humidity REAL NOT NULL, machine_status TEXT NOT NULL)"
        )
        conn.execute(
            "INSERT INTO telemetry(timestamp, temperature, humidity, machine_status) "
            "VALUES ('2026-01-01T00:00:00Z', 20, 40, 'unknown')"
        )
    monkeypatch.setattr(settings, "DATABASE_PATH", str(database))

    initialize_database()
    initialize_database()

    with sqlite3.connect(database) as conn:
        assert conn.execute("SELECT device_id FROM telemetry").fetchone()[0] == "legacy-device"
        device = conn.execute(
            "SELECT device_id, reported_availability FROM devices"
        ).fetchone()
        assert device == ("legacy-device", "offline")
        indexes = {row[1] for row in conn.execute("PRAGMA index_list(telemetry)")}
        assert "idx_telemetry_device_timestamp" in indexes


def test_initialize_database_creates_missing_parent_directory(tmp_path, monkeypatch):
    database = tmp_path / "missing" / "nested" / "telemetry.db"
    monkeypatch.setattr(settings, "DATABASE_PATH", str(database))

    initialize_database()

    assert database.is_file()
    with sqlite3.connect(database) as conn:
        assert conn.execute(
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'telemetry'"
        ).fetchone() == ("telemetry",)
