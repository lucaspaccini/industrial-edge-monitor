
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
│   ├── common/
│   ├── dashboard/
│   ├── layout/
│   └── ui/
├── hooks/
├── lib/
├── services/
├── styles/
├── types/
└── utils/
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

Future improvements:

* Humidity history chart
* Multi-series telemetry chart
* Rich diagnostic labels and units
* Alarm visualization and acknowledgement workflow
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
