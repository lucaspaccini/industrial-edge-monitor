# REST API

Base URL: `http://localhost:8000`. Interactive documentation: `http://localhost:8000/docs`.

| Endpoint | Purpose |
|---|---|
| `GET /telemetry/?device_id=edge-node-01&limit=100` | Device telemetry history |
| `GET /telemetry/latest?device_id=edge-node-01` | Latest device sample |
| `GET /telemetry/statistics?device_id=edge-node-01` | Device statistics |
| `GET /devices/` | Known devices and effective availability |
| `GET /devices/{device_id}/health` | Current validated health snapshot |
| `GET /alert-rules?device_id=...` | Rules and persisted runtime state |
| `POST /alert-rules` | Create a per-device rule |
| `GET /alert-rules/{rule_id}` | Read one rule |
| `PATCH /alert-rules/{rule_id}` | Modify, enable or disable a rule |
| `DELETE /alert-rules/{rule_id}` | Logically archive a rule |
| `GET /alerts/active?device_id=...` | Current active alerts |
| `GET /alert-events?device_id=...` | Ordered alert-event history |
| `GET /alert-events/{event_id}` | Read one historical event |
| `GET /health` | Backend process health |

For compatibility, omitting `device_id` on telemetry endpoints queries all stored devices. New clients should always select a device. A health response includes nullable device `timestamp`, mandatory backend `received_at`, reported and effective availability, `last_seen`, and extensible components/counters/metrics.

Alert history supports `device_id`, `status`, `severity`, `rule_id` and a controlled `limit` from 1 to 500. Active alerts support device, severity and rule filters. Rule creation requires an existing device. Duplicate names within the same device return `409`; missing resources return `404`; invalid rule values return `422`.

Rule conditions support `temperature` and `humidity`, `greater_than` and `less_than`, and `info`, `warning` and `critical` severities. `duration_seconds: 0` activates on the first violating sample.

`GET /alert-rules` excludes archived rules by default. `include_archived=true` exposes them for administrative inspection. DELETE never removes event history and returns `204`; deleting a missing or already archived rule returns `404`.
