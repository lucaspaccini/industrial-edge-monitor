# MQTT topics and local test

For device `edge-node-01`:

- `industrial/devices/edge-node-01/telemetry` — non-retained valid environmental samples;
- `industrial/devices/edge-node-01/health` — retained current health, on state changes, heartbeat and reconnect;
- `industrial/devices/edge-node-01/availability` — retained online state and retained offline Last Will.

The legacy collector subscription `industrial/telemetry` remains supported and assigns `legacy-device`.

Inspect all flows:

```bash
mosquitto_sub -h localhost -v -t 'industrial/devices/#' -t 'industrial/telemetry'
```

Availability messages contain `schema_version`, `device_id` and `status` (`online` or `offline`). The Last Will deliberately has no device timestamp. Health may contain `"timestamp": null` during SNTP failure; the collector adds `received_at`.

Publishing a payload whose `device_id` differs from the device segment in its topic is rejected and logged by the collector.
