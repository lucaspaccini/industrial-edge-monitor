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

---

# Build

Build the firmware:

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