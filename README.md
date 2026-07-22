# Industrial Edge Monitor

Industrial Edge Monitor is an end-to-end ESP32/IoT monitoring platform. An ESP32 acquires environmental data from a BME280, publishes validated telemetry and device diagnostics over MQTT, and a Python/Next.js stack persists and displays the data per device.

The project is developed both as a production-oriented embedded portfolio project and as the foundation of a deployable edge-monitoring product.

## Current capabilities

- ESP32 firmware built with ESP-IDF 6.0.2.
- BME280 temperature and humidity acquisition with physical-range validation.
- UTC ISO 8601 timestamps synchronized through recoverable background SNTP.
- Optional GPIO machine-status provider: `running`, `stopped`, or `unknown`.
- Separate telemetry, device-health and MQTT-availability flows.
- Stable per-device identity and MQTT topics.
- Retained health snapshots and online/offline availability with Last Will.
- Validated MQTT ingestion, SQLite persistence and in-place legacy migration.
- FastAPI endpoints filtered by device.
- Next.js dashboard with one selector controlling telemetry, history, statistics and health.

## Architecture

```text
ESP32 + BME280/GPIO
        │
        ▼
   MQTT broker
        │
        ▼
Python collector ──► SQLite ──► FastAPI ──► Next.js dashboard
```

The firmware keeps environmental telemetry, external machine status, internal device health and MQTT availability as separate domains. Infrastructure components expose results upward; application orchestrators translate them into diagnostics.

See [architecture](docs/architecture.md), [backend model](docs/backend.md) and the [firmware README](firmware/README.md) for details.

## MQTT topics

For device `edge-node-01`:

```text
industrial/devices/edge-node-01/telemetry
industrial/devices/edge-node-01/health
industrial/devices/edge-node-01/availability
```

The legacy `industrial/telemetry` topic remains supported and is assigned to `legacy-device`. The collector rejects per-device messages when the topic identity and payload `device_id` differ.

## Local quick start

Requirements: Python 3, Mosquitto, Node.js/npm and, for firmware development, ESP-IDF 6.0.2.

Create the backend environment:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
```

Start Mosquitto, then run these processes in separate terminals from the repository root:

```bash
source .venv/bin/activate
python -m backend.collector.subscriber
```

```bash
source .venv/bin/activate
uvicorn backend.api.main:app --reload
```

```bash
cd frontend
cp .env.example .env.local
npm install
npm run dev
```

Open the dashboard at `http://localhost:3000` and the API documentation at `http://127.0.0.1:8000/docs`.

Without a physical device, `python -m backend.simulator.publisher` publishes compatible legacy telemetry. Health and availability appear after the ESP32 publishes their retained messages.

## Firmware

```bash
source ~/esp/esp-idf/export.sh
cd firmware
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Configure Wi-Fi, MQTT broker, stable `device_id`, SNTP and the optional machine-status GPIO through `menuconfig`. Never connect a 24 V industrial signal directly to an ESP32 GPIO; use suitable conditioning and isolation.

## Verification

```bash
source .venv/bin/activate
pytest -q

cd frontend
npm run lint
npm run build
```

## Repository layout

```text
backend/    MQTT collector, persistence, services and FastAPI
firmware/   Modular ESP-IDF firmware
frontend/   Next.js monitoring dashboard
docs/       Architecture, setup, API, roadmap and sprint history
tests/      Backend tests
```

## Documentation

- [Setup](docs/setup.md)
- [REST API](docs/api.md)
- [MQTT topics and local test](docs/mqtt-local-test.md)
- [Firmware setup](docs/firmware-setup.md)
- [Roadmap](docs/roadmap.md)
- [Sprint history](docs/sprint-history.md)

Current milestone: **Sprint 12 — Device Health, Machine Status and Reliable Telemetry**. TLS, persistent device configuration, OTA, offline telemetry buffering and health-event history remain future work.
