# Architecture

## Product data flow

```text
ESP32 providers -> telemetry/device-health orchestrators -> MQTT transport
       |                         |                         |
       +-- BME280/GPIO/SNTP      +-- validation/state     +-- per-device topics
                                                               |
                                                        MQTT collector
                                                               |
                          SQLite telemetry + health + devices + alert state/events
                                                               |
                                                 Alert engine + FastAPI services
                                                               |
                                                selected-device dashboard
```

The firmware layers expose results upward. `sensor`, `machine_status`, `system_time` and `mqtt_client_app` do not depend on diagnostics. The telemetry and device-health services translate provider/transport outcomes into one mutex-protected `device_health` snapshot.

The four public domains remain separate:

- telemetry: valid environmental samples plus `running`, `stopped` or `unknown` machine state;
- machine status: state of the external machine, never an ESP32 error indicator;
- device health: internal `healthy`/`degraded` state and per-component diagnostics;
- availability: MQTT reachability, derived from retained state and freshness.

## Container topology

This section describes component relationships. The detailed container lifecycle, image construction, networking and persistence model are documented in [Docker and Compose](docker.md); automated repository verification is documented in [Continuous Integration](ci.md).

```text
                          Compose bridge: edge

 ESP32/LAN ── host:1883 ──► mqtt ──► collector
                                        │
                                        ▼
                              /data/telemetry.db
                                 (sqlite-data)
                                        ▲
                                        │
 browser ── host:8000 ──────────────── api

 browser ── host:3000 ──────────────── frontend
```

API and collector use the same non-root Python image and the same named SQLite volume. Their commands are different entry points, while domain and persistence code remains shared. The frontend image contains only the Next.js standalone server and traced runtime assets. Mosquitto has an explicit trusted local/LAN single-host configuration and a separate volume for retained broker data.

API starts and completes the idempotent schema migration before becoming healthy. Collector startup depends on that API health state and on a healthy broker, preventing both processes from racing the first schema migration. Normal runtime access still involves two SQLite processes, so connections enable foreign keys, WAL mode, `synchronous=NORMAL` and a configurable busy timeout.

Only ports required by external clients are published: MQTT for the ESP32, FastAPI for browser calls and diagnostics, and Next.js for the dashboard. On the private bridge, services use `mqtt` and `api` DNS names. A browser or ESP32 must use an address of the Docker host instead.

## Persistence and consistency

SQLite stores telemetry history per device, a device registry, current health snapshots, alert rules, runtime states and event history. Extensible health sections remain validated JSON instead of becoming one database column per possible component or metric.

The collector records server-side `received_at` for health. It is authoritative when device `timestamp` is null. `last_seen` advances on validated health or telemetry as well as online availability. Effective availability is online only when the retained reported state is `online` and `last_seen` is newer than the configured timeout; an offline Last Will is immediate.

After a valid telemetry row is committed, the collector passes its database ID and validated event timestamp to the alert engine. The engine has no MQTT, FastAPI or frontend dependency. It evaluates only enabled rules belonging to the sample device and commits each runtime transition together with its event mutation.

Telemetry persistence and alert evaluation are intentionally separate transactions: an alert failure is logged but cannot discard valid telemetry. Evaluation is idempotent through the persisted telemetry ID and event time, making a later retry safe without adding an outbox.

The deployment is intentionally single-host with one API process and one collector process on a local Docker volume. WAL improves reader/writer coexistence; it does not make a SQLite file safe for multiple replicas or arbitrary network filesystems. PostgreSQL, replication and orchestration are separate architectural decisions.
