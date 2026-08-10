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

All unspecified publish, receive and subscribe operations are denied. The versioned [`security-policy.json`](../docker/mosquitto/security-policy.json) is converted with a generated, hashed Mosquitto password file into the ignored Dynamic Security database. Dynamic Security is used because its `subscribeLiteral` ACLs reject unauthorized wildcard subscription requests at subscription time; a traditional static Mosquitto ACL only filters delivered messages. The shared `%u` role is scalable, but post-generation creation, revocation, rotation and synchronization of identities are deliberately deferred.

## Generate local material

Requirements are Docker, OpenSSL and Python 3. Generate the bundle before the first Compose start:

```bash
scripts/generate-mqtt-security.sh \
  --lan-host 192.168.1.20 \
  --lan-host edge-monitor.local
```

Use only the LAN names/addresses clients will actually use. The default bundle also creates `edge-node-01`, `edge-node-02`, collector, simulator, legacy-test and health-check identities. Additional identities may be included only during initial full-bundle generation with `--device DEVICE_ID`. Device IDs follow the application contract of 1–63 Unicode alphanumeric, `.`, `_` or `-` characters. Reserved service names and duplicates are rejected.

The script writes `.local/mqtt-security/` with restrictive permissions and prints neither passwords nor keys. It verifies the CA constraint, certificate chain and built-in SAN entries. A complete existing directory is left untouched and an unmanaged, invalidly marked or symlink directory is never replaced. `--force` is accepted only for a regular `.generated-version` marker with the expected value. Generation completes in the same parent before a controlled backup/rename promotion; an error preserves the prior bundle. This is a complete-bundle replacement, not normal credential rotation. `.local/` is excluded from Git and the Docker build context.

The fixed Mosquitto 2.1.2 image produces `$7$` SHA-512 PBKDF2 hashes for the generator's `mosquitto_passwd -U` operation and also supports Argon2id. The policy builder accepts only `$7$` and `$argon2id$` encoded values; plaintext and unknown hash formats fail validation.

Inspect public certificate metadata without exposing secrets:

```bash
openssl verify \
  -CAfile .local/mqtt-security/ca/ca.crt \
  .local/mqtt-security/server/server.crt
openssl x509 \
  -in .local/mqtt-security/server/server.crt \
  -noout -subject -issuer -dates -ext subjectAltName
```

## Start and verify

```bash
cp .env.example .env
docker compose config --quiet
docker compose up --build -d --wait --wait-timeout 180
docker compose ps
```

The broker mounts the server certificate/key and generated Dynamic Security database. The collector mounts only the CA and its own password file. The health check uses a dedicated constrained identity. API and frontend do not receive MQTT credentials.

Host tools can reuse ignored option files without putting a password on the command line:

```bash
mosquitto_pub \
  -o .local/mqtt-security/clients/edge-node-01.host.conf \
  -t industrial/devices/edge-node-01/telemetry \
  -m '{"device_id":"edge-node-01","timestamp":"2026-08-10T12:00:00Z","temperature":23.75,"humidity":45.5,"machine_status":"unknown"}'
```

A device subscription and a cross-device publish must be rejected. The repository smoke test verifies those negative cases, anonymous and bad-password rejection, untrusted CA and hostname mismatch, valid telemetry, retained health/availability, Last Will and persistence:

```bash
scripts/compose-security-smoke.sh
```

The smoke script creates a unique Compose project and temporary security directory, then removes only its own containers, network, volumes and secrets.

## ESP32 configuration

Generate a certificate containing the Docker host LAN hostname/IP, then copy only the public CA certificate into the ignored firmware location:

```bash
mkdir -p firmware/local_secrets
cp .local/mqtt-security/ca/ca.crt firmware/local_secrets/mqtt_ca.pem
idf.py -C firmware menuconfig
```

Under `Industrial Edge Monitor`, set:

- broker URI to `mqtts://<SAN-host-or-IP>:8883`;
- username to the same identity as `DEVICE_ID`, for example `edge-node-01`;
- that identity's generated password;
- broker CA path to `local_secrets/mqtt_ca.pem`.

The build embeds the public CA. Local `sdkconfig`, CA copy and credentials are ignored. An invalid/non-`mqtts` URI, placeholder, missing embedded CA or incomplete credentials makes MQTT initialization fail explicitly; there is no plaintext retry. ESP-MQTT keeps its existing reconnect, retained availability/health, Last Will and telemetry behavior after a valid secure initialization.

Build-time credentials in `sdkconfig` and the firmware binary are suitable only for the current controlled local workflow. Per-device provisioning, secure storage and rotation through NVS or a hardware-backed mechanism are future work.

## Manual ESP32 verification

Sprint 15 was verified successfully on a real ESP32. The device connected to Mosquitto with `mqtts` on port `8883`, negotiated TLS 1.2 with `ECDHE-RSA-AES256-GCM-SHA384`, validated the broker certificate against the embedded CA and matched the broker SAN. It authenticated with a dedicated device identity and published only to that device's authorized topics.

The BME280 telemetry path was then verified end to end through the collector, SQLite, API and dashboard. Health and retained online availability were observed, the retained offline Last Will was received after the broker detected an abrupt device loss through keepalive, and retained online availability returned after power restoration. Restarting the broker also confirmed that the running ESP32 reconnects automatically.

ESP-MQTT currently uses its default keepalive because the firmware does not override it. Consequently, an offline Last Will after sudden power loss is not immediate: detection time depends on the client keepalive and broker behavior. An explicit keepalive value should be introduced only when an availability detection requirement has been defined and measured, not selected arbitrarily.

This manual hardware result complements two separate automated controls: the firmware CI job compiles and inspects the embedded-CA branch, while the isolated security smoke test exercises broker TLS, authentication, authorization and application ingestion with container clients. GitHub Actions intentionally has no hardware-in-the-loop stage and does not perform the real ESP32 TLS handshake.

## Credential lifecycle is deferred

There is no operational command for adding, revoking or rotating one device after bundle generation. New initial identities can be selected with repeated `--device DEVICE_ID` arguments only while generating the complete bundle. `--force` replaces the entire marked bundle, including CA, server certificate and all client credentials; it is intentionally not presented as routine per-device rotation.

Persistent Device Configuration, Provisioning and Credential Lifecycle is a future sprint. It must define device synchronization, secure storage, revocation, rotation and recovery before an operational lifecycle can be claimed. Do not manually edit the generated Dynamic Security database or paste passwords into tickets, shell history, logs or source control.

## Troubleshooting

| Symptom/category | Check |
| --- | --- |
| TLS certificate verification | CA path, CA file readability, complete chain and whether the configured host/IP exists in SAN |
| TLS handshake | Client/server TLS support and minimum TLS 1.2; do not disable verification |
| DNS resolution | `mqtt` works only inside Compose; the ESP32 needs a LAN-resolvable host or IP |
| Authentication | Correct generated identity/password pair and matching complete bundle mounted by the broker |
| Authorization / Not authorized | Username-derived device scope, exact topic and dedicated service role |
| Broker unhealthy | Generated files and ownership, port `8883`, then `docker compose logs mqtt` |
| Collector exits at startup | `MQTT_CLIENT_ENABLED`, TLS/auth pairing and readable mounted CA/password files |
| Firmware reports invalid configuration | `mqtts://` URI, non-placeholder credentials and embedded CA generated before build |

Never work around these failures with insecure certificate modes, hostname bypass, anonymous access or plaintext fallback.

## Intentional limits

This sprint secures only MQTT. FastAPI and the dashboard still use unauthenticated HTTP with no reverse proxy, client passwords are locally generated files rather than an external secret store, and the ESP32 has build-time credentials rather than persistent secure provisioning. There is no per-device addition/rotation/revocation lifecycle, mTLS, automated credential rollout, hardened ingress, Internet exposure model or multi-host secret distribution. CI compiles the embedded-CA firmware branch but has no hardware-in-the-loop MQTT connection; the successful Sprint 15 ESP32 TLS test was performed manually. These limits must be resolved before a public deployment.
