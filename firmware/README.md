# Firmware

Firmware for the ESP32-based telemetry device of the **Industrial Edge Monitor** project.

## Purpose

The firmware is responsible for:

- Initializing the hardware platform.
- Managing Wi-Fi connectivity.
- Managing MQTT communication.
- Synchronizing the system clock through SNTP.
- Acquiring environmental telemetry from connected sensors.
- Publishing timestamped telemetry in JSON format only when the clock is valid.

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
│   ├── i2c_bus/           # Shared I²C bus management
│   ├── machine_status/    # Machine status provider
│   ├── mqtt_client_app/   # MQTT communication
│   ├── sensor/            # Hardware-independent sensor abstraction
│   ├── system_time/       # Timestamp generation
│   ├── telemetry/         # Telemetry acquisition and publishing
│   ├── utils/             # Shared utilities
│   └── wifi/              # Wi-Fi connectivity
│
├── main/
│   └── app_main.c
│
├── sdkconfig.defaults
├── idf_component.yml
└── README.md
```

Each component has a well-defined responsibility.

| Component | Responsibility |
|-----------|----------------|
| config | Project configuration |
| wifi | Wi-Fi connectivity |
| mqtt_client_app | MQTT communication |
| telemetry | Collects telemetry and publishes MQTT messages |
| sensor | Hardware-independent environmental sensor abstraction |
| bme280 | Bosch BME280 device driver |
| i2c_bus | Shared I²C bus management |
| machine_status | Machine state provider |
| system_time | SNTP lifecycle, clock validity and UTC ISO 8601 timestamp generation |
| utils | Shared helper functions |

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
      ├── clock invalid ──> publish skipped
      │
      ▼
MQTT Client
      │
      ▼
MQTT Broker
```

The firmware is responsible for the entire telemetry pipeline up to the MQTT broker. Data persistence, processing and visualization are handled by the backend services.

The `system_time` component starts SNTP after Wi-Fi obtains an IP address. It waits for a configurable initial interval, but a timeout is non-fatal: SNTP remains active in the background and telemetry stays paused. When synchronization eventually succeeds, timestamp generation and publishing recover automatically without rebooting the device.

Timestamps use UTC ISO 8601 format with second precision, for example `2026-07-20T14:35:42Z`.

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

Configuration files:

- `sdkconfig.defaults`
- `config/config.h`
- `config/secrets.h` *(local, ignored by Git)*

Project settings are available under `Industrial Edge Monitor` in `idf.py menuconfig`:

| Setting | Default | Purpose |
|---------|---------|---------|
| `SNTP_SERVER` | `pool.ntp.org` | Hostname of the time server |
| `SNTP_SYNC_TIMEOUT_MS` | `10000` | Maximum initial wait before boot continues |
| `TELEMETRY_PUBLISH_PERIOD_MS` | `5000` | Interval between telemetry attempts |

The SNTP timeout only limits the initial synchronous wait. It does not stop background synchronization and therefore cannot cause a boot loop.

---

# Coding Guidelines

- Keep each component focused on a single responsibility.
- Expose clean public APIs through component headers.
- Hide implementation details inside component source files.
- Avoid business logic inside hardware drivers.
- Keep hardware-independent logic separated from device-specific code.
- Prefer reusable components over monolithic application code.
