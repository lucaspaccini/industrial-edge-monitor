# Industrial Edge Monitor

Industrial Edge Monitor is an end-to-end ESP32/IoT monitoring platform. An ESP32 reads a BME280, publishes validated telemetry and diagnostics over MQTT, and a Python/Next.js stack stores, evaluates and displays data per device.

The project is both a production-oriented Embedded/IoT/Edge portfolio and the evolving foundation of a single-host edge-monitoring product.

## Current capabilities

- Modular ESP-IDF 6.0.2 firmware with BME280 acquisition, SNTP UTC time and recoverable MQTT transport.
- Separate per-device telemetry, health and availability topics, with explicit legacy-topic compatibility.
- Strict collector validation, idempotent SQLite migrations and device-scoped FastAPI endpoints.
- Persistent threshold alerts with dwell time, hysteresis and active/resolved event history.
- Next.js 16 dashboard whose device selector scopes telemetry, statistics, health and alerts together.
- Reproducible four-service Docker Compose stack and separate CI jobs for backend, frontend, firmware and containers.

## Architecture

```text
ESP32 ──MQTT──► Mosquitto ──► collector ──► SQLite volume ◄── FastAPI
                    ▲                                      │
                    │                                      ▼
                  LAN:1883                    browser ◄── Next.js
```

The API and collector reuse one production-like Python image. Mosquitto, both Python processes and the Next.js standalone server share a private Compose network. Only MQTT `1883`, API `8000` and dashboard `3000` are published to the host.

See [architecture](docs/architecture.md), the [Docker and Compose guide](docs/docker.md), the [CI guide](docs/ci.md), the [backend model](docs/backend.md) and the [firmware README](firmware/README.md) for component details.

## Docker Compose quick start

Requirements: Docker Engine with the modern Compose and Buildx plugins, plus outbound access for the initial image/dependency build.

The stack intentionally publishes MQTT as `1883:1883`. Stop or reconfigure any native Mosquitto service already bound to host port `1883` before starting Compose; the mapping is not changed automatically.

```bash
git clone <repository>
cd industrial-edge-monitor
cp .env.example .env
docker compose up --build -d
docker compose ps
```

Open:

- dashboard: `http://localhost:3000`;
- FastAPI documentation: `http://localhost:8000/docs`;
- API health: `http://localhost:8000/health`;
- MQTT from the host/LAN: `<docker-host-ip>:1883`.

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

Docker is not required for day-to-day source development. With Python 3.14, Node.js 22, npm and a local Mosquitto service:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
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

Without an ESP32, `python -m backend.simulator.publisher` publishes compatible legacy telemetry. See [docs/setup.md](docs/setup.md) for the complete non-Docker sequence.

## Firmware

```bash
source ~/esp/esp-idf/export.sh
idf.py -C firmware menuconfig
idf.py -C firmware build
idf.py -C firmware -p /dev/ttyUSB0 flash monitor
```

Configure the broker as the Docker host's LAN IP, not `mqtt` or `localhost`. Never connect a 24 V industrial signal directly to an ESP32 GPIO; use suitable conditioning and isolation.

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

The GitHub Actions workflow defines separate backend, frontend, firmware and container jobs. Equivalent checks have passed locally; the workflow run after push is the final repository gate. The container job starts an isolated stack, sends valid legacy MQTT telemetry, verifies it through FastAPI and checks SQLite persistence across container recreation. It performs no deployment. See [docs/ci.md](docs/ci.md) for the exact workflow behavior and current limitations.

## Security boundary

The supplied Compose stack is for a trusted local or lab network. Mosquitto intentionally allows anonymous, unencrypted connections so an ESP32 can connect during development; the API and frontend also have no authentication or reverse proxy. Do not expose this configuration to the public Internet. TLS, broker/API authentication, a hardened ingress, automated backup and restore, and multi-host storage remain separate future work.

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

Latest completed milestone: **Sprint 14 — Reproducible Deployment, Environment Configuration and Continuous Integration**. Persistent device configuration, TLS/authentication, hardened ingress, backup automation, multi-host storage and continuous delivery remain future work.
