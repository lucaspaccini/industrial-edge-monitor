# Firmware

Firmware for the ESP32-based telemetry device of the **Industrial Edge Monitor** project.

## Purpose

The firmware is responsible for:

- Initializing the hardware platform.
- Managing Wi-Fi connectivity.
- Managing authenticated MQTT over verified TLS.
- Synchronizing the system clock through SNTP.
- Acquiring environmental telemetry from connected sensors.
- Validating complete telemetry samples before serialization.
- Publishing timestamped telemetry only when the environmental sample and UTC timestamp are valid.
- Publishing internal health and MQTT availability independently from telemetry.

The firmware is **not** responsible for:

- Data persistence.
- Data visualization.
- Statistics computation.
- Business logic.

These responsibilities belong to the backend services.

---

## Design Principles

The firmware follows a modular component-based architecture.

Each component owns a single responsibility and exposes a minimal public API through its header files. Implementation details remain private within each component, reducing coupling and improving maintainability.

This architecture allows hardware drivers, communication services and application logic to evolve independently while keeping the firmware scalable and easy to extend.

---

## Project Structure

```text
firmware/
├── components/
│   ├── bme280/            # Environmental sensor driver
│   ├── config/            # Project configuration
│   ├── device_health/     # Thread-safe diagnostic state
│   ├── device_health_service/ # Health orchestration and publishing
│   ├── i2c_bus/           # Shared I²C bus management
│   ├── machine_status/    # Machine status provider
│   ├── mqtt_client_app/   # MQTT communication
│   ├── sensor/            # Hardware-independent sensor abstraction
│   ├── system_time/       # Timestamp generation
│   ├── telemetry/         # Telemetry acquisition and publishing
│   └── wifi/              # Wi-Fi connectivity
│
├── main/
│   ├── app_main.c
│   └── idf_component.yml  # Managed-component manifest
│
├── sdkconfig.defaults
├── dependencies.lock       # Reproducible managed-component resolution
└── README.md
```

Each component has a well-defined responsibility.

| Component | Responsibility |
|-----------|----------------|
| config | Project configuration |
| device_health | MQTT/JSON-independent diagnostic snapshot |
| device_health_service | Translates provider outcomes and publishes health heartbeats |
| wifi | Wi-Fi connectivity |
| mqtt_client_app | MQTT transport lifecycle and publish requests |
| telemetry | Orchestrates acquisition, serialization, publishing and diagnostics |
| sensor | Hardware-independent environmental sensor abstraction and measurement contract |
| bme280 | Bosch BME280 device driver |
| i2c_bus | Shared I²C bus management |
| machine_status | Machine state provider |
| system_time | SNTP lifecycle, clock validity and UTC ISO 8601 timestamp generation |

---

## Firmware Architecture

```text
                        +----------------------+
                        |       app_main       |
                        +----------+-----------+
                                   |
                  +----------------+----------------+
                  |                                 |
                  ▼                                 ▼
             Telemetry                        Wi-Fi Services
                  │                                 │
        +---------+---------+                       │
        │                   │                       │
        ▼                   ▼                       ▼
     Sensor           MQTT Client            Wi-Fi Connection
        │
        ▼
   BME280 Driver
        │
        ▼
     I²C Bus
```

---

## Telemetry Pipeline

```text
BME280 Driver
      │
      ▼
Sensor Abstraction
      │
      ▼
Telemetry + UTC Timestamp
      │
      ├── clock invalid ────────────> reject
      ├── acquisition error ────────> reject
      ├── invalid measurement ──────> reject
      ├── invalid timestamp ─────────> reject
      │
      ▼
JSON Serialization
      │
      ├── serialization error ──────> skip publish
      │
      ▼
MQTT Client
      │
      ├── disconnected/error ───────> skip/fail publish
      │
      ▼
MQTT Broker
```

The firmware is responsible for the entire telemetry pipeline up to the MQTT broker. Data persistence, processing and visualization are handled by the backend services.

The `system_time` component starts SNTP after Wi-Fi obtains an IP address. It waits for a configurable initial interval, but a timeout is non-fatal: SNTP remains active in the background and telemetry stays paused. When synchronization eventually succeeds, timestamp generation and publishing recover automatically without rebooting the device.

Timestamps use UTC ISO 8601 format with second precision, for example `2026-07-20T14:35:42Z`.

## Telemetry Validation and Recovery

`telemetry_model_create()` produces a sample only when all of these conditions hold:

- The BME280 acquisition succeeds.
- Temperature and humidity are finite numbers.
- Temperature is within `-40…85 °C` and humidity within `0…100 %RH`.
- A valid UTC timestamp can be generated.
- Machine status is `running`, `stopped`, or `unknown`; provider errors use `unknown` without discarding valid environmental telemetry.

The model builds a private candidate and copies it to the caller only after every check succeeds. Missing measurements are never replaced with zero and a missing timestamp is never replaced with a placeholder. Rejected samples are neither serialized nor published.

Sensor acquisition and MQTT publication are retried naturally on the next telemetry period. ESP-MQTT handles secure broker reconnection in the background, while the telemetry task remains alive and resumes successful publishing without restarting the device. TLS, authentication, DNS, transport and publication errors are logged as separate categories without credential values.

## Diagnostics

The shared diagnostic snapshot owns cumulative RAM counters:

- Samples acquired and validated successfully.
- Samples rejected before serialization.
- Publications accepted by the ESP-MQTT client.
- Publications failed or skipped anywhere in the pipeline.

They reset at boot and are sent in the periodic retained health heartbeat. `publish_failed` also includes a health publication request rejected by ESP-MQTT. Such a failure receives one non-blocking retry on the following health polling cycle; repeated failures wait for the next heartbeat or reconnection. Counters and metrics do not change `state_revision`, so they cannot trigger a health publication loop. An accepted publication means ESP-MQTT returned a valid message identifier; it does not represent an application-level acknowledgement from the broker or backend.

Health uses `healthy`/`degraded`; component health uses `healthy`, `degraded`, `fault`, or `unknown`; availability uses `online`/`offline`. These enums are separate from machine status. Health remains publishable while SNTP is unavailable by emitting `"timestamp": null` and `time_not_synchronized`.

Infrastructure components do not update diagnostics directly. The telemetry and health application services translate their return values and states into diagnostic updates.

---

## Component Responsibilities

The firmware is organized into logical layers.

```text
Application
    app_main

Application Services
    telemetry
    mqtt_client_app
    wifi
    machine_status
    system_time

Hardware Abstraction
    sensor

Device Drivers
    bme280
    i2c_bus
```

Each component exposes a clean public interface and depends only on the services it requires. This separation improves maintainability, testability and makes it easier to replace hardware implementations without affecting the upper layers.

---

# Development Environment

The firmware toolchain is documented in:

```text
firmware/toolchain.yml
```

`dependencies.lock` is versioned so clean local and CI builds resolve the same ESP-MQTT component version. Regenerate it only through the ESP-IDF component manager after intentionally changing `main/idf_component.yml` or the ESP-IDF target/version.

## Requirements

- Ubuntu 24.04 LTS
- ESP-IDF v6.0.2 (stable)
- ESP32-WROOM-32U DevKitC

---

# Build

From the firmware directory:

```bash
idf.py build
```

Expected result:

```text
Project build complete.
```

---

# Flash

Flash the firmware to the ESP32:

```bash
idf.py -p /dev/ttyUSB0 flash
```

Replace `/dev/ttyUSB0` with the correct serial port if necessary.

---

# Serial Monitor

Open the serial monitor:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Exit the monitor with:

```text
Ctrl + ]
```

Expected output:

```text
Industrial Edge Monitor firmware started
```

---

# Configuration

`sdkconfig.defaults` version-controls the 4 MiB flash header and custom no-OTA partition table. Kconfig now contains only non-secret hardware defaults, topic/health constants and provisioning safety limits. Device-specific values are stored through the local provisioning flow, not compiled into the image.

Runtime schema version 1 contains device ID, Wi-Fi credentials, MQTT TLS URI/public CA/credentials/client ID, telemetry cadence, machine-status settings and maintenance policy. NVS uses a separate stable binary format with magic, format version, header size, payload length, fixed-width fields and explicit conversion rather than raw runtime structs. The `device_config` component validates the complete model and owns NVS active/candidate/metadata/setup-secret keys. Other components receive the typed model and never read NVS directly; invalid NVS fails closed to confirmed serial recovery without automatic erase.

Relevant build-time settings under `Industrial Edge Monitor` are:

| Setting | Default | Purpose |
|---------|---------|---------|
| `SNTP_SERVER` | `pool.ntp.org` | Hostname of the time server |
| `SNTP_SYNC_TIMEOUT_MS` | `10000` | Maximum initial wait before boot continues |
| `MQTT_TOPIC_PREFIX` | `industrial/devices` | Prefix for per-device telemetry, health and availability |
| `DEVICE_HEALTH_PUBLISH_PERIOD_MS` | `60000` | Retained health heartbeat interval |
| `DEVICE_HEALTH_STATE_POLL_PERIOD_MS` | `1000` | Health state observation interval |
| `PROVISIONING_MAX_BODY_BYTES` | `8192` | Hard limit for local JSON requests |
| `PROVISIONING_SESSION_SECONDS` | `900` | Web session lifetime |
| `PROVISIONING_LOGIN_MAX_FAILURES` | `5` | Failures before temporary lockout |
| `PROVISIONING_CANDIDATE_MAX_ATTEMPTS` | `2` | Boot-attempt ceiling before rollback |

The SNTP timeout only limits the initial synchronous wait. It does not stop background synchronization and therefore cannot cause a boot loop.

## Provisioning and secure MQTT configuration

Build and flash the same image for every device. A blank device starts only its WPA2 SoftAP and authenticated local HTTP service; it never attempts MQTT with incomplete values. Create a mode-`0600` MQTT provisioning package with `scripts/manage-mqtt-device.py`, enter Wi-Fi credentials locally, stage a complete candidate and reboot. The candidate becomes active only after Wi-Fi, SNTP and authenticated MQTT TLS succeed. Passwords and CA contents are never returned by the web API.

The verified module has 4 MiB flash. `partitions.csv` allocates 192 KiB NVS and a 3 MiB factory application, leaving 768 KiB unallocated. Moving from the prior 2 MiB/default-table configuration requires `erase-flash` and a full flash; old NVS is not preserved. See [Device provisioning](../docs/device-provisioning.md) for the exact layout, migration, HTTP API, serial recovery and operator-provided hardware record, the [MQTT security model](../docs/mqtt-security.md) for broker policy, and [MQTT operations](../docs/mqtt-operations.md) for procedures.

## Machine status electrical safety

The default provider is disabled, which publishes `machine_status: "unknown"` and is treated as an unconfigured feature rather than a fault. When GPIO is selected, configure active level and pull mode independently to match the external circuit. There is no debounce in this sprint; use a stable logic signal.

Only apply ESP32-compatible 3.3 V logic to the configured GPIO. Never connect a 24 V industrial signal directly to the ESP32. Use suitable level conditioning and galvanic isolation designed for the installation.

For a bench test, enable the GPIO provider in `idf.py menuconfig`, confirm the selected pull configuration, and switch the pin between GND and 3.3 V. Active level yields `running`; the opposite level yields `stopped`. GPIO configuration/read errors yield `unknown` and degrade the machine-status component until recovery.

## MQTT payloads

The firmware publishes under `industrial/devices/{device_id}/...`. Telemetry includes `device_id`, UTC timestamp, temperature, humidity and machine status. Health is retained and contains schema version, nullable UTC timestamp, overall/component state, stable error codes, counters and metrics. Availability is retained; `online` is published after every connection and an offline Last Will contains no timestamp.

---

# Coding Guidelines

- Keep each component focused on a single responsibility.
- Expose clean public APIs through component headers.
- Hide implementation details inside component source files.
- Avoid business logic inside hardware drivers.
- Keep hardware-independent logic separated from device-specific code.
- Prefer reusable components over monolithic application code.
