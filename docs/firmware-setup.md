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
```

This command generates the project configuration (`sdkconfig`) from `sdkconfig.defaults`.

## Secure MQTT material

From the repository root, generate local broker material with every LAN hostname/IP that the ESP32 may use:

```bash
scripts/generate-mqtt-security.sh --lan-host 192.168.1.20
mkdir -p firmware/local_secrets
cp .local/mqtt-security/ca/ca.crt firmware/local_secrets/mqtt_ca.pem
```

Open `idf.py menuconfig` and configure `Industrial Edge Monitor > Connectivity Configuration`:

- `MQTT_BROKER_URI`: `mqtts://192.168.1.20:8883` (or another exact SAN value);
- `MQTT_BROKER_CA_CERT_PATH`: `local_secrets/mqtt_ca.pem`;
- `MQTT_USERNAME`: device identity, normally the same as `DEVICE_ID`;
- `MQTT_PASSWORD`: the corresponding generated password;
- `MQTT_CLIENT_ID`: a session identifier distinct from both values above.

`mqtt` is a Compose-only DNS name and `localhost` refers to the ESP32 itself. The LAN host/IP in the URI must be included in the server certificate SAN; hostname verification remains enabled. The public CA, local `sdkconfig` and credentials are ignored by Git. The firmware build embeds the CA and credentials, which is suitable only for the current local build-time workflow; secure persistent provisioning is future work.

Do not bypass certificate verification or replace `mqtts://` with plaintext after an error. See [MQTT security](mqtt-security.md) for bundle generation, trust configuration, lifecycle limits and diagnostics.

---

# Build

Build the firmware:

```bash
idf.py build
```

The versioned `firmware/dependencies.lock` pins the ESP-MQTT managed component and records ESP-IDF 6.0.2/ESP32 resolution for reproducible local and CI builds. `managed_components/` remains generated and ignored.

A normal local build with no CA file configured can compile the no-CA branch; `mqtt_init()` then fails closed at runtime instead of falling back to plaintext. The GitHub Actions firmware job follows a different path: it creates a one-run CA private key under `$RUNNER_TEMP`, copies only its public certificate to the ignored `firmware/local_secrets/mqtt_ca.pem`, and verifies both `MQTT_BROKER_CA_EMBEDDED=1` and the embedded-certificate target. This exercises `target_add_binary_data`, the linker symbols and the `broker.verification.certificate` branch at build time. CI does not connect a physical ESP32 to a broker and therefore does not replace the successful manual Sprint 15 TLS test on real hardware.

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

Confirm the URI uses `mqtts://`, the placeholder has been replaced, username/password are non-empty, and the configured CA file existed before the build. A local build without a configured CA may compile successfully, but `mqtt_init()` deliberately rejects that configuration at runtime. GitHub Actions does not use this no-CA path: it generates an ephemeral CA, copies the public certificate to `firmware/local_secrets/mqtt_ca.pem`, and verifies `MQTT_BROKER_CA_EMBEDDED=1` plus certificate incorporation. This remains a build check rather than an automated hardware TLS connection.

## TLS certificate or hostname failure

Verify that the firmware contains the CA which signed the active broker certificate and that the URI hostname/IP appears exactly in its SAN. Regenerate and redistribute the trust material instead of enabling an insecure mode.

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

Configuration values are stored in the `sdkconfig` file and exposed to the firmware through `sdkconfig.h`.
