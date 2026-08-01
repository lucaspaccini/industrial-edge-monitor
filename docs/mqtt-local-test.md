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

With Docker Compose, the collector uses the private hostname `mqtt`, while host tools use `localhost:1883`. An ESP32 on the LAN must use the Docker host's LAN IP and exposed port `1883`; neither `mqtt` nor `localhost` identifies the broker from the device.

The supplied Mosquitto listener is anonymous and unencrypted by design for trusted local testing only. Do not expose it to an untrusted LAN or the Internet.

Availability messages contain `schema_version`, `device_id` and `status` (`online` or `offline`). The Last Will deliberately has no device timestamp. Health may contain `"timestamp": null` during SNTP failure; the collector adds `received_at`.

Publishing a payload whose `device_id` differs from the device segment in its topic is rejected and logged by the collector.
