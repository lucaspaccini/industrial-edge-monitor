# Architecture

```text
Simulator
    │
    ▼
 MQTT Broker
    │
    ▼
Collector
    │
    ▼
SQLite
    │
    ▼
FastAPI
    │
    ▼
Next.js Dashboard
```

## Components

### Simulator

Publishes MQTT telemetry messages.

### Collector

Subscribes to MQTT topics and stores telemetry into SQLite.

### Database

Stores telemetry history.

### API

Exposes telemetry through REST endpoints.

### Frontend

Displays telemetry and charts.