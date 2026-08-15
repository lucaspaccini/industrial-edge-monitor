# Firmware Development Setup

This document describes how to prepare the ESP-IDF development environment for the Industrial Edge Monitor firmware.

---

# Supported Platform

- Ubuntu 24.04 LTS
- ESP-IDF v6.0.2
- Target: ESP32
- Board: ESP32-WROOM-32U DevKitC
- Sensor: BME280

---

# Prerequisites

Install the required packages:

```bash
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

---

# ESP-IDF Installation

Clone the ESP-IDF repository and install the ESP32 toolchain.

```bash
mkdir -p ~/esp
cd ~/esp

git clone -b v6.0.2 --recursive https://github.com/espressif/esp-idf.git

cd ~/esp/esp-idf

./install.sh esp32
```

---

# Environment Setup

Before working on the firmware, load the ESP-IDF environment:

```bash
source ~/esp/esp-idf/export.sh
```

This command must be executed for every new terminal session used for firmware development.

Verify that the environment has been loaded:

```bash
echo $IDF_PATH
```

Expected output:

```text
/home/<user>/esp/esp-idf
```

Verify the installation:

```bash
idf.py --version
```

Expected output:

```text
ESP-IDF v6.0.2
```

---

# Serial Port Permissions

The user must belong to the `dialout` group in order to access the ESP32 serial port.

```bash
sudo usermod -aG dialout $USER
```

After executing the command, log out and log back in (or reboot the system).

Verify group membership:

```bash
groups
```

Verify the connected serial port:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

Example:

```text
/dev/ttyUSB0
```

---

# Project Configuration

From the firmware directory:

```bash
cd firmware
```

Configure the target once:

```bash
idf.py set-target esp32
rm -f sdkconfig.old
```

This command generates the project configuration (`sdkconfig`) from `sdkconfig.defaults`.
The generated `sdkconfig.old` is not a supported backup and may retain historical credentials; delete it immediately as shown above.

## Device-independent firmware and provisioning material

From the repository root, generate broker material with every LAN hostname/IP that the ESP32 may use, then create a device package:

```bash
scripts/generate-mqtt-security.sh --lan-host 192.168.1.20
scripts/manage-mqtt-device.py add edge-node-03 \
  --broker-uri mqtts://192.168.1.20:8883
```

Do not copy that package or any CA/password into the firmware source. Build the same binary for all devices. On a blank ESP32, read the newly generated setup secret from serial once, join `IEM-Setup-*`, open `http://192.168.4.1`, authenticate, import the MQTT values, enter Wi-Fi locally and stage the complete configuration.

`mqtt` is a Compose-only DNS name and `localhost` refers to the ESP32 itself. Use the Docker host LAN hostname/IP, include it in the broker certificate SAN and specify port `8883`. Hostname verification remains enabled. `.local/`, provisioning packages and local `sdkconfig` are ignored by Git and Docker contexts.

Do not bypass certificate verification or replace `mqtts://` with plaintext after an error. See [MQTT security](mqtt-security.md) for bundle generation, trust configuration, lifecycle limits and diagnostics.

---

# Build

Build the firmware:

```bash
idf.py build
```

The versioned `firmware/dependencies.lock` pins ESP-MQTT and the ESP-IDF 6 replacement `espressif/cjson`, and records ESP-IDF 6.0.2/ESP32 resolution. `managed_components/` remains generated and ignored. `sdkconfig.defaults` selects a 4 MiB flash header and the custom `partitions.csv`; the local `sdkconfig` is not the source of truth.

CI and local builds contain no device-specific trust anchor or credentials. CI validates the table and fails when the binary exceeds the 3 MiB application partition; it does not connect physical hardware.

Expected result:

```text
Project build complete.
```

---

# Flash

For an already migrated device, flash the firmware normally:

```bash
idf.py -p /dev/ttyUSB0 flash
```

Replace `/dev/ttyUSB0` with the correct serial port if necessary.

The first migration from the former 2 MiB/default layout is destructive and must not claim to preserve old NVS:

```bash
idf.py set-target esp32
rm -f sdkconfig.old
idf.py build
idf.py -p /dev/ttyUSB0 erase-flash
idf.py -p /dev/ttyUSB0 flash monitor
```

See [Device provisioning](device-provisioning.md) for the complete 4 MiB layout and physical checklist.

Expected result:

```text
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

---

# Serial Monitor

Open the serial monitor:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Expected output:

```text
Industrial Edge Monitor firmware started
```

Exit the monitor with:

```text
Ctrl + ]
```

---

# Project Workflow

Typical firmware development workflow:

```bash
cd firmware

source ~/esp/esp-idf/export.sh

idf.py build

idf.py -p /dev/ttyUSB0 flash

idf.py -p /dev/ttyUSB0 monitor
```

---

# Update ESP-IDF

When upgrading ESP-IDF, update:

- `firmware/toolchain.yml`
- `firmware/dependencies.lock` through the ESP-IDF component manager
- `firmware/README.md`
- `docs/firmware-setup.md`

before migrating the project.

---

# Troubleshooting

## `idf.py: command not found`

The ESP-IDF environment has not been loaded.

```bash
source ~/esp/esp-idf/export.sh
```

---

## Permission denied on `/dev/ttyUSB0`

Verify that the user belongs to the `dialout` group:

```bash
groups
```

---

## ESP32 not detected

Check the USB connection:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

Verify that the USB cable supports data transfer.

## MQTT configuration rejected

Confirm the candidate uses `mqtts://host:port`, username equals `device_id`, client ID is distinct, password is non-empty and the public CA parses as PEM. The firmware never falls back to plaintext and never attempts MQTT from `UNPROVISIONED`.

## SoftAP page incorrectly rejected

The first Sprint 17 phone test reproduced `provisioning is available only through the SoftAP` at `http://192.168.4.1`. ESP-IDF 6.0.2 can expose the accepted IPv4 connection as an IPv4-mapped IPv6 local endpoint when its dual-stack HTTP listener is active. The firmware now classifies a full `sockaddr_storage` against the live SoftAP netif IP instead of interpreting it as `sockaddr_in` or authorizing a hard-coded address. The correction is covered automatically, and the operator subsequently reported the corrected phone/SoftAP path as passed on hardware; the detailed operator-provided record is in [Device provisioning](device-provisioning.md#operator-provided-hardware-verification-record).

## TLS certificate or hostname failure

Verify that the active NVS configuration contains the public CA which signed the broker certificate and that the URI hostname/IP appears exactly in its SAN. Rotate and reprovision trust material instead of enabling an insecure mode.

# Firmware Configuration

The firmware uses the ESP-IDF Kconfig system for project configuration.

Open the configuration menu:

```bash
idf.py menuconfig
```

The project configuration is available under:

```text
Industrial Edge Monitor
├── Connectivity Configuration
└── Telemetry Configuration
```

Only non-secret hardware defaults, topic/health constants and provisioning limits are stored in Kconfig. Device-specific runtime configuration lives in the `iem_config` NVS namespace behind `device_config`; Wi-Fi, MQTT and providers never read NVS directly.
