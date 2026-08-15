# MQTT security

The standard Compose path uses authenticated MQTT over TLS on port `8883`. There is no automatic downgrade to plaintext.

## Security layers

TLS encrypts traffic, protects it against modification in transit and lets clients verify the broker certificate against the local CA. Authentication then maps each connection to a Mosquitto username. Authorization applies least-privilege topic rules to that authenticated username. These are separate controls: a valid TLS connection does not by itself authorize a publish.

The local CA is a trust anchor, not a client credential. It signs a server certificate whose Subject Alternative Name (SAN) contains `mqtt`, `localhost`, `127.0.0.1` and every explicitly requested LAN hostname/IP. Paho and ESP-MQTT validate both the chain and the URI hostname/IP. Client certificates are intentionally not used in this sprint; clients authenticate with unique passwords inside the verified TLS channel.

## Identities and authorization

`device_id`, MQTT client ID and MQTT username remain distinct concepts. A device username currently equals its stable `device_id` so the broker can expand `%u` in its rules. MQTT client IDs identify concurrent protocol sessions and do not grant topic access. The collector still validates that a per-device topic and payload carry the same `device_id`, providing an application-layer check in addition to broker authorization.

| Username/role | Allowed operation |
| --- | --- |
| `edge-node-01` and other device identities | Publish only `industrial/devices/%u/telemetry`, `/health` and `/availability`; no subscriptions |
| `collector` | Subscribe to and receive the legacy telemetry topic and the three required per-device topic filters; no publish |
| `simulator`, `legacy-test` | Publish only `industrial/telemetry` |
| `healthcheck` | Publish only `industrial/healthcheck` |

All unspecified publish, receive and subscribe operations are denied. The versioned [`security-policy.json`](../docker/mosquitto/security-policy.json) is converted with a generated, hashed Mosquitto password file into the ignored Dynamic Security database. Dynamic Security is used because its `subscribeLiteral` ACLs reject unauthorized wildcard subscription requests at subscription time; a traditional static Mosquitto ACL only filters delivered messages. The shared `%u` role is also used by the transactional per-device lifecycle command.

## Operational procedures

Bundle generation, exact file inventory and permissions, Compose mounts, certificate inspection, ESP32 package handling, transactional identity lifecycle, troubleshooting, recovery and positive/negative smoke tests are documented in the dedicated [MQTT operations runbook](mqtt-operations.md). Keeping procedures there prevents operational commands from obscuring this security model. The common firmware embeds no device-specific CA or credentials; its complete candidate is activated only after station, SNTP and authenticated TLS succeed. See also [Device provisioning](device-provisioning.md).

## Manual ESP32 verification

Sprint 15 was verified successfully on a real ESP32. The device connected to Mosquitto with `mqtts` on port `8883`, negotiated TLS 1.2 with `ECDHE-RSA-AES256-GCM-SHA384`, validated the broker certificate against the embedded CA and matched the broker SAN. It authenticated with a dedicated device identity and published only to that device's authorized topics.

The BME280 telemetry path was then verified end to end through the collector, SQLite, API and dashboard. Health and retained online availability were observed, the retained offline Last Will was received after the broker detected an abrupt device loss through keepalive, and retained online availability returned after power restoration. Restarting the broker also confirmed that the running ESP32 reconnects automatically.

ESP-MQTT currently uses its default keepalive because the firmware does not override it. Consequently, an offline Last Will after sudden power loss is not immediate: detection time depends on the client keepalive and broker behavior. An explicit keepalive value should be introduced only when an availability detection requirement has been defined and measured, not selected arbitrarily.

This Sprint 15 result is historical evidence for the TLS transport and contracts. The operator-provided Sprint 17 record separately covers the new provisioning, credential lifecycle and recovery flow on hardware. CI builds the credential-free image and exercises broker TLS/ACL with container clients; it has no hardware-in-the-loop stage.

## Intentional limits

FastAPI and the dashboard still use unauthenticated HTTP with no hardened reverse proxy. Client passwords remain locally generated files rather than a managed fleet secret store; device NVS lacks encryption, flash encryption and Secure Boot. There is no mTLS, automated remote rollout, Internet exposure model or multi-host secret distribution. The local provisioning page is HTTP protected by a unique WPA2 SoftAP, not application-layer HTTPS. CI has no hardware-in-the-loop MQTT connection. These limits must be resolved before a public deployment.
