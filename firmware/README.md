# Firmware

Firmware for the ESP32-based telemetry device of the Industrial Edge Monitor project.

## Purpose

The firmware is responsible for:

- Connecting the ESP32 to the Wi-Fi network.
- Connecting to the MQTT broker.
- Reading telemetry from the connected sensors.
- Publishing telemetry messages in JSON format.

The firmware is **not** responsible for:

- Data persistence.
- Data visualization.
- Statistics computation.
- Business logic.

These responsibilities belong to the backend services.

---

# Project Structure

```text
firmware/
├── main/
│   ├── app_main.c
│   ├── config/
│   ├── wifi/
│   ├── mqtt/
│   ├── sensors/
│   ├── telemetry/
│   └── utils/
├── sdkconfig.defaults
└── README.md
```

Each module has a single responsibility.

| Module | Responsibility |
|----------|---------------|
| config | Project configuration |
| wifi | Wi-Fi connection |
| mqtt | MQTT communication |
| sensors | Sensor drivers |
| telemetry | Telemetry generation |
| utils | Shared utilities |

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

---

# Coding Guidelines

- Keep modules focused on a single responsibility.
- Avoid business logic inside hardware drivers.
- Keep hardware-independent logic separated whenever possible.
- Prefer reusable modules over monolithic `app_main.c`.