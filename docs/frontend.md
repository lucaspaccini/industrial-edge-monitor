
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

Install exactly the dependency graph recorded in `package-lock.json`, then copy the local environment template:

```bash
cd frontend
npm ci
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

`next.config.ts` enables Next.js standalone output. The dashboard does not import or render `next/image`; image optimization is also disabled globally, so direct `/_next/image` requests return `404` before Next.js invokes `sharp`. This is a defense-in-depth control while the vulnerable transitive package remains in the standalone trace, not a claim that the dependency advisory is resolved. The multi-stage `frontend/Dockerfile` installs dependencies with `npm ci`, builds the application with an explicit public API URL, and copies only the generated standalone server, static assets and public files into the runtime stage. The final container runs `node server.js` as a non-root user on `0.0.0.0:3000`; it does not contain or run the development server.

Build and run it independently with:

```bash
cd frontend
docker build \
  --build-arg NEXT_PUBLIC_API_URL=http://127.0.0.1:8000 \
  -t industrial-edge-monitor-frontend .
docker run --rm -p 3000:3000 industrial-edge-monitor-frontend
```

The repository-level Compose workflow manages this image together with Mosquitto, the collector and the API. The production image is intended for the documented local/single-host deployment; exposing a self-hosted Next.js server directly to the public Internet would additionally require a hardened reverse proxy and controls outside the current sprint.

## Residual dependency advisories

`npm audit --omit=dev` remains non-zero. npm reports three affected package entries: the direct `next` entry aggregates findings from its transitive `postcss` and optional `sharp` dependencies. The PostCSS findings `GHSA-qx2v-qp2m-jg93`, `GHSA-6g55-p6wh-862q` and `GHSA-r28c-9q8g-f849` affect the build dependency pinned by Next.js. PostCSS is absent from the final standalone runtime image, and the build processes only repository-controlled CSS.

The sharp finding `GHSA-f88m-g3jw-g9cj` remains present in the standalone image because Next.js dependency tracing includes its image optimizer. This dashboard does not use `next/image`, accept image uploads or configure external image sources. In addition, `images.unoptimized` makes `/_next/image` return `404` before sharp is invoked. The setting limits reachability but does not remove the vulnerable package or resolve the advisory.

At Sprint 14 closure no stable Next.js release provides compatible patched transitive versions. No downgrade, forced audit fix or unsupported dependency override is applied. The residual risk is accepted only for the documented trusted, non-public single-host deployment and must be reassessed when an upstream-compatible release becomes available.

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
