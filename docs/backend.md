# Backend data model

## SQLite migration

Startup runs an idempotent migration before applying `schema.sql`:

1. inspect `PRAGMA table_info(telemetry)`;
2. add `device_id TEXT NOT NULL DEFAULT 'legacy-device'` only when absent;
3. assign `legacy-device` to null or empty identities;
4. create `idx_telemetry_device_timestamp` and `idx_devices_last_seen` with `IF NOT EXISTS`;
5. create `devices` and `device_health_current` without deleting or recreating existing data.

`telemetry` remains historical. `device_health_current` is an upserted snapshot, not a health event history. `devices` holds retained reported availability and `last_seen`.

The collector validates MQTT payloads with Pydantic before persistence. Per-device topics must carry the same `device_id`; mismatches are rejected and logged. The legacy `industrial/telemetry` topic is explicitly attributed to `legacy-device`.

Omitting `device_id` on legacy REST telemetry endpoints still queries across all devices. The dashboard always supplies it so every panel uses one identity.
