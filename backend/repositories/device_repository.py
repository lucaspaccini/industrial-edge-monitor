import json

from backend.api.database import get_connection


def mark_seen(device_id: str, received_at: str, assume_online: bool = False) -> None:
    with get_connection() as conn:
        conn.execute(
            """
            INSERT INTO devices(device_id, reported_availability, last_seen)
            VALUES (?, ?, ?)
            ON CONFLICT(device_id) DO UPDATE SET last_seen = excluded.last_seen
            """,
            (device_id, "online" if assume_online else "offline", received_at),
        )


def set_availability(device_id: str, availability: str, received_at: str) -> None:
    with get_connection() as conn:
        conn.execute(
            """
            INSERT INTO devices(device_id, reported_availability, last_seen,
                                availability_updated_at)
            VALUES (?, ?, ?, ?)
            ON CONFLICT(device_id) DO UPDATE SET
                reported_availability = excluded.reported_availability,
                last_seen = CASE WHEN excluded.reported_availability = 'online'
                                 THEN excluded.last_seen ELSE devices.last_seen END,
                availability_updated_at = excluded.availability_updated_at
            """,
            (device_id, availability, received_at, received_at),
        )


def upsert_health(device_id: str, payload: dict, received_at: str) -> None:
    with get_connection() as conn:
        conn.execute(
            """
            INSERT INTO devices(device_id, reported_availability, last_seen)
            VALUES (?, 'online', ?)
            ON CONFLICT(device_id) DO UPDATE SET last_seen = excluded.last_seen
            """,
            (device_id, received_at),
        )
        conn.execute(
            """
            INSERT INTO device_health_current(
                device_id, payload, device_timestamp, status, availability,
                components, counters, metrics, received_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(device_id) DO UPDATE SET
                payload = excluded.payload,
                device_timestamp = excluded.device_timestamp,
                status = excluded.status,
                availability = excluded.availability,
                components = excluded.components,
                counters = excluded.counters,
                metrics = excluded.metrics,
                received_at = excluded.received_at
            """,
            (
                device_id,
                json.dumps(payload, separators=(",", ":")),
                payload.get("timestamp"),
                payload["status"],
                payload["availability"],
                json.dumps(payload["components"], separators=(",", ":")),
                json.dumps(payload["counters"], separators=(",", ":")),
                json.dumps(payload["metrics"], separators=(",", ":")),
                received_at,
            ),
        )


def fetch_devices() -> list[dict]:
    with get_connection() as conn:
        rows = conn.execute(
            "SELECT device_id, reported_availability, last_seen "
            "FROM devices ORDER BY device_id"
        ).fetchall()
        return [dict(row) for row in rows]


def fetch_health(device_id: str) -> dict | None:
    with get_connection() as conn:
        row = conn.execute(
            """
            SELECT h.payload, h.received_at, d.reported_availability, d.last_seen
            FROM device_health_current h
            JOIN devices d ON d.device_id = h.device_id
            WHERE h.device_id = ?
            """,
            (device_id,),
        ).fetchone()
        if row is None:
            return None
        result = dict(row)
        result["payload"] = json.loads(result["payload"])
        return result
