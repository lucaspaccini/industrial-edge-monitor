# Device provisioning and credential lifecycle

Sprint 17 introduces one device-independent ESP32 image. Wi-Fi, MQTT identity, broker CA, telemetry cadence, machine-status input and maintenance policy are validated as one versioned configuration and stored in NVS. No device password or CA is compiled from `sdkconfig`.

The implementation follows the ESP-IDF 6.0.x primary documentation for [Wi-Fi/APSTA](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-guides/wifi.html), [the lightweight HTTP server](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/protocols/esp_http_server.html), [hardware random generation](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/system/random.html), [NVS](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/storage/nvs_flash.html) and [logging](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/system/log.html). ESP-IDF 6 removed its built-in JSON component, so the manifest and lockfile use Espressif's supported [`espressif/cjson` migration](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/migration-guides/release-6.x/6.0/protocols.html#json).

## Runtime states

```text
BOOT -> LOAD_CONFIGURATION
  |-- absent, corrupt or unknown schema -> UNPROVISIONED
  |     `-- WPA2 SoftAP + authenticated HTTP only; no station or MQTT attempt
  `-- valid -> MAINTENANCE_WINDOW or OPERATIONAL
        |-- APSTA + station + MQTT TLS + local HTTP
        `-- timeout -> stop HTTP -> WIFI_MODE_STA (never esp_wifi_stop)
```

The SoftAP uses `IEM-Setup-<last-six-MAC-hex>` and the default ESP-IDF AP network `192.168.4.1/24` with DHCP. On first boot, before ADC/I²C/Wi-Fi use, firmware calls `bootloader_random_enable()`, obtains 128 bits with `esp_fill_random()`, immediately calls `bootloader_random_disable()`, clears the temporary bytes, stores the secret in NVS and writes it once directly to the serial console before installing the diagnostic log sink. The value never enters ESP-IDF logs or the RAM diagnostic ring. Session and CSRF tokens are generated later, after RF is active. A factory reset erases the secret, so the next boot generates a different value.

The serial recovery task starts immediately after NVS initialization and before setup-secret access. An existing secret is accepted only when the NVS value is a string containing exactly 32 hexadecimal characters plus its terminator. A wrong NVS type, wrong length, invalid character, read failure or commit failure leaves the device in fail-closed serial recovery: firmware neither erases nor regenerates the key. Generation occurs only for `ESP_ERR_NVS_NOT_FOUND`, retains the documented hardware-entropy sequence, and clears random, generated, read and output buffers on failure.

On a configured device, `maintenance_on_boot` controls the initial APSTA window. `maintenance_window_seconds` is the inactivity deadline and `maintenance_max_session_seconds` is the absolute deadline. Only a successfully authenticated request refreshes inactivity. Closing the window stops the HTTP server and changes APSTA to STA without stopping Wi-Fi; MQTT observes station/IP events only.

If maintenance-on-boot is disabled, connect a local serial console and enter exactly:

```text
factory-reset ERASE-DEVICE-CONFIGURATION
```

This confirmed recovery command erases and reinitializes the complete NVS partition, then reboots into `UNPROVISIONED`. NVS initialization errors fail closed into this serial-only recovery state; firmware never automatically erases NVS after `NO_FREE_PAGES` or `NEW_VERSION_FOUND`. It does not provide a remote shell.

## Flash layout and migration

The verified module is an ESP32-D0WD-V3 revision 3.1 with 4 MiB flash at 3.3 V. The ignored pre-Sprint-17 `firmware/sdkconfig` was the exact source of the artificial limit: it contained `CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y`, `CONFIG_ESPTOOLPY_FLASHSIZE="2MB"`, `CONFIG_PARTITION_TABLE_SINGLE_APP=y` and `CONFIG_PARTITION_TABLE_FILENAME="partitions_singleapp.csv"`. That default table provided only a 1 MiB factory application. Sprint 17 replaces this local-only choice with the versioned `sdkconfig.defaults` and `partitions.csv` below; OTA is intentionally absent.

| Region | Type | Offset | Size | End |
| --- | --- | ---: | ---: | ---: |
| Bootloader | ESP-IDF | `0x001000` | max `0x007000` | `0x008000` |
| Partition table | ESP-IDF | `0x008000` | `0x001000` | `0x009000` |
| `nvs` | data/NVS | `0x009000` | `0x030000` (192 KiB) | `0x039000` |
| `phy_init` | data/PHY | `0x039000` | `0x001000` | `0x03A000` |
| Alignment gap | unallocated | `0x03A000` | `0x006000` | `0x040000` |
| `factory` | app/factory | `0x040000` | `0x300000` (3 MiB) | `0x340000` |
| Deliberate reserve | unallocated | `0x340000` | `0x0C0000` (768 KiB) | `0x400000` |

The 192 KiB NVS partition gives ample headroom for two approximately 4.8 KiB configuration blobs (each includes the public CA), setup secret, metadata and NVS page/wear overhead. The 3 MiB application region accommodates HTTP, embedded UI and diagnostics with growth room. The final 768 KiB is deliberately unallocated: it prevents an incidental feature from consuming the full physical flash and leaves a future, explicitly reviewed data-partition option without implying OTA support.

Changing the table moves both NVS and the application. The old NVS is **not preservable by this migration procedure**. Perform a controlled erase and complete flash:

```bash
source ~/esp/esp-idf/export.sh
idf.py -C firmware set-target esp32
rm -f firmware/sdkconfig.old
idf.py -C firmware build
idf.py -C firmware -p /dev/ttyUSB0 erase-flash
idf.py -C firmware -p /dev/ttyUSB0 flash monitor
```

`set-target` deliberately regenerates the ignored local `sdkconfig` from the versioned defaults. Delete the generated `firmware/sdkconfig.old` immediately because a historical configuration can contain credentials; it is neither a source of truth nor a supported backup. This does not and cannot preserve flash NVS. The equivalent explicit write produced by the verified build is:

```bash
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write-flash \
  --flash-mode dio --flash-size 4MB --flash-freq 40m \
  0x1000 firmware/build/bootloader/bootloader.bin \
  0x8000 firmware/build/partition_table/partition-table.bin \
  0x40000 firmware/build/industrial_edge_monitor_firmware.bin
```

The generated flash command must contain `--flash-size 4MB`, bootloader at `0x1000`, partition table at `0x8000` and application at `0x40000`. CI parses the versioned table, rejects overlap/misalignment/layout drift, checks the built binary against the 3 MiB application partition, and checks the 4 MiB flash arguments.

## NVS model and candidate activation

Namespace `iem_config` contains:

| Key | Purpose |
| --- | --- |
| `active` | Last configuration that completed Wi-Fi, SNTP and authenticated MQTT TLS checks |
| `candidate` | Fully validated pending configuration; never a partial update |
| `metadata` | State, candidate revision, boot attempts, boot count and last rollback reason |
| `setup_secret` | Unique SoftAP/web secret |

Storage format version 1 starts with a fixed `IEMC`/`IEMM` magic, little-endian 16-bit format/header versions and a 32-bit payload length. The payload uses explicit fixed-width integers, one-byte booleans/enums and fixed-size strings; it never persists a raw C struct, compiler padding or ABI-dependent `bool`/enum representation. Decode checks magic, format, header and payload lengths before converting to the runtime model and performing full semantic validation. Unknown versions and malformed/truncated blobs fail closed. Re-encoding is deterministic, and activation is retry-safe if power is lost between the candidate, active and metadata commits.

Runtime schema version 1 includes `device_id`, Wi-Fi credentials, `mqtts://host:port`, public CA PEM, MQTT username/password/client ID, telemetry interval, machine-status provider/GPIO/polarity/pull and maintenance policy. The web snapshot returns only `*_password_configured` and `mqtt_ca_certificate_configured` flags.

`Save candidate` validates and commits only the candidate. `Apply and reboot` is rejected unless metadata is pending. On boot, the candidate gets a bounded attempt, then must obtain station IP, valid SNTP time and an authenticated MQTT TLS connection. Candidate load marks a protected validation interval: stage, cancel and another apply return `409 Conflict`, and activation re-reads NVS under the same static FreeRTOS mutex and requires state, revision and every field to match the configuration actually verified by `app_main`. Active write, metadata transition and candidate deletion are serialized without recursively taking the mutex; interrupted commits remain retry-safe on the next boot. Only the matching candidate is copied to active. Failure records a redacted reason, deletes the candidate and reboots with the untouched active configuration. If no active configuration exists, the next boot returns to provisioning. The attempt ceiling prevents reboot loops.

NVS is fault-tolerant and wear-levelled, but Sprint 17 does not enable NVS encryption, flash encryption or Secure Boot. A physically capable attacker may extract stored credentials; production resistance to physical extraction remains future work.

## Local API

All handlers reject requests not accepted on the SoftAP local endpoint. ESP-IDF 6.0.2 enables IPv6 by default and its HTTP listener can accept an IPv4 request through an `AF_INET6` socket as an IPv4-mapped IPv6 local address. The gate therefore uses `sockaddr_storage`, validates the returned length/family, retrieves the live SoftAP IPv4 address and netmask through `esp_netif_get_ip_info()`, and accepts either a matching `AF_INET` address or matching IPv4-mapped `AF_INET6`. Native IPv6, station-side addresses, missing netif state, invalid descriptors, `getsockname()` errors, truncation and unexpected families fail closed. Rejections log only numeric socket family, redacted endpoint classification and policy reason. The client address is not used as the sole proof of ingress. After the maintenance window the server is stopped, so the API is not available on the station LAN.

| Method and path | Purpose |
| --- | --- |
| `POST /api/session` | Exchange setup secret for an expiring HttpOnly, SameSite=Strict session, CSRF token and decimal-string stream generation |
| `DELETE /api/session` | Clear the session; CSRF required |
| `GET /api/status` | Redacted firmware, network, time, MQTT, sensor, machine and health status |
| `GET /api/config` | Active configuration snapshot without passwords or CA contents |
| `PUT /api/config/candidate` | Validate and stage a complete candidate; CSRF required |
| `POST /api/config/apply` | Reboot only when a candidate is pending; CSRF required |
| `DELETE /api/config/candidate` | Cancel pending configuration; CSRF required |
| `GET /api/logs?after=<sequence>` | At most eight records plus cursor, loss and overwritten metadata |
| `GET /api/logs/stream?after=<sequence>` | Single authenticated asynchronous Server-Sent Events stream |
| `POST /api/reboot` | Controlled reboot; CSRF required |
| `POST /api/factory-reset` | Erase configuration with CSRF plus `X-Confirm-Factory-Reset: ERASE-DEVICE-CONFIGURATION` |

JSON bodies require `application/json` and are capped at 8192 bytes. Login failures trigger a bounded lockout. The single session expires, secrets are compared without early exit, and modifying operations require CSRF. Partially received request bodies are cleared on receive errors; setup-secret inputs and flat or nested candidate password strings are cleared before their owning buffers/JSON trees are released. The page clears masked password/file inputs when they no longer serve the current operation and never renders the CSRF token in status output. HTTP inside WPA2 is accepted for this local bench workflow but is not end-to-end application encryption or production-grade transport security. Transport, authentication and application code remain separable for later HTTPS or Protocomm work; there is no shared TLS private key in firmware.

Status reads station RSSI only while the station connection state makes the ESP-IDF query valid; disconnected and `UNPROVISIONED` states return the existing JSON `null` value without creating a health fault. When `/api/config` reports `unprovisioned`, or after session replacement, logout, factory reset or a relevant load/import failure, the page clears the configuration textarea, file selector and masked Wi-Fi/MQTT fields so prior configuration material cannot remain in the DOM.

## Diagnostics

The ESP-IDF log sink still writes serial output and duplicates bounded messages into a 64-record RAM ring. Each record has a monotonic sequence, relative timestamp, level, component and truncated message. A critical section copies at most eight records (compile-time bounded to 2 KiB); JSON construction and network I/O occur afterward. Cursor, first available sequence, overwritten count and records lost before the requested cursor are explicit. No per-log heap allocation or NVS write occurs.

The normal SSE handler authenticates the HttpOnly cookie and requires the decimal-string generation returned by that same login as a separate query parameter; neither the session nor CSRF token appears in the URL. Missing, malformed, expired or stale generations receive `204 No Content`, which prevents EventSource from continuing its automatic reconnect under a replacement session. The page closes its current EventSource before login, retains the generation only for that login and clears it on logout. One static FreeRTOS mutex serializes worker admission, generation, active state, socket ownership, shutdown and completion. A dedicated worker sends small batches, verifies session and worker generations every iteration, and calls `httpd_req_async_handler_complete()` exactly once before atomically recording completion of its own worker generation. Service stop first prevents new admission and then waits for that exact generation; there is no binary completion token that an old worker can leave for a newer one. Allocation and worker-creation failures follow the same exact-generation completion path. Send timeout is two seconds and each service-stop wait is capped at four seconds; after removing SoftAP reachability, the maintenance task continues retrying exceptional cleanup without a fixed retry ceiling. Status/configuration remain serviceable while a valid stream is live. Runtime logs expose stack high-water marks for app startup, maintenance and SSE workers.

## MQTT device administration

The [MQTT operations runbook](mqtt-operations.md) is the authoritative procedure for initial bundle generation, exact file ownership, `add`/`list`/`inspect`/`rotate`/`revoke`, safe rotation order, broker recreation, certificate inspection, recovery and smoke tests. The [MQTT security document](mqtt-security.md) remains the authoritative description of trust and authorization policy. Neither procedure changes `.env` or the firmware's MQTT topic and payload contracts.

## Operator-provided hardware verification record

The following main Sprint 17 checks were reported as passed by the operator on the real ESP32. They are manual evidence supplied to this repository review, not commands run by the automated suite or by this final hardening pass. No dates, serial output, credential values or additional physical results are inferred here.

- 4 MiB flash and the new partition table; first boot in `UNPROVISIONED`; one-time setup-secret generation.
- WPA2 SoftAP, DHCP and phone access; web authentication, CSRF, lockout and redaction of passwords, CA and setup secret.
- Redacted status, batched logs and live SSE.
- Wi-Fi/MQTT provisioning; separate Save candidate and Apply; activation only after Wi-Fi, SNTP and MQTT TLS.
- Active/candidate NVS persistence; controlled interruption during validation followed by success on the second attempt; rollback for invalid MQTT configuration.
- APSTA maintenance followed by STA-only operation, with MQTT and telemetry continuity after SoftAP shutdown.
- Machine-status GPIO behavior.
- MQTT credential rotation; identity revocation with rejection of the old password; identity restoration through a new provisioning package.
- Web factory reset; confirmed serial full-NVS recovery; regenerated setup secret and return to `UNPROVISIONED`; complete reprovisioning after resets.

Unreadable-NVS-metadata fail-closed behavior is covered by deterministic automated fault injection; it was **not** reported as a physical fault-injection test. The final RSSI disconnected-state guard and page stale-DOM cleanup are automatically covered and locally built after the hardware run; they do not add a claim of a further physical test.

## Explicit limits

No BLE provisioning, captive portal, OTA, permanent station-LAN web server, remote shell, mTLS, API/dashboard authentication, Internet configuration, Secure Boot, flash/NVS encryption, cloud secret store, fleet manager or second-device rollout is introduced. AP and station share one ESP32 radio; in APSTA the SoftAP follows the station channel, which can briefly disrupt a phone during channel changes.
