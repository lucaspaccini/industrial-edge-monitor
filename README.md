# Industrial Edge Monitor

Industrial Edge Monitor is an end-to-end ESP32/IoT monitoring platform. An ESP32 reads a BME280, publishes validated telemetry and diagnostics over MQTT, and a Python/Next.js stack stores, evaluates and displays data per device.

The project is both a production-oriented Embedded/IoT/Edge portfolio and the evolving foundation of a single-host edge-monitoring product.

## Current capabilities

- Modular ESP-IDF 6.0.2 firmware with BME280 acquisition, SNTP UTC time and authenticated MQTT over TLS.
- Separate per-device telemetry, health and availability topics, with explicit legacy-topic compatibility.
- Strict collector validation, idempotent SQLite migrations and device-scoped FastAPI endpoints.
- Persistent threshold alerts with dwell time, hysteresis and active/resolved event history.
- Next.js 16 dashboard whose device selector scopes telemetry, statistics, health and alerts together.
- Reproducible four-service Docker Compose stack and separate CI jobs for backend, frontend, firmware and containers.

## Architecture

```text
ESP32 ──MQTTS──► Mosquitto ──MQTTS──► collector ──► SQLite volume ◄── FastAPI
                    ▲                                      │
                    │                                      ▼
                  LAN:8883                    browser ◄── Next.js
```

The API and collector reuse one production-like Python image. Mosquitto, both Python processes and the Next.js standalone server share a private Compose network. Only authenticated MQTT TLS `8883`, API `8000` and dashboard `3000` are published to the host.

See [architecture](docs/architecture.md), the [Docker and Compose guide](docs/docker.md), the [CI guide](docs/ci.md), the [backend model](docs/backend.md) and the [firmware README](firmware/README.md) for component details.

## Docker Compose quick start

Requirements: Docker Engine with the modern Compose and Buildx plugins, plus outbound access for the initial image/dependency build.

Generate local development credentials before starting the secure stack. Add the Docker host LAN hostname or IP to the broker certificate when the ESP32 connects from the LAN:

```bash
git clone <repository>
cd industrial-edge-monitor
cp .env.example .env
scripts/generate-mqtt-security.sh --lan-host <docker-host-lan-hostname-or-ip>
docker compose up --build -d
docker compose ps
```

Open:

- dashboard: `http://localhost:3000`;
- FastAPI documentation: `http://localhost:8000/docs`;
- API health: `http://localhost:8000/health`;
- MQTT over TLS from the host/LAN: `<docker-host-ip>:8883`.

The generated `.local/mqtt-security/` tree contains secrets, is excluded from Git and Docker build contexts, and must not be copied into images or logs. Existing complete material is left unchanged. `--force` performs only an intentional complete-bundle replacement and refuses unmarked, invalid or symlink targets. Stop or reconfigure any native service already bound to host port `8883`.

Useful lifecycle commands:

```bash
docker compose logs -f
docker compose up --build -d
docker compose down
```

`docker compose down` preserves the named SQLite and Mosquitto volumes. **`docker compose down -v` permanently removes stored telemetry, rules, events, health snapshots and retained broker data.**

The browser-facing `NEXT_PUBLIC_API_URL` is embedded during `docker compose build`. If the dashboard is opened from another machine, set it to a browser-reachable host/LAN URL before building, then rebuild the frontend image. Never use the Compose-only hostname `api` in this variable.

Full installation, configuration and end-to-end procedures are in [docs/setup.md](docs/setup.md). Container concepts, image construction and operations are explained separately in [docs/docker.md](docs/docker.md).

## MQTT topics

For device `edge-node-01`:

```text
industrial/devices/edge-node-01/telemetry
industrial/devices/edge-node-01/health
industrial/devices/edge-node-01/availability
```

The legacy `industrial/telemetry` topic remains supported and is assigned to `legacy-device`. The collector rejects per-device messages when the topic identity and payload `device_id` differ.

## Traditional development workflow

Docker is not required for day-to-day source development. With Python 3.14, Node.js 22, npm and a TLS/authenticated local Mosquitto service:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
MQTT_CLIENT_ENABLED=true \
MQTT_USERNAME=collector \
MQTT_PASSWORD_FILE=.local/mqtt-security/clients/collector.password \
MQTT_CLIENT_ID=industrial-edge-collector \
python -m backend.collector.subscriber
```

Run the API and frontend in separate terminals:

```bash
source .venv/bin/activate
uvicorn backend.api.main:app --reload
```

```bash
cd frontend
cp .env.example .env.local
npm ci
npm run dev
```

Without an ESP32, run the simulator with its dedicated legacy-publisher identity:

```bash
MQTT_CLIENT_ENABLED=true \
MQTT_USERNAME=simulator \
MQTT_PASSWORD_FILE=.local/mqtt-security/clients/simulator.password \
MQTT_CLIENT_ID=industrial-edge-simulator \
python -m backend.simulator.publisher
```

The shared `.env` keeps MQTT disabled, so the local API never starts a client or reads the password file. See [docs/setup.md](docs/setup.md) for the complete non-Docker sequence.

## Firmware

```bash
source ~/esp/esp-idf/export.sh
idf.py -C firmware menuconfig
idf.py -C firmware build
idf.py -C firmware -p /dev/ttyUSB0 flash monitor
```

Copy the generated CA to the ignored `firmware/local_secrets/mqtt_ca.pem`, then configure an `mqtts://<LAN-host>:8883` URI and the device credentials in `menuconfig`. The URI host/IP must be present in the certificate SAN; it must not be `mqtt` or `localhost` from the ESP32. Never connect a 24 V industrial signal directly to an ESP32 GPIO; use suitable conditioning and isolation.

## Quality gates

```bash
source .venv/bin/activate
pytest -q

cd frontend
npm test
npm run lint
NEXT_PUBLIC_API_URL=http://127.0.0.1:8000 npm run build

cd ..

idf.py -C firmware build
docker compose config --quiet
docker compose build
```

The GitHub Actions workflow defines separate backend, frontend, firmware and container jobs. The container job generates temporary security material, exercises positive and negative TLS/authentication/ACL cases, verifies MQTT-to-API ingestion and checks SQLite persistence across container recreation. It performs no deployment. See [docs/ci.md](docs/ci.md) for the exact workflow behavior and current limitations.

## Security boundary

The supplied Compose path encrypts MQTT, verifies the broker certificate, requires a distinct username/password identity and enforces least-privilege broker authorization. It has no automatic plaintext fallback. API/dashboard authentication, HTTPS and a hardened ingress are still absent, and locally generated credentials are not a production secret-management, provisioning, per-device rotation or revocation system. Do not expose the stack directly to the public Internet. See [MQTT security](docs/mqtt-security.md).

## Repository layout

```text
backend/       MQTT collector, persistence, alert engine and FastAPI
docker/        Mosquitto deployment configuration
firmware/      Modular ESP-IDF firmware
frontend/      Next.js monitoring dashboard and production image
docs/          Architecture, setup, API, roadmap and sprint history
tests/         Isolated backend tests
compose.yaml   Reproducible single-host stack
```

Latest completed milestone: **Sprint 15 — Secure MQTT Communication and Device Authentication**. Persistent device configuration, provisioning and credential lifecycle, API authentication, hardened HTTPS ingress, backup automation, multi-host storage and continuous delivery remain future work.
