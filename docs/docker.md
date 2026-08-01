# Docker and Docker Compose

This guide explains the container model implemented for Industrial Edge Monitor. It is the technical reference for image construction, Compose behavior, networking, persistence, health checks and routine operations. Installation and end-to-end procedures remain in [Setup and deployment](setup.md); component relationships remain in [Architecture](architecture.md); automated verification is described in [Continuous Integration](ci.md).

The implementation sources are:

- [`compose.yaml`](../compose.yaml);
- the single-stage [backend Dockerfile](../Dockerfile);
- the multi-stage [frontend Dockerfile](../frontend/Dockerfile);
- the [Mosquitto configuration](../docker/mosquitto/mosquitto.conf).

## Purpose and scope

Docker gives the project a repeatable way to construct and run the backend, frontend and broker with explicit versions, commands, configuration boundaries, network membership and persistent storage. A clean host with a compatible Docker Engine and Compose plugin can build the same declared application dependency graph and start the same four-service topology without installing Python, Node.js or Mosquitto directly on the host.

In this repository, **reproducible deployment** means that the build inputs, dependency lock files, service definitions and startup commands are versioned together and can be applied from a clean checkout. It does not mean that builds are byte-for-byte identical: base images and GitHub Actions are version-tagged rather than digest-pinned, and external package registries remain build dependencies.

The images are **production-like** because they use explicit runtime dependencies, non-root application users, a standalone frontend server, health checks and persistent volumes. They are not **production-secure**. The supplied stack has anonymous plaintext MQTT, unauthenticated HTTP services, no TLS and no hardened ingress, and it publishes ports directly on the host.

Docker solves packaging, process isolation, service discovery, repeatable topology and local persistent mounts. It does not replace:

- unit, integration or system testing;
- application and infrastructure monitoring;
- backup and restore procedures;
- TLS, authentication or authorization;
- secret management;
- high availability or multi-host storage;
- capacity planning and load testing.

## Core terminology

| Term | Meaning in this project |
| --- | --- |
| Dockerfile | A versioned recipe that constructs an image. The repository has one backend Dockerfile and one frontend Dockerfile. |
| Build context | The directory tree available to `COPY` and other build operations. The backend context is the repository root; the frontend context is `frontend/`. |
| `.dockerignore` | A context filter that prevents unnecessary or sensitive paths such as `.env`, databases, caches and build output from being sent to the builder. Each build context has its own file. |
| Image | An immutable filesystem and metadata template used to create containers. Source changes are not reflected until a relevant image is rebuilt. |
| Image tag | A readable image reference such as `industrial-edge-monitor-backend:local`. Tags can be moved to different image contents and are not cryptographic identities. |
| Image layer | An immutable build result produced by Dockerfile instructions. Layers can be reused between builds when their instruction and inputs are unchanged. |
| Build cache | Previously built layers retained to avoid repeating unchanged work. A cache improves speed; it is not an application artifact or a correctness guarantee. |
| Container | A runtime instance of an image with its own process namespace, writable layer, mounts and network interfaces. API and collector are separate containers even though they use the same image. |
| Registry | A service that stores and distributes images. This stack pulls public base images but does not publish project images to a registry. |
| Process | A running program inside a container, such as Uvicorn, the MQTT subscriber or the Next.js server. |
| PID 1 | The first process in a container. It has special signal and child-reaping responsibilities. Compose enables a small init process for API, collector and frontend. |
| Port exposure | Image metadata declared with `EXPOSE`. It documents an intended container port but does not publish that port on the host. |
| Port publishing | A Compose `ports` mapping that binds a host port to a container port, for example `8000:8000`. |
| Bind mount | A direct mapping from a host path into a container. Mosquitto receives its repository configuration file through a read-only bind mount. |
| Named volume | Docker-managed persistent storage identified by name. `sqlite-data` and `mqtt-data` survive container replacement. |
| Bridge network | A private Docker network providing container connectivity and DNS-based service discovery on one host. The Compose network is `edge`. |
| Compose project | The namespaced group of services, containers, network and volumes managed from one Compose model. A unique project name isolates CI smoke resources. |
| Compose service | A service definition such as `api` or `collector` that Compose turns into a container configuration. |
| Health check | A command executed periodically by Docker to classify a running container as `healthy` or `unhealthy`. A running process without a health check has no Docker health classification. |
| Restart policy | The rule controlling automatic process restart. All four services use `unless-stopped`. This is not monitoring or high availability. |
| Build argument | A value available while an image is built. `NEXT_PUBLIC_API_URL` is passed to the frontend build and embedded in browser code. |
| Runtime environment variable | A value provided when a container starts. Python configuration such as `DATABASE_PATH` and `MQTT_HOST` is read at runtime. |

`EXPOSE 8000` in the backend image is metadata only. It does not create a host listener. This is why the collector can show `8000/tcp` in container tooling while having no published host port.

## Project container architecture

The Compose model contains four services on the `edge` bridge network.

| Service | Image | Main process or command | Published ports | Network | Mounts | Docker health check | Responsibility |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `mqtt` | `eclipse-mosquitto:2.1.2-alpine` | Upstream entrypoint running Mosquitto with the mounted configuration | `1883:1883` | `edge` | read-only broker configuration; `mqtt-data:/mosquitto/data` | MQTT publish to `industrial/healthcheck` | Accept MQTT connections and retain broker state. |
| `api` | `industrial-edge-monitor-backend:local` | `python -m uvicorn backend.api.main:app --host 0.0.0.0 --port 8000` | `8000:8000` | `edge` | `sqlite-data:/data` | HTTP request to `/health`, which also verifies the database connection | Serve REST endpoints and read/write application state. |
| `collector` | `industrial-edge-monitor-backend:local` | `python -m backend.collector.subscriber` | none | `edge` | `sqlite-data:/data` | none | Subscribe to MQTT, validate messages, persist data and evaluate alerts. |
| `frontend` | `industrial-edge-monitor-frontend:local` | `node server.js` | `3000:3000` | `edge` | none | HTTP request to `/` | Serve the Next.js dashboard. |

API and collector share the backend image but are different containers with different main processes. Both mount the same named volume at `/data`, so both resolve `DATABASE_PATH=/data/telemetry.db` to the same SQLite database and its WAL-related files.

The collector does not publish a port. If `docker compose ps` displays `8000/tcp` for it, that value comes from `EXPOSE 8000` in the shared image. It is not bound to the host and cannot be reached through host port 8000. The collector also has no Docker health check. Its readiness is verified through a running process, a successful MQTT connection and the `Subscribed to legacy and per-device topics` log entry.

## Compose extension and YAML reuse

The top-level block below is a Compose extension field combined with a YAML anchor:

```yaml
x-backend: &backend
```

`x-backend` is not a service and does not create a container. It stores the common backend build, image, environment, volume, network, init, restart and shutdown configuration.

API and collector reuse that mapping with the YAML merge key:

```yaml
<<: *backend
```

This avoids duplicating configuration that must remain identical. Each service then supplies its own command and startup dependencies. API adds a published port and health check. Collector changes the command and waits for healthy API and MQTT dependencies.

## What happens during startup

The verification command is:

```bash
docker compose up -d --wait --wait-timeout 180
```

Compose performs these operations:

1. It reads `compose.yaml` and validates the Compose model.
2. It interpolates `${VARIABLE:-default}` expressions using the shell environment and Compose environment-file rules.
3. It resolves or creates the project-scoped `edge` network and the `sqlite-data` and `mqtt-data` named volumes.
4. It creates or reconciles the four service containers.
5. Mosquitto and API become eligible to start; there is no dependency ordering between those two services, so they may start concurrently.
6. The Mosquitto health check publishes a small MQTT message until the broker is classified `healthy`.
7. Importing the FastAPI application runs the idempotent SQLite initialization and migrations before Uvicorn is ready to serve normal requests.
8. The API health check calls `/health`; that endpoint opens the database, runs `SELECT 1` and reports the API healthy only after a successful response.
9. Collector becomes eligible to start only after both API and Mosquitto are healthy.
10. Collector initializes the database idempotently, connects to `mqtt:1883` and subscribes to the legacy and per-device topics.
11. Frontend becomes eligible to start after API is healthy. Its start relative to collector is not strictly ordered because it does not depend on collector or MQTT.
12. The frontend health check requests `/`. `--wait` returns successfully when health-checked services are healthy and the collector, which has no health check, is running. It fails if the 180-second timeout expires first.

The dependency graph guarantees the necessary prerequisites without claiming a fully sequential startup that Compose does not enforce.

Related commands have different effects:

| Command | Effect |
| --- | --- |
| `docker compose up -d` | Creates or reconciles and starts containers in the background. It does not force rebuilding an already available image. |
| `docker compose up --build -d` | Builds the project images before creating or reconciling containers, then starts them in the background. |
| `docker compose build` | Builds images only. It does not start or replace running containers. |
| `docker compose restart` | Stops and starts existing containers. It does not rebuild images, recreate containers or apply changed Compose environment/configuration. |

A source or dependency change copied into an image normally requires `docker compose build` followed by `docker compose up -d`, or the combined `docker compose up --build -d`. A restart alone continues to use the existing image and container configuration.

## Backend image

The repository-level `Dockerfile` is explicitly **single-stage**:

1. `ARG PYTHON_VERSION=3.14.4` selects `python:3.14.4-slim-bookworm` unless the build overrides the argument.
2. `ENV` disables bytecode output, enables unbuffered logs and disables pip's version-check noise.
3. `WORKDIR /app` establishes the application directory.
4. `requirements-runtime.txt` is copied before source code so dependency installation can remain cached when only backend source changes.
5. `python -m pip install --no-cache-dir --no-compile --requirement requirements-runtime.txt` installs exact runtime package versions. Exact Python pins improve repeatability; the base image and registry downloads are still tag- and registry-dependent.
6. Group and user `app` are created with UID and GID `10001`, and `/data` is created with matching ownership.
7. `COPY --chown=app:app backend ./backend` copies only backend application code with non-root ownership.
8. `USER app` prevents Uvicorn and the collector from running as root.
9. `EXPOSE 8000` records intended API port metadata; it does not publish a host port.
10. The default `CMD` runs Uvicorn on `0.0.0.0:8000`.

Compose explicitly supplies the API command and overrides the image command for collector with `python -m backend.collector.subscriber`. Sharing the image keeps Python dependencies and application code identical while retaining separate processes, lifecycle, logs and failure domains.

## Frontend image

The frontend Dockerfile has three named stages based on `node:22.22.1-alpine3.22`.

The current application manifest pins Next.js `16.2.12`; `package-lock.json` records the complete npm dependency graph used by `npm ci`.

### `dependencies`

- Copies `package.json` and `package-lock.json` before application source.
- Runs `npm ci`, which installs exactly the lockfile graph and rejects an inconsistent manifest/lockfile pair.
- Produces a dependency layer reusable while the manifests remain unchanged.

### `build`

- Reuses `node_modules` from `dependencies`.
- Disables Next.js telemetry.
- Receives `NEXT_PUBLIC_API_URL` as a build argument.
- Fails early when the build argument is empty.
- Exposes the value to `next build`, which embeds it into browser JavaScript.
- Copies the frontend source and generates the Next.js standalone output.

### `runtime`

- Starts from a clean Node Alpine image rather than the complete build filesystem.
- Creates non-root user `nextjs` with UID `1001`.
- Copies only `public`, `.next/standalone` and `.next/static` with the correct ownership.
- Runs `node server.js` as `nextjs` on `0.0.0.0:3000`.

Development source, npm cache, tests and general build tooling are not copied into the runtime stage. This selective copy is why the final image is smaller than an image containing the complete source tree and all development dependencies. The standalone tracer can still include runtime files such as sharp that Next.js considers reachable; residual frontend dependency findings are documented in [Frontend](frontend.md#residual-dependency-advisories).

## Process and shutdown behavior

Compose sets `init: true` for API, collector and frontend. Docker inserts a minimal init process as PID 1 to forward signals and reap orphaned child processes. Mosquitto uses its upstream entrypoint without this Compose option.

When a service is stopped, Compose sends `SIGTERM` and waits for the configured `stop_grace_period` of 15 seconds. If the process has not exited by then, Docker sends `SIGKILL`. `SIGKILL` cannot be handled and prevents further cleanup.

The collector installs handlers for `SIGTERM` and `SIGINT`. It logs shutdown, asks the Paho MQTT client to disconnect and lets `loop_forever()` exit. This is graceful process shutdown; it does not guarantee broker delivery of messages that were never accepted before termination.

All services use `restart: unless-stopped`. Docker can restart them after an unexpected exit or daemon restart unless an operator explicitly stopped them. This policy does not measure correctness, alert an operator, provide redundancy or create high availability.

## Networking and address resolution

All services join the project-scoped bridge network derived from `edge`. Compose provides internal DNS records matching service names. Collector therefore connects to `mqtt:1883`; it does not use the host-published port.

A browser and an ESP32 are outside the Compose network:

- a browser running in the same VM or host can use `http://localhost:3000` and `http://localhost:8000`;
- a browser on another computer must use a reachable LAN address of the Docker host;
- an ESP32 must use the Docker host's LAN address and port `1883`;
- `mqtt` and `api` are Compose-only DNS names and are not valid from browser JavaScript or firmware outside the bridge network;
- `localhost` on an ESP32 refers to the ESP32 itself.

The mappings mean:

- `1883:1883`: host TCP 1883 forwards to Mosquitto TCP 1883;
- `8000:8000`: host TCP 8000 forwards to the API container TCP 8000;
- `3000:3000`: host TCP 3000 forwards to the frontend container TCP 3000.

No host IP is specified in these mappings, so Docker publishes them on available host interfaces. Tooling commonly displays IPv4 `0.0.0.0` and, when enabled, IPv6 `[::]`. This is broader than an internal-only listener and is one reason the stack must remain on a trusted, firewalled network.

A native Mosquitto process already bound to host port 1883 prevents the Compose broker from starting. Stop that service or reconfigure one of the deployments deliberately; this repository keeps the standard `1883:1883` mapping.

## Configuration

The repository tracks `.env.example` as a non-secret template and ignores `.env`. Compose uses shell values and environment-file rules to interpolate expressions such as `${LOG_LEVEL:-INFO}` before it creates containers. The `.env` file is not mounted into the containers.

Python settings are runtime values supplied to API and collector. `backend.core.config.Settings` validates them with Pydantic. `APP_ENV=production` enables stricter checks, including an external database path and a non-loopback MQTT host. It does not enable TLS, authentication, authorization or a hardened ingress.

`NEXT_PUBLIC_API_URL` is different:

- Compose passes it as a frontend build argument;
- the Dockerfile rejects an empty value;
- Next.js embeds it in browser-delivered JavaScript during `next build`;
- changing the runtime container environment cannot replace the compiled value;
- the frontend image must be rebuilt after changing it;
- it must be reachable from the user's browser, so `http://api:8000` is invalid outside the Compose network;
- every `NEXT_PUBLIC_*` value is public by design and must never contain a secret.

The complete variable table and environment preparation procedure remain in [Setup and deployment](setup.md#configuration).

## Persistence and SQLite

Container writable layers are tied to individual containers. Replacing a container discards changes stored only in that layer. Named volumes have a separate lifecycle:

- `sqlite-data` is mounted read/write at `/data` in both API and collector;
- `mqtt-data` is mounted at `/mosquitto/data` and stores Mosquitto persistence, including retained state.

The Python services use `/data/telemetry.db`. SQLite also creates `telemetry.db-wal` and `telemetry.db-shm` as needed in the same directory, so all database-related files remain on the shared volume. Replacing API or collector and mounting the same volume exposes the same database state to the new container.

Each file-backed connection enables:

- `PRAGMA foreign_keys = ON`;
- `PRAGMA journal_mode = WAL`;
- `PRAGMA synchronous = NORMAL`;
- SQLite connect timeout from `DATABASE_TIMEOUT_SECONDS`;
- matching millisecond `PRAGMA busy_timeout`.

WAL improves overlap between readers and the writer. SQLite still serializes writes and supports only one writer at a time. The timeout allows temporary lock contention to clear; it does not remove the single-writer constraint. The supported topology is one API process and one collector process on one Docker host with a local named volume. It is not a multi-replica or network-filesystem database design.

These commands differ materially:

```bash
docker compose down
docker compose down -v
```

`down` removes the project's containers and default network but preserves named volumes. `down -v` also deletes `sqlite-data` and `mqtt-data`.

> **Warning:** `docker compose down -v` permanently deletes telemetry, health snapshots, devices, alert rules, alert state, alert history and retained MQTT state stored in the named volumes. Do not run it unless that exact data destruction is intended and any required backup has been verified.

Docker volumes are persistence, not backup. Automated backup and restore are not implemented.

## Health checks and service state

The implemented checks are:

- Mosquitto: `mosquitto_pub` connects to `127.0.0.1:1883` inside the broker container and publishes to `industrial/healthcheck`;
- API: Python requests `/health`; the endpoint verifies that a SQLite connection and `SELECT 1` succeed;
- frontend: Node.js fetches `http://127.0.0.1:3000` and requires a successful HTTP status;
- collector: no Docker health check.

State terms are distinct:

- **process started**: Docker launched the configured command; it may still be initializing or may be functionally blocked;
- **Up**: the container's main process is running;
- **healthy**: a configured health check is currently succeeding;
- **unhealthy**: a configured health check has failed for the configured retry threshold while the process may still be running;
- **restarting**: the process exited and Docker is applying the restart policy.

Do not describe the collector as Docker-healthy. Check `docker compose ps`, then confirm its MQTT connection and subscription in `docker compose logs collector`.

## Operational commands

Validate the model without printing resolved configuration:

```bash
docker compose config --quiet
```

Use `docker compose config` only when the resolved model itself is required for debugging; review its output before sharing because interpolation can include environment-derived values.

Build and start:

```bash
docker compose build
docker compose up -d --wait --wait-timeout 180
docker compose up --build -d
docker compose restart
```

Inspect status, images and logs:

```bash
docker compose ps
docker compose images
docker compose logs -f
docker compose logs -f collector
```

Inspect runtime identities and the shared mount:

```bash
docker compose exec api id
docker compose exec collector id
docker compose exec frontend id
docker compose exec api ls -lh /data
docker compose exec collector ls -lh /data
```

If a small set of known non-sensitive variables must be confirmed, request only those names:

```bash
docker compose exec api printenv APP_ENV DATABASE_PATH MQTT_HOST
```

Do not dump all environment variables into logs or reports.

Stop processes without removing containers, or remove containers while preserving volumes:

```bash
docker compose stop
docker compose down
```

Interpret `docker compose ps` columns as follows:

- `NAME`: project-scoped container name;
- `IMAGE`: image reference used by that container;
- `COMMAND`: main command, often truncated in terminal output;
- `SERVICE`: Compose service definition that created the container;
- `STATUS`: process state plus health classification when a health check exists;
- `PORTS`: published host mappings and image-exposed container ports. `0.0.0.0:8000->8000/tcp` is published; bare `8000/tcp` is exposed metadata only.

## Smoke test

A smoke test checks that the most important path works after assembling the system. It is narrower than a complete end-to-end acceptance suite, while spanning more components than an isolated unit test. An integration test verifies component collaboration; this repository's container job is both a stack integration check and a smoke test of the main data path.

The implemented smoke sequence:

1. uses a unique `COMPOSE_PROJECT_NAME` so containers, network and volumes are isolated from other projects;
2. validates and builds the Compose model;
3. starts the stack and waits for the implemented health checks;
4. confirms collector subscription through its logs;
5. confirms API and collector mount the same named volume at `/data`;
6. publishes a known legacy telemetry sample;
7. polls the API until that sample appears and records its exact telemetry ID;
8. removes and recreates API and collector without replacing the volume;
9. requires the same telemetry ID after recreation;
10. uses a shell `trap` to print status and logs on failure and remove only the project-scoped smoke resources.

Checking the exact ID proves that the row observed after recreation is the specific previously inserted record, not a new row with coincidentally equal field values.

This smoke test does not establish security, load capacity, browser behavior, hardware integration, long-duration availability, fault tolerance or multi-host operation. The local manual procedure is in [Setup and deployment](setup.md#mqtt-smoke-test); the automated implementation is described in [Continuous Integration](ci.md#container-job).

## Troubleshooting

| Symptom | Checks and action |
| --- | --- |
| Host port 1883 is already in use | Check the host listener with `ss -ltnp`; stop or reconfigure the native Mosquitto service before starting the Compose broker. Do not silently change the project mapping. |
| A service is `unhealthy` | Run `docker compose ps`, then inspect only that service with `docker compose logs --tail 200 <service>`. Check the health command and its dependency, such as SQLite for API. |
| Collector is `Up` but not subscribed | Run `docker compose logs -f collector`; look first for connection errors and then for `Subscribed to legacy and per-device topics`. Confirm broker health and `MQTT_HOST=mqtt`. |
| Frontend responds but browser API calls fail | Use browser developer tools to inspect the requested URL and CORS response. Confirm that the compiled URL is reachable from the browser, not merely from a container. |
| `NEXT_PUBLIC_API_URL` is stale | Rebuild the frontend image and reconcile its container. `docker compose restart frontend` alone cannot replace a value embedded during build. |
| Permission error under `/data` | Compare `docker compose exec api id` and `docker compose exec collector id`, then inspect `/data` from both containers. Both should run as UID/GID `10001` and mount the same volume read/write. |
| Database is locked | Inspect API and collector logs for lock errors, confirm only the supported single API and collector processes use the file, and verify the configured timeout. Do not place the SQLite volume on an arbitrary network filesystem. |
| Source changed but behavior did not | Rebuild the affected image. A restart reuses the existing image and container configuration. |
| Services after Docker daemon restart | `restart: unless-stopped` normally restarts services not explicitly stopped. Confirm with `docker compose ps` and inspect logs rather than assuming application readiness. |
| Need to inspect images or volumes | Use `docker compose images`, `docker volume ls` and `docker volume inspect <project-volume>`. Confirm the Compose project name before acting on a volume. |
| Docker consumes unexpected disk space | Start with `docker system df` and inspect images, build cache and volumes individually. Do not use broad destructive prune commands as a diagnostic shortcut. |
| Build or pull fails | Check registry/network availability and the first failing Dockerfile instruction. A cached local image can hide a temporary registry problem until a clean build. |

## Security boundary and future hardening

Current controls and limitations are explicit:

- application processes run as non-root users;
- runtime dependencies and major tool versions are version-pinned, but images are referenced by tags rather than immutable digests;
- Mosquitto allows anonymous plaintext MQTT;
- MQTT, API and frontend ports are published on all available host interfaces;
- API and frontend have no authentication;
- there is no reverse proxy or TLS termination;
- there is no infrastructure monitoring or alert delivery for container failures;
- residual npm advisories remain tracked and scoped in [Frontend](frontend.md#residual-dependency-advisories);
- the supported environment is a trusted, firewalled LAN or lab on one host.

Future hardening includes TLS, broker and API authentication, a hardened ingress, secret management, infrastructure monitoring, automated backup and restore, image digest pinning, dependency provenance and a database/storage design suitable for multiple hosts. CI behavior and its separate future evolution toward delivery or deployment are documented in [Continuous Integration](ci.md).
