from backend.api.database import get_connection


def fetch_telemetry_history(limit: int = 100):
    conn = None

    try:
        conn = get_connection()
        cursor = conn.cursor()

        cursor.execute(
            """
            SELECT id, timestamp, temperature, humidity, machine_status
            FROM telemetry
            ORDER BY timestamp DESC
            LIMIT ?
            """,
            (limit,),
        )

        rows = cursor.fetchall()
        return [dict(row) for row in rows]

    finally:
        if conn is not None:
            conn.close()

def fetch_latest_telemetry():
    conn = None

    try:
        conn = get_connection()
        cursor = conn.cursor()

        cursor.execute(
            """
            SELECT id, timestamp, temperature, humidity, machine_status
            FROM telemetry
            ORDER BY timestamp DESC
            LIMIT 1
            """
        )

        row = cursor.fetchone()
        return dict(row) if row is not None else None

    finally:
        if conn is not None:
            conn.close()

def insert_telemetry(payload: dict):
    conn = None

    try:
        conn = get_connection()
        cursor = conn.cursor()

        cursor.execute(
            """
            INSERT INTO telemetry (timestamp, temperature, humidity, machine_status)
            VALUES (?, ?, ?, ?)
            """,
            (
                payload["timestamp"],
                payload["temperature"],
                payload["humidity"],
                payload["machine_status"],
            ),
        )

        conn.commit()

    finally:
        if conn is not None:
            conn.close()
