# Industrial Edge Monitor frontend

Next.js 16 dashboard for device-scoped telemetry, statistics and current device health.

## Run locally

```bash
cp .env.example .env.local
npm ci
npm run dev
```

`NEXT_PUBLIC_API_URL` must reference the FastAPI service, normally `http://127.0.0.1:8000`. Open `http://localhost:3000`.

Because the dashboard calls FastAPI from the browser, this URL must be reachable by the browser. A Compose service name such as `http://api:8000` works only between containers and is not a valid browser URL.

The device selector is the single dashboard scope: telemetry cards, chart, statistics and health always use the same `device_id`. Components, counters and metrics in the health panel are rendered dynamically so additional diagnostic keys remain visible without mandatory UI changes.

## Checks

```bash
npm test
npm run lint
npm run build
```

`npm run build` requires `NEXT_PUBLIC_API_URL` in the environment. `NEXT_PUBLIC_` values are embedded into the client bundle by Next.js at build time; changing the variable only when starting an existing build has no effect.

## Production container

The production image uses Next.js standalone output and runs the generated minimal Node.js server as a non-root user:

```bash
docker build \
  --build-arg NEXT_PUBLIC_API_URL=http://127.0.0.1:8000 \
  -t industrial-edge-monitor-frontend .
docker run --rm -p 3000:3000 industrial-edge-monitor-frontend
```

Rebuild the image when the public API URL changes. The repository-level Compose workflow supplies this build argument automatically; see the project [setup guide](../docs/setup.md).

The frontend uses the App Router, TypeScript, Tailwind CSS, shadcn/ui and Recharts. Broader project setup and architecture are documented in the repository [README](../README.md) and [frontend documentation](../docs/frontend.md).
