# Industrial Edge Monitor frontend

Next.js 16 dashboard for device-scoped telemetry, statistics and current device health.

## Run locally

```bash
cp .env.example .env.local
npm install
npm run dev
```

`NEXT_PUBLIC_API_URL` must reference the FastAPI service, normally `http://127.0.0.1:8000`. Open `http://localhost:3000`.

The device selector is the single dashboard scope: telemetry cards, chart, statistics and health always use the same `device_id`. Components, counters and metrics in the health panel are rendered dynamically so additional diagnostic keys remain visible without mandatory UI changes.

## Checks

```bash
npm run lint
npm run build
```

The frontend uses the App Router, TypeScript, Tailwind CSS, shadcn/ui and Recharts. Broader project setup and architecture are documented in the repository [README](../README.md) and [frontend documentation](../docs/frontend.md).
