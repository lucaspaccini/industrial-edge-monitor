import sqlite3

from backend.api.database import get_connection


RULE_SELECT = """
SELECT r.*, s.state AS runtime_state, s.pending_since, s.last_evaluated_at,
       s.pending_extreme_value, s.last_value, s.last_telemetry_id
FROM alert_rules r
JOIN alert_rule_states s ON s.rule_id = r.id
"""


def device_exists(device_id: str) -> bool:
    with get_connection() as conn:
        return conn.execute(
            "SELECT 1 FROM devices WHERE device_id = ?",
            (device_id,),
        ).fetchone() is not None


def create_rule(payload: dict, timestamp: str) -> dict:
    with get_connection() as conn:
        cursor = conn.execute(
            """
            INSERT INTO alert_rules(
                name, device_id, metric, operator, threshold, duration_seconds,
                hysteresis, severity, enabled, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                payload["name"],
                payload["device_id"],
                payload["metric"],
                payload["operator"],
                payload["threshold"],
                payload["duration_seconds"],
                payload["hysteresis"],
                payload["severity"],
                int(payload["enabled"]),
                timestamp,
                timestamp,
            ),
        )
        rule_id = cursor.lastrowid
        conn.execute(
            "INSERT INTO alert_rule_states(rule_id, state) VALUES (?, 'normal')",
            (rule_id,),
        )
    return fetch_rule(rule_id)


def fetch_rule(rule_id: int) -> dict | None:
    with get_connection() as conn:
        row = conn.execute(
            RULE_SELECT + " WHERE r.id = ?",
            (rule_id,),
        ).fetchone()
        return _rule_row(row)


def fetch_rules(
    device_id: str | None = None,
    enabled: bool | None = None,
    include_archived: bool = False,
) -> list[dict]:
    query = RULE_SELECT + " WHERE 1 = 1"
    parameters: list = []
    if not include_archived:
        query += " AND r.archived_at IS NULL"
    if device_id is not None:
        query += " AND r.device_id = ?"
        parameters.append(device_id)
    if enabled is not None:
        query += " AND r.enabled = ?"
        parameters.append(int(enabled))
    query += " ORDER BY r.id"
    with get_connection() as conn:
        return [_rule_row(row) for row in conn.execute(query, parameters).fetchall()]


def update_rule_and_runtime(
    rule_id: int,
    payload: dict,
    timestamp: str,
    reset_runtime: bool,
    resolution_reason: str | None,
) -> dict:
    columns = [f"{key} = ?" for key in payload]
    values = [int(value) if key == "enabled" else value for key, value in payload.items()]
    columns.append("updated_at = ?")
    values.extend([timestamp, rule_id])

    with get_connection() as conn:
        conn.execute(
            f"UPDATE alert_rules SET {', '.join(columns)} WHERE id = ?",
            values,
        )
        if resolution_reason is not None:
            conn.execute(
                """
                UPDATE alert_events
                SET status = 'resolved', resolved_at = ?,
                    resolution_reason = ?
                WHERE rule_id = ? AND status = 'active'
                """,
                (timestamp, resolution_reason, rule_id),
            )
        if reset_runtime:
            conn.execute(
                """
                UPDATE alert_rule_states
                SET state = 'normal', pending_since = NULL,
                    pending_extreme_value = NULL,
                    last_evaluated_at = NULL, last_value = NULL,
                    last_telemetry_id = NULL
                WHERE rule_id = ?
                """,
                (rule_id,),
            )
    return fetch_rule(rule_id)


def archive_rule(rule_id: int, timestamp: str) -> None:
    with get_connection() as conn:
        cursor = conn.execute(
            """
            UPDATE alert_rules
            SET enabled = 0, archived_at = ?, updated_at = ?
            WHERE id = ? AND archived_at IS NULL
            """,
            (timestamp, timestamp, rule_id),
        )
        if cursor.rowcount == 0:
            return
        conn.execute(
            """
            UPDATE alert_events
            SET status = 'resolved', resolved_at = ?,
                resolution_reason = 'rule_archived'
            WHERE rule_id = ? AND status = 'active'
            """,
            (timestamp, rule_id),
        )
        conn.execute(
            """
            UPDATE alert_rule_states
            SET state = 'normal', pending_since = NULL,
                pending_extreme_value = NULL, last_evaluated_at = NULL,
                last_value = NULL, last_telemetry_id = NULL
            WHERE rule_id = ?
            """,
            (rule_id,),
        )


def fetch_events(
    *,
    device_id: str | None = None,
    status: str | None = None,
    severity: str | None = None,
    rule_id: int | None = None,
    limit: int = 100,
) -> list[dict]:
    query = "SELECT * FROM alert_events WHERE 1 = 1"
    parameters: list = []
    filters = {
        "device_id": device_id,
        "status": status,
        "severity": severity,
        "rule_id": rule_id,
    }
    for column, value in filters.items():
        if value is not None:
            query += f" AND {column} = ?"
            parameters.append(value)
    query += " ORDER BY activated_at DESC, id DESC LIMIT ?"
    parameters.append(limit)
    with get_connection() as conn:
        return [
            _event_row(row)
            for row in conn.execute(query, parameters).fetchall()
        ]


def fetch_event(event_id: int) -> dict | None:
    with get_connection() as conn:
        return _event_row(
            conn.execute(
                "SELECT * FROM alert_events WHERE id = ?",
                (event_id,),
            ).fetchone()
        )


def fetch_enabled_rules_for_device(
    conn: sqlite3.Connection,
    device_id: str,
) -> list[dict]:
    rows = conn.execute(
        RULE_SELECT + """
        WHERE r.device_id = ? AND r.enabled = 1 AND r.archived_at IS NULL
        ORDER BY r.id
        """,
        (device_id,),
    ).fetchall()
    return [_rule_row(row) for row in rows]


def update_runtime_state(
    conn: sqlite3.Connection,
    rule_id: int,
    *,
    state: str,
    pending_since: str | None,
    pending_extreme_value: float | None,
    evaluated_at: str,
    value: float,
    telemetry_id: int,
) -> None:
    conn.execute(
        """
        UPDATE alert_rule_states
        SET state = ?, pending_since = ?, pending_extreme_value = ?,
            last_evaluated_at = ?,
            last_value = ?, last_telemetry_id = ?
        WHERE rule_id = ?
        """,
        (
            state,
            pending_since,
            pending_extreme_value,
            evaluated_at,
            value,
            telemetry_id,
            rule_id,
        ),
    )


def create_event(
    conn: sqlite3.Connection,
    rule: dict,
    *,
    started_at: str,
    activated_at: str,
    value: float,
    extreme_value: float,
) -> None:
    conn.execute(
        """
        INSERT INTO alert_events(
            rule_id, device_id, rule_name, metric, operator, threshold,
            hysteresis, duration_seconds, severity, status, started_at,
            activated_at, activation_value, last_value, extreme_value)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'active', ?, ?, ?, ?, ?)
        """,
        (
            rule["id"],
            rule["device_id"],
            rule["name"],
            rule["metric"],
            rule["operator"],
            rule["threshold"],
            rule["hysteresis"],
            rule["duration_seconds"],
            rule["severity"],
            started_at,
            activated_at,
            value,
            value,
            extreme_value,
        ),
    )


def fetch_active_event(
    conn: sqlite3.Connection,
    rule_id: int,
) -> dict | None:
    return _event_row(
        conn.execute(
            "SELECT * FROM alert_events WHERE rule_id = ? AND status = 'active'",
            (rule_id,),
        ).fetchone()
    )


def update_active_event(
    conn: sqlite3.Connection,
    event_id: int,
    *,
    last_value: float,
    extreme_value: float,
) -> None:
    conn.execute(
        "UPDATE alert_events SET last_value = ?, extreme_value = ? WHERE id = ?",
        (last_value, extreme_value, event_id),
    )


def resolve_active_event(
    conn: sqlite3.Connection,
    event_id: int,
    *,
    resolved_at: str,
    last_value: float,
    reason: str,
) -> None:
    conn.execute(
        """
        UPDATE alert_events
        SET status = 'resolved', resolved_at = ?, resolution_reason = ?,
            last_value = ?
        WHERE id = ? AND status = 'active'
        """,
        (resolved_at, reason, last_value, event_id),
    )


def _rule_row(row: sqlite3.Row | None) -> dict | None:
    if row is None:
        return None
    result = dict(row)
    result["enabled"] = bool(result["enabled"])
    return result


def _event_row(row: sqlite3.Row | None) -> dict | None:
    return dict(row) if row is not None else None
