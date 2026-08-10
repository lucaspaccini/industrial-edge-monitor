# MQTT topics and local test

The standard broker accepts only authenticated MQTT over TLS on `8883`. Generate the ignored local security bundle and start Compose first:

```bash
scripts/generate-mqtt-security.sh --lan-host <docker-host-lan-hostname-or-ip>
docker compose up --build -d --wait --wait-timeout 180
```

For device `edge-node-01`:

- `industrial/devices/edge-node-01/telemetry` — non-retained valid environmental samples;
- `industrial/devices/edge-node-01/health` — retained current health, on state changes, heartbeat and reconnect;
- `industrial/devices/edge-node-01/availability` — retained online state and retained offline Last Will.

The legacy collector subscription `industrial/telemetry` remains supported and assigns `legacy-device`. It can be written only by the dedicated `simulator`/`legacy-test` role, not by a device or collector.

Publish a valid sample without exposing the generated password on the command line:

```bash
mosquitto_pub \
  -o .local/mqtt-security/clients/edge-node-01.host.conf \
  -t industrial/devices/edge-node-01/telemetry \
  -m '{"device_id":"edge-node-01","timestamp":"2026-08-10T12:00:00Z","temperature":23.75,"humidity":45.5,"machine_status":"unknown"}'

curl --fail 'http://localhost:8000/telemetry/latest?device_id=edge-node-01'
```

Normal device identities cannot subscribe. The collector has the only role allowed to use the exact required wildcard filters. To observe topics manually, use the collector option file carefully in a controlled development environment:

```bash
mosquitto_sub \
  -o .local/mqtt-security/clients/collector.host.conf \
  -v \
  -t 'industrial/devices/+/telemetry' \
  -t 'industrial/devices/+/health' \
  -t 'industrial/devices/+/availability' \
  -t 'industrial/telemetry'
```

With Compose, the collector uses `mqtt:8883`; host tools use `localhost:8883`. An ESP32 on the LAN uses the Docker host's SAN-covered LAN hostname/IP and port `8883`. Neither `mqtt` nor `localhost` identifies the broker from the ESP32.

Availability messages contain `schema_version`, `device_id` and `status` (`online` or `offline`). The Last Will deliberately has no device timestamp. Health may contain `"timestamp": null` during SNTP failure; the collector adds `received_at`. Publishing a payload whose `device_id` differs from the device segment in its topic is rejected by the collector even after broker authorization.

Use `scripts/compose-security-smoke.sh` for the complete isolated test matrix, including anonymous/bad-password rejection, CA and hostname verification, ACL denial, secure ingestion, retained state, Last Will and persistence. Certificate generation, initial identities, lifecycle limits and troubleshooting are documented in [MQTT security](mqtt-security.md).
