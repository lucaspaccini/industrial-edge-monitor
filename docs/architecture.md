# Architecture

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

The four public domains are intentionally separate:

- telemetry: valid environmental samples plus `running`, `stopped` or `unknown` machine state;
- machine status: state of the external machine, never an ESP32 error indicator;
- device health: internal `healthy`/`degraded` state and per-component diagnostics;
- availability: MQTT reachability, derived from retained state and freshness.

SQLite stores telemetry history per device, a device registry, and only the current health snapshot. Extensible health sections remain validated JSON instead of becoming one database column per possible component or metric.

The collector always records server-side `received_at` for health. This is authoritative when device `timestamp` is null. `last_seen` advances on validated health or telemetry as well as online availability. Effective availability is online only when the retained reported state is `online` and `last_seen` is newer than the configured timeout; an offline Last Will is immediate.

After a valid telemetry row is committed, the collector passes its database ID and validated event timestamp to the backend alert engine. The engine has no MQTT, FastAPI or frontend dependency. It evaluates only enabled rules belonging to the sample device and commits each runtime transition together with its event mutation.

Telemetry persistence and alert evaluation are intentionally separate transactions: an alert failure is logged but cannot discard valid telemetry. Evaluation is idempotent through the persisted telemetry ID and event time, making a later retry safe without adding an outbox in this sprint.
