# Roadmap

This roadmap describes the planned evolution of the Industrial Edge Monitor project.

---

# Phase 1 — Foundation ✅

## Backend

- [x] MQTT simulator
- [x] MQTT collector
- [x] SQLite persistence
- [x] FastAPI REST API
- [x] Configuration management

## Frontend

- [x] Next.js dashboard
- [x] Telemetry cards
- [x] Temperature history chart
- [x] Real-time polling

---

# Phase 2 — Software Architecture ✅

Goal: build a scalable and maintainable architecture.

## Backend

- [x] Service Layer
- [x] Repository Layer
- [x] Shared Core
- [x] Shared Services
- [x] Shared Repositories
- [x] Health endpoint
- [x] Centralized logging
- [x] Configuration management
- [x] Database initialization
- [x] Initial testing infrastructure
- [x] Service Objects
- [x] Telemetry statistics service
- [x] Telemetry statistics API
- [x] Dashboard statistics integration

### Future Improvements

- [ ] FastAPI Dependency Injection
- [ ] Repository abstraction
- [ ] API versioning

---

# Phase 3 — Embedded Integration 🚧

Goal: replace the simulator with a modular embedded firmware running on the ESP32.

## Foundation

- [x] ESP32-WROOM-32U setup
- [x] ESP-IDF development environment
- [x] Firmware project initialization
- [x] Firmware build
- [x] Firmware flashing
- [x] Serial monitor verification

## Connectivity

- [x] Firmware component architecture
- [x] Kconfig integration
- [x] Wi-Fi connection
- [x] MQTT connectivity

## Telemetry Engine

- [x] Periodic telemetry task
- [x] Telemetry data model
- [x] JSON serialization
- [x] Sensor abstraction layer
- [x] Machine status provider
- [x] System time provider
- [x] First telemetry published from ESP32
- [x] SNTP clock synchronization
- [x] UTC ISO 8601 timestamps
- [x] Telemetry publishing gated on valid system time

## Sensors

- [x] BME280 driver
- [x] Sensor abstraction layer
- [x] Replace simulated sensor values with real measurements
- [x] Real-time telemetry acquisition
- [ ] Support multiple sensor providers
- [ ] Sensor health monitoring
- [ ] Sensor self-test during startup

## Deployment

- [ ] Replace Python simulator with ESP32
- [ ] OTA updates (optional)

## Reliability and Security

- [x] Non-blocking recovery after initial SNTP timeout
- [x] End-to-end telemetry error propagation
- [x] Sensor measurement validation
- [x] Invalid and incomplete sample rejection
- [x] In-memory telemetry diagnostics
- [x] Automatic recovery after temporary sensor and MQTT failures
- [x] Device health snapshots with stable diagnostic codes
- [x] MQTT availability with retained online state and Last Will
- [x] Per-device identity, topics and telemetry persistence
- [x] Configurable disabled/GPIO machine status provider
- [x] Configurable per-device threshold alert rules
- [x] Persistent pending/active rule state
- [x] Persistent active/resolved alert lifecycle
- [x] Dwell time, hysteresis and out-of-order sample protection
- [ ] Persistent configuration management
- [ ] Secure MQTT communication (TLS)
- [ ] Telemetry buffering during connectivity outages

---

# Phase 4 — Industrial Dashboard 🚧

Goal: create a production-like monitoring interface.

- [ ] Dashboard layout redesign
- [ ] History page
- [ ] Multiple charts
- [x] Shared device selector for telemetry, statistics and health
- [x] Current device health and effective availability
- [x] Device-scoped alert rules, active alerts and event history
- [x] Essential alert rule editor
- [ ] Rich diagnostic labels, units and visual prioritization
- [ ] Industrial KPI cards
- [ ] Responsive layout improvements

---

# Phase 5 — Engineering Excellence 🚧

Goal: improve reliability, maintainability and deployment.

- [x] Unit tests
- [x] Integration tests
- [x] Production-like Docker images
- [x] Reproducible Docker Compose stack
- [x] Continuous integration for backend, frontend, firmware and containers
- [ ] Continuous deployment
- [ ] Code Coverage
- [ ] Pre-commit hooks
- [x] Frontend linting
- [ ] Type Checking
- [ ] Raspberry Pi deployment
- [x] Validated environment-specific backend configuration
- [ ] Production security configuration (TLS, authentication and hardened ingress)
- [ ] Automated backup and restore for persistent data
- [ ] Multi-host storage and database architecture

---

# Phase 6 — Release

Goal: prepare the project for public release.

- [ ] Complete documentation
- [ ] Architecture diagrams
- [ ] Release v1.0.0
