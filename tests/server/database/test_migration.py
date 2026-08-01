import sqlite3
from concurrent.futures import ThreadPoolExecutor
from threading import Barrier

import backend.api.database as database_module
from backend.api.database import get_connection
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
        alert_tables = {
            row[0]
            for row in conn.execute(
                "SELECT name FROM sqlite_master WHERE type = 'table' "
                "AND name IN ('alert_rules', 'alert_rule_states', 'alert_events')"
            )
        }
        assert alert_tables == {
            "alert_rules",
            "alert_rule_states",
            "alert_events",
        }


def test_initialize_database_creates_missing_parent_directory(tmp_path, monkeypatch):
    database = tmp_path / "missing" / "nested" / "telemetry.db"
    monkeypatch.setattr(settings, "DATABASE_PATH", str(database))

    initialize_database()

    assert database.is_file()
    with sqlite3.connect(database) as conn:
        assert conn.execute(
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'telemetry'"
        ).fetchone() == ("telemetry",)
        assert conn.execute("PRAGMA journal_mode").fetchone()[0] == "wal"
    with get_connection() as conn:
        assert conn.execute("PRAGMA busy_timeout").fetchone()[0] == 30000


def test_relative_database_path_is_resolved_from_project_base(tmp_path, monkeypatch):
    monkeypatch.setattr(database_module, "BASE_DIR", tmp_path)
    monkeypatch.setattr(settings, "DATABASE_PATH", "state/telemetry.db")

    initialize_database()

    assert (tmp_path / "state" / "telemetry.db").is_file()


def test_clean_database_initialization_is_safe_when_called_concurrently(
    tmp_path, monkeypatch
):
    database = tmp_path / "concurrent" / "telemetry.db"
    monkeypatch.setattr(settings, "DATABASE_PATH", str(database))
    start = Barrier(2)

    def initialize_together(_):
        start.wait()
        return initialize_database()

    with ThreadPoolExecutor(max_workers=2) as executor:
        results = list(executor.map(initialize_together, range(2)))

    assert results == [None, None]
    with sqlite3.connect(database) as conn:
        assert conn.execute(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' "
            "AND name IN ('telemetry', 'devices', 'device_health_current')"
        ).fetchone()[0] == 3


def test_sprint13_archive_migration_preserves_rules_and_events(tmp_path, monkeypatch):
    database = tmp_path / "sprint13.db"
    with sqlite3.connect(database) as conn:
        conn.executescript(
            """
            CREATE TABLE devices(
                device_id TEXT PRIMARY KEY,
                reported_availability TEXT NOT NULL DEFAULT 'offline',
                last_seen TEXT,
                availability_updated_at TEXT
            );
            INSERT INTO devices(device_id) VALUES ('edge-01');
            CREATE TABLE alert_rules(
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                device_id TEXT NOT NULL,
                metric TEXT NOT NULL,
                operator TEXT NOT NULL,
                threshold REAL NOT NULL,
                duration_seconds INTEGER NOT NULL,
                hysteresis REAL NOT NULL,
                severity TEXT NOT NULL,
                enabled INTEGER NOT NULL,
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL,
                UNIQUE(device_id, name)
            );
            INSERT INTO alert_rules VALUES(
                1, 'Existing', 'edge-01', 'temperature', 'greater_than',
                30, 0, 1, 'warning', 1,
                '2026-07-26T10:00:00Z', '2026-07-26T10:00:00Z'
            );
            CREATE TABLE alert_rule_states(
                rule_id INTEGER PRIMARY KEY,
                state TEXT NOT NULL,
                pending_since TEXT,
                pending_extreme_value REAL,
                last_evaluated_at TEXT,
                last_value REAL,
                last_telemetry_id INTEGER
            );
            INSERT INTO alert_rule_states(rule_id, state) VALUES (1, 'normal');
            CREATE TABLE alert_events(
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
                status TEXT NOT NULL,
                started_at TEXT NOT NULL,
                activated_at TEXT NOT NULL,
                resolved_at TEXT,
                resolution_reason TEXT,
                activation_value REAL NOT NULL,
                last_value REAL NOT NULL,
                extreme_value REAL NOT NULL
            );
            INSERT INTO alert_events VALUES(
                1, 1, 'edge-01', 'Existing', 'temperature', 'greater_than',
                30, 1, 0, 'warning', 'resolved',
                '2026-07-26T10:00:01Z', '2026-07-26T10:00:01Z',
                '2026-07-26T10:00:02Z', 'condition_recovered', 31, 29, 31
            );
            """
        )
    monkeypatch.setattr(settings, "DATABASE_PATH", str(database))

    initialize_database()
    initialize_database()

    with sqlite3.connect(database) as conn:
        columns = {
            row[1] for row in conn.execute("PRAGMA table_info(alert_rules)")
        }
        assert "archived_at" in columns
        assert conn.execute(
            "SELECT name FROM alert_rules WHERE id = 1"
        ).fetchone() == ("Existing",)
        assert conn.execute(
            "SELECT resolution_reason FROM alert_events WHERE id = 1"
        ).fetchone() == ("condition_recovered",)
        indexes = {
            row[1] for row in conn.execute("PRAGMA index_list(alert_rules)")
        }
        assert "idx_alert_rules_active_name" in indexes
