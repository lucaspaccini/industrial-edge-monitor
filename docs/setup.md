# Setup and deployment

This document owns installation, configuration and end-to-end operating procedures. See [Docker and Compose](docker.md) for container internals and operational concepts, [Continuous Integration](ci.md) for GitHub Actions behavior, and [Architecture](architecture.md) for component relationships and data flow.

## Docker Compose deployment

### Prerequisites

- Git;
- Docker Engine;
- the modern Docker Compose plugin (`docker compose`);
- the Docker Buildx plugin used by `docker compose build`;
- outbound Internet access for the initial base-image and dependency downloads.

Host ports `1883`, `8000` and `3000` must be available. In particular, stop or reconfigure a native Mosquitto service already listening on `1883` before `docker compose up`; Compose intentionally keeps the `1883:1883` mapping.

Clone and prepare the local environment file:

```bash
git clone <repository>
cd industrial-edge-monitor
cp .env.example .env
```

The template contains no credentials. `.env`, SQLite files, caches and local firmware/frontend configuration are ignored by Git and excluded from image build contexts.

### Configuration

Backend processes read configuration only through `backend.core.config.Settings`. Invalid ports, timeouts, CORS origins, MQTT hosts/topics, device IDs or environment names stop the process with a Pydantic validation error.

| Variable | Local default | Standard Compose value | Meaning |
| --- | --- | --- | --- |
| `APP_ENV` | `development` | `production` | Application environment: `development`, `test` or `production` |
| `LOG_LEVEL` | `INFO` | `${LOG_LEVEL:-INFO}` | Python root log level |
| `APP_NAME` | `Industrial Edge Monitor API` | same unless overridden | API display name |
| `APP_VERSION` | `0.1.0` | same unless overridden | API-reported application version |
| `APP_DESCRIPTION` | `REST API for Industrial Edge Monitor` | same unless overridden | API description |
| `DATABASE_PATH` | `data/telemetry.db` | `/data/telemetry.db` | SQLite file used by API and collector |
| `DATABASE_TIMEOUT_SECONDS` | `30` | `${DATABASE_TIMEOUT_SECONDS:-30}` | SQLite lock wait and busy timeout |
| `DEFAULT_HISTORY_LIMIT` | `100` | `${DEFAULT_HISTORY_LIMIT:-100}` | Default REST history limit |
| `MQTT_HOST` | `localhost` | `mqtt` | Broker host visible to the collector |
| `MQTT_PORT` | `1883` | `1883` | MQTT TCP port |
| `MQTT_KEEPALIVE_SECONDS` | `60` | `${MQTT_KEEPALIVE_SECONDS:-60}` | Collector MQTT keepalive |
| `MQTT_TOPIC` | `industrial/telemetry` | same | Explicit legacy telemetry topic |
| `MQTT_TOPIC_PREFIX` | `industrial/devices` | same | Per-device topic prefix |
| `LEGACY_DEVICE_ID` | `legacy-device` | same | Identity assigned to legacy telemetry |
| `DEVICE_OFFLINE_TIMEOUT_SECONDS` | `150` | same | Maximum `last_seen` age for effective online state |
| `CORS_ORIGINS` | localhost origins | same unless overridden | Comma-separated browser origins, including scheme and optional port |
| `NEXT_PUBLIC_API_URL` | `http://127.0.0.1:8000` | `http://localhost:8000` fallback | Browser-reachable FastAPI URL embedded into the frontend build |

Development connectivity defaults are rejected when `Settings` is started as `production`; Compose therefore supplies `/data/telemetry.db` and the internal broker hostname explicitly.

`APP_ENV=production` enables stricter server-side configuration validation. It does not provide persistent ESP32 configuration, TLS, authentication or a hardened ingress.

Python settings above are runtime variables. `NEXT_PUBLIC_API_URL` is different: Next.js inserts it into browser JavaScript during `next build`. Changing it on an already-built container has no effect. Set it before `docker compose build` and rebuild the frontend whenever the public API address changes.

For access from another LAN machine, for example:

```bash
NEXT_PUBLIC_API_URL=http://192.168.1.20:8000 docker compose build frontend
docker compose up -d frontend
```

Also include the dashboard's actual browser origin in `CORS_ORIGINS` before starting the API.

### Start and inspect

```bash
docker compose up --build -d
docker compose ps
curl --fail http://localhost:8000/health
curl --fail http://localhost:3000/
```

Follow all logs or one process:

```bash
docker compose logs -f
docker compose logs -f mqtt collector
docker compose logs -f api frontend
```

Rebuild after source, dependency or public frontend environment changes:

```bash
docker compose build
docker compose up -d
```

### MQTT smoke test

Wait until the collector reports `Subscribed to legacy and per-device topics`, then publish a valid legacy sample through the broker container:

```bash
docker compose exec mqtt mosquitto_pub \
  -h 127.0.0.1 \
  -t industrial/telemetry \
  -m '{"timestamp":"2026-08-01T12:00:00Z","temperature":23.75,"humidity":45.5,"machine_status":"unknown"}'

curl --fail 'http://localhost:8000/telemetry/latest?device_id=legacy-device'
```

This exercises Mosquitto, the collector, validation, SQLite and FastAPI without changing the telemetry payload or topic contract.

### Persistence and shutdown

`sqlite-data` stores the SQLite database and its WAL files. `mqtt-data` stores retained Mosquitto state. Recreating application containers does not remove either named volume:

```bash
docker compose stop api collector
docker compose rm -f api collector
docker compose up -d api collector
```

Normal shutdown preserves data:

```bash
docker compose down
```

Destructive cleanup removes both named volumes:

```bash
docker compose down -v
```

**Do not use `down -v` when the stored telemetry, alert history or retained MQTT state must be preserved.** Back up the Docker volume before material upgrades or host maintenance.

### Addressing rules

- Containers use Compose DNS: the collector reaches `mqtt:1883`.
- Browser code cannot resolve Compose service names; it uses the host/LAN API address compiled into `NEXT_PUBLIC_API_URL`.
- An ESP32 cannot resolve the private Compose name either; configure the Docker host's LAN IP and exposed port `1883` in firmware `menuconfig`.
- `localhost` on the ESP32 means the ESP32 itself, not the development computer.

### Scope and security

This is a single-host deployment using a local Docker volume. WAL and a bounded busy timeout support the one API process plus one collector process, but SQLite is not being presented as a replicated or network-filesystem database. Do not run multiple API/collector replicas against this file without a separate database design.

Mosquitto explicitly permits anonymous plaintext MQTT and port `1883` is published for the ESP32. FastAPI and Next.js have no authentication or reverse proxy. Keep the stack on a trusted, firewalled lab/LAN network; it is not production-secure for Internet exposure.

Automated backup/restore and multi-host storage are not implemented. The documented volume-copy and retention guidance is an operational precaution, not a completed backup subsystem.

## Traditional non-Docker workflow

### Backend and broker

Requirements: Python 3.14 and a local Mosquitto service.

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
cp .env.example .env
sudo systemctl start mosquitto
```

Run the long-lived processes from the repository root in separate terminals:

```bash
source .venv/bin/activate
python -m backend.collector.subscriber
```

```bash
source .venv/bin/activate
uvicorn backend.api.main:app --reload
```

Database initialization and migrations are idempotent. The configured relative path is resolved from the repository root and its parent directory is created automatically. The collector does not hot-reload; restart it after changing ingestion or alert-engine code.

Optional legacy telemetry simulator:

```bash
source .venv/bin/activate
python -m backend.simulator.publisher
```

### Frontend

Requirements: Node.js 22 and npm.

```bash
cd frontend
cp .env.example .env.local
npm ci
npm run dev
```

Open `http://localhost:3000`. For production-style local validation:

```bash
npm test
npm run lint
NEXT_PUBLIC_API_URL=http://127.0.0.1:8000 npm run build
npm run start
```

### Firmware

ESP-IDF 6.0.2 remains external to the repository:

```bash
source ~/esp/esp-idf/export.sh
idf.py -C firmware menuconfig
idf.py -C firmware build
```

Hardware flashing and serial monitoring remain manual and are documented in [firmware setup](firmware-setup.md).
