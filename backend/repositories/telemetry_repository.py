from backend.api.database import get_connection


def fetch_telemetry_history(limit: int = 100, device_id: str | None = None):
    conn = None

    try:
        conn = get_connection()
        cursor = conn.cursor()

        query = """SELECT id, device_id, timestamp, temperature, humidity,
                          machine_status FROM telemetry"""
        parameters: tuple = ()
        if device_id is not None:
            query += " WHERE device_id = ?"
            parameters = (device_id,)
        query += " ORDER BY timestamp DESC LIMIT ?"
        cursor.execute(query, (*parameters, limit))

        rows = cursor.fetchall()
        return [dict(row) for row in rows]

    finally:
        if conn is not None:
            conn.close()

def fetch_latest_telemetry(device_id: str | None = None):
    conn = None

    try:
        conn = get_connection()
        cursor = conn.cursor()

        query = """SELECT id, device_id, timestamp, temperature, humidity,
                          machine_status FROM telemetry"""
        parameters: tuple = ()
        if device_id is not None:
            query += " WHERE device_id = ?"
            parameters = (device_id,)
        query += " ORDER BY id DESC LIMIT 1"
        cursor.execute(query, parameters)

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
            INSERT INTO telemetry
                (device_id, timestamp, temperature, humidity, machine_status)
            VALUES (?, ?, ?, ?, ?)
            """,
            (
                payload["device_id"], payload["timestamp"],
                payload["temperature"],
                payload["humidity"],
                payload["machine_status"],
            ),
        )

        conn.commit()
        return cursor.lastrowid

    finally:
        if conn is not None:
            conn.close()

def fetch_telemetry_statistics(device_id: str | None = None):
    conn = None

    try:
        conn = get_connection()
        cursor = conn.cursor()

        query = """
            SELECT
                COUNT(*) AS samples,
                MIN(temperature) AS min_temperature,
                MAX(temperature) AS max_temperature,
                AVG(temperature) AS avg_temperature,
                MIN(humidity) AS min_humidity,
                MAX(humidity) AS max_humidity,
                AVG(humidity) AS avg_humidity,
                MAX(timestamp) AS last_update
            FROM telemetry
            """
        parameters: tuple = ()
        if device_id is not None:
            query += " WHERE device_id = ?"
            parameters = (device_id,)
        cursor.execute(query, parameters)

        row = cursor.fetchone()
        return dict(row) if row is not None else None

    finally:
        if conn is not None:
            conn.close()
