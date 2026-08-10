from pathlib import Path
from threading import Lock

from backend.api.database import get_connection
from backend.core.config import settings


_INITIALIZATION_LOCK = Lock()


def _rebuild_alert_tables_if_needed(conn) -> None:
    rules_sql_row = conn.execute(
        "SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'alert_rules'"
    ).fetchone()
    events_sql_row = conn.execute(
        "SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'alert_events'"
    ).fetchone()
    legacy_name_unique = False
    if rules_sql_row is not None:
        for index in conn.execute("PRAGMA index_list(alert_rules)").fetchall():
            if index[3] != "u":
                continue
            columns = [
                row[2]
                for row in conn.execute(
                    f'PRAGMA index_info("{index[1]}")'
                ).fetchall()
            ]
            if columns == ["device_id", "name"]:
                legacy_name_unique = True
                break
    rebuild_rules = rules_sql_row is not None and (
        "archived_at" not in rules_sql_row[0]
        or legacy_name_unique
    )
    rebuild_events = events_sql_row is not None and (
        "rule_archived" not in events_sql_row[0]
    )
    if not rebuild_rules and not rebuild_events:
        return

    conn.commit()
    conn.execute("PRAGMA foreign_keys = OFF")
    if rebuild_rules:
        conn.execute(
            """
            CREATE TABLE alert_rules_new
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
            )
            """
        )
        source_columns = {
            row[1] for row in conn.execute("PRAGMA table_info(alert_rules)")
        }
        archived_value = "archived_at" if "archived_at" in source_columns else "NULL"
        conn.execute(
            f"""
            INSERT INTO alert_rules_new(
                id, name, device_id, metric, operator, threshold,
                duration_seconds, hysteresis, severity, enabled,
                created_at, updated_at, archived_at)
            SELECT id, name, device_id, metric, operator, threshold,
                   duration_seconds, hysteresis, severity, enabled,
                   created_at, updated_at, {archived_value}
            FROM alert_rules
            """
        )
        conn.execute("DROP TABLE alert_rules")
        conn.execute("ALTER TABLE alert_rules_new RENAME TO alert_rules")

    if rebuild_events:
        conn.execute(
            """
            CREATE TABLE alert_events_new
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
            )
            """
        )
        conn.execute(
            """
            INSERT INTO alert_events_new
            SELECT * FROM alert_events
            """
        )
        conn.execute("DROP TABLE alert_events")
        conn.execute("ALTER TABLE alert_events_new RENAME TO alert_events")
    conn.commit()
    conn.execute("PRAGMA foreign_keys = ON")


def initialize_database() -> None:
    # SQLite can transiently lock the first journal-mode transition when two
    # threads initialize the same new file. Serialize only schema bootstrap;
    # normal repository access remains concurrent and process-safe through WAL.
    with _INITIALIZATION_LOCK:
        _initialize_database()


def _initialize_database() -> None:
    conn = get_connection()

    try:
        schema_path = Path(__file__).parent / "schema.sql"
        _rebuild_alert_tables_if_needed(conn)

        columns = {
            row[1] for row in conn.execute("PRAGMA table_info(telemetry)").fetchall()
        }
        if columns and "device_id" not in columns:
            conn.execute(
                "ALTER TABLE telemetry ADD COLUMN device_id TEXT "
                "NOT NULL DEFAULT 'legacy-device'"
            )

        with open(schema_path, "r", encoding="utf-8") as schema_file:
            schema = schema_file.read()
            conn.executescript(schema)

        health_columns = {
            row[1] for row in conn.execute(
                "PRAGMA table_info(device_health_current)"
            ).fetchall()
        }
        health_migration_columns = {
            "device_timestamp": "TEXT",
            "status": "TEXT",
            "availability": "TEXT",
            "components": "TEXT",
            "counters": "TEXT",
            "metrics": "TEXT",
        }
        for column, column_type in health_migration_columns.items():
            if column not in health_columns:
                conn.execute(
                    f"ALTER TABLE device_health_current ADD COLUMN {column} {column_type}"
                )

        alert_state_columns = {
            row[1] for row in conn.execute(
                "PRAGMA table_info(alert_rule_states)"
            ).fetchall()
        }
        if "pending_extreme_value" not in alert_state_columns:
            conn.execute(
                "ALTER TABLE alert_rule_states "
                "ADD COLUMN pending_extreme_value REAL"
            )

        conn.execute(
            "UPDATE telemetry SET device_id = ? "
            "WHERE device_id IS NULL OR trim(device_id) = ''",
            (settings.LEGACY_DEVICE_ID,),
        )
        conn.execute(
            """
            INSERT INTO devices(device_id, reported_availability, last_seen)
            SELECT device_id, 'offline', MAX(timestamp)
            FROM telemetry
            WHERE device_id IS NOT NULL AND trim(device_id) != ''
            GROUP BY device_id
            ON CONFLICT(device_id) DO NOTHING
            """
        )

        conn.commit()

    finally:
        conn.close()
