from pathlib import Path

from backend.api.database import get_connection


def initialize_database() -> None:
    conn = get_connection()

    try:
        schema_path = Path(__file__).parent / "schema.sql"

        with open(schema_path, "r", encoding="utf-8") as schema_file:
            conn.executescript(schema_file.read())

        conn.commit()

    finally:
        conn.close()