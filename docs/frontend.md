
# Frontend

## Overview

The frontend is built with **Next.js 16** and provides a real-time dashboard for device-scoped industrial telemetry and health.

It consumes the REST API exposed by the FastAPI backend and displays live telemetry data stored in the SQLite database.

---

## Technology Stack

| Technology   | Purpose                         |
| ------------ | ------------------------------- |
| Next.js 16   | React framework with App Router |
| React 19     | User interface library          |
| TypeScript   | Static typing                   |
| Tailwind CSS | Utility-first CSS framework     |
| shadcn/ui    | Reusable UI components          |
| Radix UI     | Accessible UI primitives        |
| Lucide React | Icons                           |
| Recharts     | Telemetry visualization         |

---

## Folder Structure

```text
src/
├── app/
├── components/
│   ├── charts/
│   ├── dashboard/
│   ├── layout/
│   └── ui/
├── hooks/
├── lib/
├── services/
└── types/
```

---

## Architecture

The frontend follows a layered architecture.

```text
Dashboard Components
        │
        ▼
Custom Hooks
        │
        ▼
Service Layer
        │
        ▼
API Client
        │
        ▼
FastAPI Backend
```

Each layer has a single responsibility:

* Components are responsible for rendering the UI.
* Hooks manage component state and periodic updates.
* Services perform REST API calls.
* The API client centralizes HTTP communication.

---

## Real-Time Updates

Telemetry data is refreshed automatically every few seconds through a custom React hook.

This approach keeps the dashboard synchronized with the backend while maintaining a clear separation between presentation and data access logic.

## Configuration

Use Node.js 24.19.0. The repository-root `.nvmrc` pins the local baseline and `package.json` declares `>=24.19.0 <25`, allowing compatible later Node 24 patches without silently moving to the next major. Install exactly the dependency graph recorded by npm 11.17.0 in `package-lock.json`, with lifecycle scripts disabled, then copy the local environment template:

```bash
nvm install
nvm use
node --version  # v24.19.0
cd frontend
npm ci --ignore-scripts
cp .env.example .env.local
```

`NEXT_PUBLIC_API_URL` is the FastAPI base URL used by browser-side requests. It must therefore be reachable from the user's browser, normally `http://127.0.0.1:8000` for local development. Compose DNS names such as `api` and `mqtt` are resolvable by containers, not by the browser or an ESP32 on the LAN.

Next.js embeds every `NEXT_PUBLIC_` value into the JavaScript bundle during `next build`. The URL is a build-time setting: changing the container environment after the image has been built does not update it. Rebuild the frontend image when deploying with a different API address.

## Verification

Run all frontend quality gates from `frontend/`:

```bash
npm test
npm run lint
NEXT_PUBLIC_API_URL=http://127.0.0.1:8000 npm run build
```

## Production Image

`next.config.ts` enables Next.js standalone output. The dashboard does not import or render `next/image`; image optimization remains disabled globally, so direct `/_next/image` requests return `404` before Next.js invokes `sharp`. The multi-stage `frontend/Dockerfile` uses Node.js 24.19.0, installs the lockfile with lifecycle scripts disabled, builds the application with an explicit public API URL, and copies only the generated standalone server, static assets and public files into the runtime stage. The final container runs `node server.js` as a non-root user on `0.0.0.0:3000`; it does not contain or run the development server.

Build and run it independently with:

```bash
cd frontend
docker build \
  --build-arg NEXT_PUBLIC_API_URL=http://127.0.0.1:8000 \
  -t industrial-edge-monitor-frontend .
docker run --rm -p 3000:3000 industrial-edge-monitor-frontend
```

The repository-level Compose workflow manages this image together with Mosquitto, the collector and the API. The production image is intended for the documented local/single-host deployment; exposing a self-hosted Next.js server directly to the public Internet would additionally require a hardened reverse proxy and controls outside the current sprint.

## Dependency advisory status

Sprint 16 upgraded the direct runtime dependency from Next.js 16.2.12 to stable 16.3.0. The Next.js tree now pins PostCSS 8.5.23 and admits sharp 0.35.3; the separate Tailwind/shadcn build tree resolves PostCSS 8.5.26. `npm audit` and `npm audit --omit=dev` both report zero vulnerabilities.

The earlier PostCSS findings `GHSA-qx2v-qp2m-jg93`, `GHSA-6g55-p6wh-862q`, `GHSA-r28c-9q8g-f849` and `GHSA-fxqj-rqcc-2cmp` are resolved by the patched PostCSS versions. PostCSS remains a build-time dependency and was confirmed absent from the standalone runtime image.

The earlier sharp finding `GHSA-f88m-g3jw-g9cj` is resolved by sharp 0.35.3. Sharp remains present in the standalone trace because it is an optional Next.js runtime dependency, but `images.unoptimized` is intentionally retained: the dashboard still does not use `next/image`, and the optimizer endpoint was locally verified to return `404`. No downgrade, preview/canary release, forced audit fix or unsupported transitive override was used.

---

## Dashboard

Current dashboard features:

* Temperature card
* Humidity card
* Machine status card
* Timestamp card
* Temperature history chart
* Telemetry statistics
* Shared device selector
* Effective online/offline availability
* Data-driven component health, counters and metrics
* Active-alert count and panel
* Recent alert-event history
* Rule list with visible pending state
* Essential create/edit/enable/disable rule form
* Confirmed logical rule deletion with immediate list update

Future improvements:

* Humidity history chart
* Multi-series telemetry chart
* Rich diagnostic labels and units
* Alarm visualization and acknowledgement workflow
* External notification delivery
* Historical analysis page

---

## Design Principles

The frontend has been designed following these principles:

* Component-based architecture
* Separation of concerns
* Strong typing with TypeScript
* Reusable UI components
* Responsive layout
* Clean and maintainable code
* Scalable project structure
