# Setup and deployment

This document owns installation, configuration and end-to-end operating procedures. See [Docker and Compose](docker.md) for container internals and operational concepts, [Continuous Integration](ci.md) for GitHub Actions behavior, and [Architecture](architecture.md) for component relationships and data flow.

## Docker Compose deployment

### Prerequisites

- Git;
- Docker Engine;
- the modern Docker Compose plugin (`docker compose`);
- the Docker Buildx plugin used by `docker compose build`;
- outbound Internet access for the initial base-image and dependency downloads.

Host ports `8883`, `8000` and `3000` must be available. Stop or reconfigure a native broker already listening on `8883` before `docker compose up`.

Clone and prepare the local environment file:

```bash
git clone <repository>
cd industrial-edge-monitor
cp .env.example .env
scripts/generate-mqtt-security.sh --lan-host <docker-host-lan-hostname-or-ip>
```

The template contains no credentials. The generator creates ignored local CA/server material and random per-client credentials without printing secrets. `.env`, `.local/`, SQLite files, caches and local firmware/frontend configuration are ignored by Git and excluded from image build contexts. Per-device lifecycle and rotation are not implemented; see [MQTT security](mqtt-security.md) before using an ESP32 or replacing a complete bundle.

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
| `MQTT_PORT` | `8883` | `8883` | MQTT TLS port |
| `MQTT_KEEPALIVE_SECONDS` | `60` | `${MQTT_KEEPALIVE_SECONDS:-60}` | Collector MQTT keepalive |
| `MQTT_CLIENT_ENABLED` | `false` in `.env.example` | `true` only for collector; false for API | Enable the long-lived MQTT client explicitly per process |
| `MQTT_TLS_ENABLED` | `true` | `true` | Enable verified TLS; mandatory for a production MQTT client |
| `MQTT_CA_CERT_PATH` | `.local/mqtt-security/ca/ca.crt` | `/run/mqtt-client/ca.crt` | Trusted broker CA certificate |
| `MQTT_USERNAME` | `collector` | `${MQTT_COLLECTOR_USERNAME:-collector}` | Authenticated MQTT identity |
| `MQTT_PASSWORD_FILE` | generated collector password | `/run/mqtt-client/password` | Read-only secret file, never logged |
| `MQTT_CLIENT_ID` | `industrial-edge-collector` | `${MQTT_COLLECTOR_CLIENT_ID:-industrial-edge-collector}` | Protocol session identity, separate from username/device ID |
| `MQTT_RECONNECT_MIN_SECONDS` | `1` | same | Initial reconnect delay |
| `MQTT_RECONNECT_MAX_SECONDS` | `30` | same | Maximum reconnect delay |
| `MQTT_TOPIC` | `industrial/telemetry` | same | Explicit legacy telemetry topic |
| `MQTT_TOPIC_PREFIX` | `industrial/devices` | same | Per-device topic prefix |
| `LEGACY_DEVICE_ID` | `legacy-device` | same | Identity assigned to legacy telemetry |
| `DEVICE_OFFLINE_TIMEOUT_SECONDS` | `150` | same | Maximum `last_seen` age for effective online state |
| `CORS_ORIGINS` | localhost origins | same unless overridden | Comma-separated browser origins, including scheme and optional port |
| `NEXT_PUBLIC_API_URL` | `http://127.0.0.1:8000` | `http://localhost:8000` fallback | Browser-reachable FastAPI URL embedded into the frontend build |

Development connectivity defaults are rejected when `Settings` is started as `production`; Compose therefore supplies `/data/telemetry.db`, internal broker hostname and mounted TLS/authentication material explicitly. Missing/unreadable files, partial credentials and inconsistent TLS settings fail before connection. The API does not enable an MQTT client and therefore receives no MQTT secret.

`APP_ENV=production` makes TLS and authentication mandatory for enabled Python MQTT clients. It does not provide persistent ESP32 provisioning, API authentication or a hardened HTTPS ingress.

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

Wait until the collector reports `Subscribed to legacy and per-device topics`, then publish a valid device sample from the host with its ignored TLS option file:

```bash
mosquitto_pub \
  -o .local/mqtt-security/clients/edge-node-01.host.conf \
  -t industrial/devices/edge-node-01/telemetry \
  -m '{"device_id":"edge-node-01","timestamp":"2026-08-01T12:00:00Z","temperature":23.75,"humidity":45.5,"machine_status":"unknown"}'

curl --fail 'http://localhost:8000/telemetry/latest?device_id=edge-node-01'
```

This exercises TLS, authentication, authorization, Mosquitto, collector validation, SQLite and FastAPI without changing the telemetry payload or topic contract. Run `scripts/compose-security-smoke.sh` for isolated negative tests, Last Will and persistence verification.

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

- Containers use Compose DNS: the collector reaches `mqtt:8883` and verifies the `mqtt` certificate SAN.
- Browser code cannot resolve Compose service names; it uses the host/LAN API address compiled into `NEXT_PUBLIC_API_URL`.
- An ESP32 cannot resolve the private Compose name either; configure an `mqtts://` URI containing the Docker host LAN hostname/IP and port `8883`, and include that exact value in the generated certificate SAN.
- `localhost` on the ESP32 means the ESP32 itself, not the development computer.

### Scope and security

This is a single-host deployment using a local Docker volume. WAL and a bounded busy timeout support the one API process plus one collector process, but SQLite is not being presented as a replicated or network-filesystem database. Do not run multiple API/collector replicas against this file without a separate database design.

Mosquitto requires verified TLS, username/password authentication and least-privilege authorization on port `8883`; anonymous and plaintext connections are not part of the standard path. FastAPI and Next.js still have no authentication, HTTPS termination or reverse proxy. Keep the stack on a trusted, firewalled lab/LAN network; it is not production-secure for Internet exposure.

Automated backup/restore and multi-host storage are not implemented. The documented volume-copy and retention guidance is an operational precaution, not a completed backup subsystem.

## Traditional non-Docker workflow

### Backend and broker

Requirements: Python 3.14 and a separately configured TLS/authenticated Mosquitto service. The standard generated Compose material can also be used for host processes while the Compose broker runs.

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
cp .env.example .env
scripts/generate-mqtt-security.sh --lan-host localhost
```

Run the collector explicitly with its dedicated identity:

```bash
source .venv/bin/activate
MQTT_CLIENT_ENABLED=true \
MQTT_USERNAME=collector \
MQTT_PASSWORD_FILE=.local/mqtt-security/clients/collector.password \
MQTT_CLIENT_ID=industrial-edge-collector \
python -m backend.collector.subscriber
```

Run the API separately. It inherits `MQTT_CLIENT_ENABLED=false`, does not construct an MQTT client and does not read the configured password file:

```bash
source .venv/bin/activate
uvicorn backend.api.main:app --reload
```

Database initialization and migrations are idempotent. The configured relative path is resolved from the repository root and its parent directory is created automatically. The collector does not hot-reload; restart it after changing ingestion or alert-engine code.

Optional legacy telemetry simulator:

```bash
source .venv/bin/activate
MQTT_CLIENT_ENABLED=true \
MQTT_USERNAME=simulator \
MQTT_PASSWORD_FILE=.local/mqtt-security/clients/simulator.password \
MQTT_CLIENT_ID=industrial-edge-simulator \
python -m backend.simulator.publisher
```

The simulator's `simulator` role can publish only `industrial/telemetry`; it must not reuse the collector identity.

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

Copy the generated public CA and configure the secure URI/device credentials as described in [firmware setup](firmware-setup.md). Hardware flashing and serial monitoring remain manual.
