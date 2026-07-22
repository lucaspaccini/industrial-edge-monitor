from pathlib import Path

from backend.api.database import get_connection
from backend.core.config import settings


def initialize_database() -> None:
    conn = get_connection()

    try:
        schema_path = Path(__file__).parent / "schema.sql"

        columns = {
            row[1] for row in conn.execute("PRAGMA table_info(telemetry)").fetchall()
        }
        if columns and "device_id" not in columns:
            conn.execute(
                "ALTER TABLE telemetry ADD COLUMN device_id TEXT "
                "NOT NULL DEFAULT 'legacy-device'"
            )

        with open(schema_path, "r", encoding="utf-8") as schema_file:
            conn.executescript(schema_file.read())

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
