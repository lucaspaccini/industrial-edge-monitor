# REST API

Base URL: `http://localhost:8000`. Interactive documentation: `http://localhost:8000/docs`.

| Endpoint | Purpose |
|---|---|
| `GET /telemetry/?device_id=edge-node-01&limit=100` | Device telemetry history |
| `GET /telemetry/latest?device_id=edge-node-01` | Latest device sample |
| `GET /telemetry/statistics?device_id=edge-node-01` | Device statistics |
| `GET /devices/` | Known devices and effective availability |
| `GET /devices/{device_id}/health` | Current validated health snapshot |
| `GET /health` | Backend process health |

For compatibility, omitting `device_id` on telemetry endpoints queries all stored devices. New clients should always select a device. A health response includes nullable device `timestamp`, mandatory backend `received_at`, reported and effective availability, `last_seen`, and extensible components/counters/metrics.
