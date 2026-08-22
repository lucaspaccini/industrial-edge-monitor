# MQTT operations runbook

This runbook covers local broker trust material and per-device MQTT identity operations for the single-host Industrial Edge Monitor deployment. The security model and threat boundaries remain in [MQTT security](mqtt-security.md); this document is the operational procedure.

## 1. Scope and terminology

- The **local CA** is the private trust domain. Its public certificate validates the broker certificate; its private key signs certificates and is never supplied to a runtime service or device.
- The **Mosquitto server certificate** is public and identifies the broker. The matching **server private key** stays on the broker host and is mounted only into Mosquitto.
- **Hostname/SAN verification** checks that the hostname or IP in `mqtts://host:8883` is listed in the server certificate Subject Alternative Name. A valid CA signature alone is insufficient.
- An MQTT **username/password** authenticates one client to Mosquitto. It is unrelated to the TLS server private key.
- The MQTT **client ID** identifies a protocol session. It is distinct from the application `device_id` and grants no authorization by itself.
- The application **`device_id`** is the stable device identity in payloads, topics, SQLite and dashboard views. For devices, the MQTT username deliberately equals `device_id` so `%u` can scope ACLs.
- **Authentication** proves the connecting identity. **Authorization** decides which topics that identity may use.
- Mosquitto **Dynamic Security** loads generated clients, password hashes, roles and default-deny ACLs.
- A **provisioning package** is a mode-`0600` JSON transfer artifact containing the public CA and one device's MQTT URI, username, generated password and client ID. It contains no Wi-Fi credentials.
- **Device credentials** means that device's MQTT username/password, not the CA or broker key.
- Client certificates and **mTLS are not implemented**. The ESP32 authenticates the broker by CA plus hostname/SAN; Mosquitto authenticates the ESP32 by username/password.

## 2. Complete trust flow

```text
local generator
  -> local CA (private signing key + public certificate)
  -> signed Mosquitto server certificate
  -> Mosquitto mounts server certificate/private key
  -> Dynamic Security loads identities, password hashes and roles
  -> one-device provisioning package carries public CA + device credentials
  -> ESP32 verifies the broker chain and URI hostname/IP
  -> Mosquitto verifies the device username/password
  -> default-deny ACL permits only that username's device topics
```

The device never receives the CA private key, server private key, Dynamic Security file or credentials for another identity.

## 3. Generate the initial broker bundle

Prerequisites are Bash, Python 3, OpenSSL, Docker and access to `eclipse-mosquitto:2.1.2-alpine`. From the repository root:

```bash
scripts/generate-mqtt-security.sh --lan-host 192.168.1.3
```

Complete syntax:

```text
scripts/generate-mqtt-security.sh \
  [--output DIR] \
  [--lan-host HOST_OR_IP]... \
  [--device DEVICE_ID]... \
  [--force]
```

The default output is `.local/mqtt-security`. Each `--lan-host` adds an IP or DNS SAN to the server certificate. Every hostname or IP used by an ESP32 broker URI must be present in those SANs. Built-in SANs are `mqtt`, `localhost` and `127.0.0.1`.

The marker `.generated-version` contains bundle format version `1`. Without `--force`, a complete managed bundle is left unchanged; an incomplete bundle is rejected. An unmanaged directory, wrong/missing marker, symlink root, symlink marker, unsafe broad path or protected ancestor is never replaced.

Generation occurs under `umask 077` in a temporary directory beside the target. It generates a new CA, CA private key, server key/certificate and all initial passwords, hashes the password database, creates Dynamic Security JSON, verifies the certificate, normalizes permissions, then promotes the staged directory by rename. During `--force`, the old managed directory is first renamed to a backup and restored if promotion fails. A successful promotion removes that temporary backup.

> **Destructive trust-domain warning:** `--force` replaces the complete CA, server identity and every generated client credential. All provisioned devices will distrust or fail authentication until reprovisioned. It is not single-device rotation. Confirm the explicit output path, marker and recovery material before using it.

## 4. Exact file inventory

Initial generation creates this tree; lifecycle operations may add `devices/`, further client files and packages:

```text
.local/mqtt-security/
├── .generated-version
├── ca/
│   ├── ca.crt
│   └── ca.key
├── server/
│   ├── server.crt
│   └── server.key
├── mosquitto/
│   ├── passwords
│   └── dynamic-security.json
├── clients/
│   ├── collector.{password,container.conf,host.conf}
│   ├── healthcheck.{password,container.conf,host.conf}
│   ├── simulator.{password,container.conf,host.conf}
│   ├── legacy-test.{password,container.conf,host.conf}
│   └── edge-node-NN.{password,container.conf,host.conf}
└── devices/
    └── DEVICE_ID.json

.local/provisioning-packages/
└── DEVICE_ID.provisioning.json
```

The generator and lifecycle tool temporarily bind-mount their complete same-parent staging bundle at `/work` into a fixed Mosquitto utility image for password hashing and numeric ownership normalization. Consequently every staged file is visible to that short-lived local utility container even when it is not read by the command. The table's final column describes **runtime** service/device use, which is a narrower boundary.

| Path/category | Generator and reader | Classification and expected access | Runtime container/device use |
| --- | --- | --- | --- |
| `.generated-version` | generator; generator/lifecycle validators | non-secret, `0600` | not mounted or copied |
| `ca/ca.crt` | generator; Mosquitto, collector, clients, package writer | public, `0644`, read-only use | mounted read-only; may be copied to its device |
| `ca/ca.key` | generator/OpenSSL only | **secret**, `0600` | never runtime-mounted or copied to a device |
| `server/server.crt` | generator; Mosquitto/inspection tools | public, `0644` | mounted read-only into Mosquitto only |
| `server/server.key` | generator; Mosquitto | **secret**, host group-readable `0440`, container UID 1883 ownership | mounted read-only into Mosquitto only |
| `mosquitto/passwords` | generator/lifecycle; policy builder | **secret-equivalent hashed credentials**, `0600` | not runtime-mounted; never copied to a device |
| `mosquitto/dynamic-security.json` | policy builder/lifecycle; Dynamic Security plugin | sensitive hashes/policy, UID 1883 and host group, `0660` | writable bind mount because the plugin owns its state |
| `clients/*.password` | generator/lifecycle; corresponding service or operator | **secret**, normally `0600`; collector and demo `edge-node-02` are UID 10001/host-group `0440` | collector and opt-in simulator mount only their own password read-only |
| `clients/*.container.conf` | generator/lifecycle; container test/client | **secret** because it embeds `--pw`; normally `0600`; health check is UID 1883/host-group `0440` | health check mounted read-only; others only in controlled test mounts |
| `clients/*.host.conf` | initial generator; host tools | **secret**, `0600` | never mounted or copied |
| `devices/DEVICE_ID.json` | lifecycle tool; `inspect` | sensitive metadata without password, directory `0700`, file `0600` | not mounted or copied |
| provisioning package | lifecycle tool; operator and SoftAP page | **secret**, `0600` | transfer only to the matching device workflow |

Directories created by the initial generator are `0755`, except lifecycle `devices/` which is `0700`. Numeric container ownership may display as `nobody` on a host with user-namespace mapping; verify numeric UID/GID when diagnosing.

`.local/` and `*.provisioning.json` are ignored by Git and excluded from Docker build contexts. A provisioning package may supply only its public CA and one-device fields through the authenticated SoftAP page. No other bundle file may be copied to an ESP32.

## 5. Docker Compose and Mosquitto

`MQTT_SECURITY_DIR` selects the host bundle directory; the default is `./.local/mqtt-security`. Compose uses bind mounts, not a secret-management service:

| Host path under `MQTT_SECURITY_DIR` | Container path | Mode |
| --- | --- | --- |
| `ca/ca.crt` | `/run/mqtt-security/ca/ca.crt` | read-only |
| `server/server.crt` | `/run/mqtt-security/server/server.crt` | read-only |
| `server/server.key` | `/run/mqtt-security/server/server.key` | read-only |
| `mosquitto/dynamic-security.json` | `/run/mqtt-security/mosquitto/dynamic-security.json` | writable |
| `clients/healthcheck.container.conf` | `/run/mqtt-security/clients/healthcheck.container.conf` | read-only |

`docker/mosquitto/mosquitto.conf` loads the CA, server certificate/key and Dynamic Security plugin configuration at broker start. It exposes TLS-only port 8883, disables anonymous access and persists broker data in the named `mqtt-data` volume.

After `add`, `rotate` or `revoke`, recreate only the broker so it loads the newly promoted bind-mounted files:

```bash
docker compose up --detach --force-recreate mqtt
```

Recreation disconnects existing MQTT sessions. Retained messages and broker persistence remain in `mqtt-data`. Without recreation, a currently loaded old password/identity may remain accepted even though disk files changed.

The collector mounts only the public CA and `collector.password`, subscribes after API and broker health, and writes to the shared SQLite volume. The opt-in `demo` simulator mounts the CA and only `edge-node-02.password`; it is excluded from ordinary `docker compose up`. The health check publishes only to `industrial/healthcheck` using its constrained option file. API and frontend neither connect to MQTT nor receive device credentials.

## 6. Policy and roles

The versioned source is [`docker/mosquitto/security-policy.json`](../docker/mosquitto/security-policy.json). `build_mosquitto_security.py` combines it with supported `$7$` or Argon2id password hashes.

- **device** is the default role. Username must equal `device_id`; `%u` permits publish only to `industrial/devices/%u/telemetry`, `/health` and `/availability`, including the retained online state and Last Will topic. Devices cannot subscribe or publish for another username.
- **collector** may issue literal subscriptions and receive the legacy telemetry topic plus `industrial/devices/+/telemetry`, `/health` and `/availability`; it cannot publish.
- **simulator** and **legacy-test** receive `legacy-publisher`, which may publish only `industrial/telemetry`.
- **healthcheck** may publish only `industrial/healthcheck`.

Every unspecified publish, receive and subscribe action is denied. `subscribeLiteral` is intentional: it lets Dynamic Security reject unauthorized subscription requests rather than merely filtering delivered messages.

## 7. Device identity lifecycle

Common prerequisites: a valid managed version-1 bundle, no symlinks inside it, Docker access, the Mosquitto image and a valid 1–63-character ASCII device ID (`[A-Za-z0-9][A-Za-z0-9._-]{0,62}`) not reserved for a service.

General syntax:

```text
scripts/manage-mqtt-device.py [--bundle PATH] [--mosquitto-image IMAGE] COMMAND ...
```

### Add

```bash
scripts/manage-mqtt-device.py add edge-node-03 \
  --broker-uri mqtts://192.168.1.3:8883
```

`add` rejects an existing identity, validates the explicit-port `mqtts` URI, creates a CSPRNG `secrets.token_urlsafe(32)` password, hashes it interactively through `mosquitto_passwd`, assigns the default device role, writes device metadata/client files and atomically creates `.local/provisioning-packages/edge-node-03.provisioning.json` as `0600`. The password is absent from argv and output. Recreate Mosquitto before the new device attempts validation.

### List and inspect

```bash
scripts/manage-mqtt-device.py list
scripts/manage-mqtt-device.py inspect edge-node-03
```

`list` prints only non-service usernames. `inspect` prints device ID, client ID, broker URI and a credential-configured boolean. Neither displays a password, package, password hash, CA body or client option file.

### Rotate

```bash
scripts/manage-mqtt-device.py rotate edge-node-03 \
  --broker-uri mqtts://192.168.1.3:8883
```

`rotate` requires the identity to exist and transactionally replaces its password, metadata/client files, Dynamic Security data and provisioning package. Safe coordinated order:

1. Open and keep the authenticated ESP32 maintenance window available.
2. Run `rotate`; securely import the new package and **Save candidate** while the broker still accepts the active old credential.
3. Recreate only `mqtt` so the new credential becomes authoritative; a brief device disconnect is expected.
4. Select **Apply and reboot** so the already staged candidate validates with the new credential.
5. Confirm the device reconnects and the previous credential is rejected.

The tool does not support an overlap where old and new passwords are both valid. If candidate validation rolls back after broker recreation, the old active password cannot reconnect; retain SoftAP access and the new package, correct the candidate and retry. Do not rotate again blindly, because that would invalidate the package just staged.

### Revoke

```bash
scripts/manage-mqtt-device.py revoke edge-node-03
docker compose up --detach --force-recreate mqtt
```

`revoke` removes the identity from the hashed password source, rebuilds Dynamic Security and deletes that device's bundle `clients/` files and `devices/` metadata. It does **not** delete the external provisioning package or historical SQLite data. Revocation becomes effective when Mosquitto is recreated; that recreation also terminates an existing session. The dashboard retains the device and history; effective availability becomes offline according to retained state/last-seen rules.

To restore a revoked identity, use `add` again with the same `device_id`, recreate Mosquitto, import the new package and provision a candidate. Existing database history remains associated with that stable ID.

Every mutating command copies the bundle to same-parent staging, validates it, rebuilds Dynamic Security, normalizes runtime permissions and promotes with backup/rename rollback. Package promotion is part of the transaction for add/rotate.

## 8. Provisioning package handling

Default path:

```text
.local/provisioning-packages/DEVICE_ID.provisioning.json
```

Schema version 1 has exactly:

```text
schema_version
device_id
mqtt.broker_uri
mqtt.username
mqtt.password
mqtt.client_id
mqtt.ca_certificate
```

It includes the public CA and plaintext MQTT password, but no Wi-Fi SSID/password. The SoftAP page extracts the MQTT password into a masked field, removes it from the visible JSON, accepts Wi-Fi locally and submits the complete candidate; firmware persists validated fields in NVS. The page and firmware do not return passwords or CA contents.

Keep packages `0600`, transfer through a controlled local channel, minimize retention and delete only after a recovery policy permits it. Never place a package in Git, a ticket, chat, log, shell argument, container image or screenshot.

> **Destructive artifact warning:** deleting the last valid package can remove the operator's recovery copy of a device credential. Verify the exact file and recovery plan before deletion; no automatic package backup exists.

## 9. Certificate verification

Public metadata inspection is read-only:

```bash
openssl x509 -in .local/mqtt-security/server/server.crt \
  -noout -subject -issuer -dates -ext subjectAltName -fingerprint -sha256
openssl verify -CAfile .local/mqtt-security/ca/ca.crt \
  .local/mqtt-security/server/server.crt
```

Verify certificate/key correspondence without printing the private key:

```bash
cmp -s \
  <(openssl x509 -in .local/mqtt-security/server/server.crt -pubkey -noout | openssl pkey -pubin -outform DER) \
  <(openssl pkey -in .local/mqtt-security/server/server.key -pubout -outform DER) \
  && echo 'certificate and key match'
```

Verify the laboratory IP handshake and hostname/SAN check:

```bash
openssl s_client -connect 192.168.1.3:8883 \
  -verify_ip 192.168.1.3 \
  -CAfile .local/mqtt-security/ca/ca.crt \
  -verify_return_error -brief </dev/null
```

Negative checks must fail with nonzero status:

```bash
openssl s_client -connect 192.168.1.3:8883 \
  -verify_hostname wrong.example.invalid \
  -CAfile .local/mqtt-security/ca/ca.crt \
  -verify_return_error -brief </dev/null

openssl s_client -connect 192.168.1.3:8883 \
  -verify_ip 192.168.1.3 \
  -CAfile /explicit/path/to/untrusted-ca.crt \
  -verify_return_error -brief </dev/null
```

Replacing or renewing the CA/server certificate is not device-password rotation. Changing the CA requires distributing a new trust anchor to every device. The repository has no isolated server-certificate renewal command; the generator performs complete-bundle replacement, so controlled renewal remains an operational limitation.

## 10. Ownership, permissions and troubleshooting

Diagnose without reading contents:

```bash
test -f .local/mqtt-security/.generated-version
test ! -L .local/mqtt-security
find .local/mqtt-security -maxdepth 3 \
  -printf '%M %u:%g %p\n' | sort
```

The observed `Permission denied` during rotate occurs when the host user cannot read files copied into lifecycle staging, commonly because container normalization left UID 1883/10001 files with a GID the user does not belong to. Use the explicit, idempotent repair command:

```bash
scripts/manage-mqtt-device.py \
  --bundle .local/mqtt-security \
  normalize-permissions
```

The command rejects root or contained symlinks, a wrong version marker and incomplete bundles. It only normalizes the required owner/mode metadata; it does not rewrite certificates, private keys, hashes, policy, device metadata or provisioning packages. It succeeds when `edge-node-02` is present or intentionally revoked and is safe to run repeatedly. Never use `chmod -R 777`; inspect rather than manually filling missing files.

| Symptom | Check and action |
| --- | --- |
| Broker not healthy | `docker compose ps mqtt` and `docker compose logs mqtt`; check exact mounts, modes and marker |
| Hostname mismatch | Compare broker URI host/IP with certificate SAN; regenerate only through an approved trust-domain change |
| CA not trusted | Confirm the client uses this bundle's `ca.crt`; never disable verification |
| Old password still works | Recreate `mqtt`; editing disk files does not reload the running plugin state |
| Device offline after rotate | Keep maintenance open; confirm candidate saved before recreation, then apply and inspect redacted rollback status |
| Package missing | Do not extract a password from logs; rotate deliberately to create a new package, then coordinate cutover |
| Port 8883 occupied | `ss -ltnp 'sport = :8883'`; stop/reconfigure the exact conflicting process |
| Bundle incomplete/wrong version | Do not patch it manually; restore a controlled backup or explicitly review complete `--force` replacement |
| Retained state says online | Retained payload is historical broker state; compare current connection/last-seen/effective availability |

## 11. Backup and disaster recovery

To preserve the current trust domain and identities, a protected backup must include the entire `.local/mqtt-security` tree: CA private key/certificate, server key/certificate, hashed password source, Dynamic Security state, marker, device metadata and client material. Losing `ca.key` prevents signing within the existing trust domain; losing current identity state may force device reprovisioning.

SQLite backup is separate and protects telemetry/device/alert history, not broker trust. Provisioning packages are separate plaintext credential recovery artifacts and do not replace a bundle backup. Public certificates need integrity but not secrecy; private keys, client/password files, Dynamic Security data and packages require encrypted-at-rest backup, restricted access and audited controlled restore.

No automatic backup/restore is implemented. Test restoration to an isolated explicit path, validate marker/symlink/permissions, inspect public certificates and run security smoke before replacing an operational bundle.

## 12. Security smoke test

```bash
scripts/compose-security-smoke.sh
```

The script creates a unique temporary directory and Compose project, generates fresh credentials, starts MQTT/API/collector/frontend and verifies health. Positive paths cover authenticated TLS telemetry, retained health/availability, Last Will, legacy ingestion and SQLite persistence across API/collector recreation. Negative paths cover anonymous access, wrong password, untrusted CA, hostname mismatch, cross-device publish, device wildcard subscription and collector publish.

Its trap removes only its own container, Compose project, volumes, network and temporary material. It does not test the ESP32, radio behavior, physical NVS/power loss, Internet-facing hardening or every failure mode. A smoke test is a rapid critical-path integration check, not an exhaustive security suite.

The separate `scripts/mqtt-device-lifecycle-smoke.sh` checks add, rotate/old-password rejection/new-password acceptance and revoke/rejection against a temporary real broker.

## 13. Copyable operating procedures

### Initialize a new local broker

```bash
scripts/generate-mqtt-security.sh --lan-host 192.168.1.3
docker compose config --quiet
docker compose up --detach --wait --wait-timeout 180
```

### Add and provision a new ESP32

```bash
scripts/manage-mqtt-device.py add edge-node-03 \
  --broker-uri mqtts://192.168.1.3:8883
docker compose up --detach --force-recreate mqtt
```

Flash the common image only through the separate controlled hardware procedure. Join its WPA2 SoftAP, authenticate, import the matching package, enter Wi-Fi, Save candidate, then Apply and verify Wi-Fi/SNTP/MQTT TLS activation.

> **ESP32 reset warning:** web factory reset and confirmed serial full-NVS recovery erase active/candidate configuration and setup secret. Do not invoke them during ordinary credential operations.

### Inspect identities

```bash
scripts/manage-mqtt-device.py list
scripts/manage-mqtt-device.py inspect edge-node-03
```

### Rotate one device

```bash
scripts/manage-mqtt-device.py rotate edge-node-03 \
  --broker-uri mqtts://192.168.1.3:8883
# Import and Save candidate while the old broker credential is still loaded.
docker compose up --detach --force-recreate mqtt
# Apply the already saved candidate from the still-open maintenance page.
```

### Revoke or restore one device

```bash
scripts/manage-mqtt-device.py revoke edge-node-03
docker compose up --detach --force-recreate mqtt

scripts/manage-mqtt-device.py add edge-node-03 \
  --broker-uri mqtts://192.168.1.3:8883
docker compose up --detach --force-recreate mqtt
```

Import the new package and provision again after restore.

### Verify TLS and ACL

Run the read-only certificate/handshake commands in section 9, then:

```bash
scripts/compose-security-smoke.sh
scripts/mqtt-device-lifecycle-smoke.sh
```

### Stop and restart without deleting data

```bash
docker compose stop
docker compose start
docker compose ps
```

> **Volume deletion warning:** `docker compose down -v` deletes the named SQLite and Mosquitto volumes. It is not a normal stop/restart command. Use it only for an explicitly approved destructive reset after a verified backup.

> **Bundle deletion/replacement warning:** never recursively remove a generic path. Confirm the explicit managed bundle, marker and backup before complete `--force` replacement. Confirm an exact package path before deleting a provisioning package.
