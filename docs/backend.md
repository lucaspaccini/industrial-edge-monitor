# Backend data model

## Runtime configuration

`backend.core.config.Settings` is the only backend configuration access point. It loads the root `.env` for traditional local development and normal process environment variables in containers and CI. `APP_ENV` accepts only `development`, `test` or `production`; connectivity defaults that point into the source tree or to `localhost` are rejected in production. Ports, limits, timeouts, MQTT topics/device identity and comma-separated HTTP(S) CORS origins are validated at startup.

Logging uses the validated `LOG_LEVEL`. API and collector receive the same runtime settings in Compose, except for their process command. The complete environment matrix is in [setup](setup.md).

The container runtime installs the exact production dependency closure in `requirements-runtime.txt`; `requirements.txt` additionally contains the pinned test toolchain used locally and in CI.

## SQLite migration

Startup runs an idempotent migration before applying `schema.sql`:

1. inspect `PRAGMA table_info(telemetry)`;
2. add `device_id TEXT NOT NULL DEFAULT 'legacy-device'` only when absent;
3. assign `legacy-device` to null or empty identities;
4. create `idx_telemetry_device_timestamp` and `idx_devices_last_seen` with `IF NOT EXISTS`;
5. create `devices` and `device_health_current` without deleting or recreating existing data.

`telemetry` remains historical. `device_health_current` is an upserted snapshot, not a health event history. `devices` holds retained reported availability and `last_seen`.

Relative database paths are resolved from the repository root and missing parent directories are created. File-backed connections enable foreign keys, WAL and `synchronous=NORMAL`; a configurable SQLite connect/busy timeout handles short writer contention between API and collector. In Compose, the API completes migration before collector startup. This remains a single-host SQLite design, not a multi-replica or network-filesystem solution.

## Alert persistence

The idempotent schema creates:

- `alert_rules`: immutable identity plus current per-device configuration;
- `alert_rule_states`: one persisted `normal`, `pending` or `active` runtime state per rule;
- `alert_events`: historical `active`/`resolved` events with a snapshot of the rule configuration.

A partial unique SQLite index permits at most one active event per rule. Indices cover device, rule, status, severity and event time. Existing telemetry, health and device rows are never rewritten or evaluated during migration.

Rule and event mutations are atomic within the alert-engine transaction. The same telemetry ID is ignored twice. A timestamp older than or equal to `last_evaluated_at` is logged and ignored, so out-of-order samples cannot activate or resolve events.

For `greater_than`, violation is `value > threshold` and recovery is `value <= threshold - hysteresis`. For `less_than`, violation is `value < threshold` and recovery is `value >= threshold + hysteresis`. Values inside the hysteresis band keep active events active.

Disabling a rule resets pending state and resolves an active event with `rule_disabled`. Changes to metric, operator, threshold, duration or hysteresis reset runtime state and resolve an active event with `rule_updated`. Archiving sets `archived_at`, disables the rule, resets pending state and resolves an active event with `rule_archived`. Archived rules are excluded from evaluation and default listings, while their history remains available and their name can be reused.

The collector is a long-running Python process without hot reload. It must be restarted after deploying backend code that changes telemetry processing. Structured transition logs include telemetry ID, device ID, rule ID, metric/value, previous/resulting state and outcome.

The collector validates MQTT payloads with Pydantic before persistence. Per-device topics must carry the same `device_id`; mismatches are rejected and logged. The legacy `industrial/telemetry` topic is explicitly attributed to `legacy-device`.

Omitting `device_id` on legacy REST telemetry endpoints still queries across all devices. The dashboard always supplies it so every panel uses one identity.
