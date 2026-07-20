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
- [ ] Persistent configuration management
- [ ] Secure MQTT communication (TLS)
- [ ] Telemetry buffering during connectivity outages

---

# Phase 4 — Industrial Dashboard

Goal: create a production-like monitoring interface.

- [ ] Dashboard layout redesign
- [ ] History page
- [ ] Multiple charts
- [ ] Device status
- [ ] System health dashboard
- [ ] Industrial KPI cards
- [ ] Responsive layout improvements

---

# Phase 5 — Engineering Excellence

Goal: improve reliability, maintainability and deployment.

- [ ] Unit Tests
- [ ] Integration Tests
- [ ] Docker
- [ ] Docker Compose
- [ ] CI/CD
- [ ] Code Coverage
- [ ] Pre-commit hooks
- [ ] Linting
- [ ] Type Checking
- [ ] Raspberry Pi deployment
- [ ] Production configuration

---

# Phase 6 — Release

Goal: prepare the project for public release.

- [ ] Complete documentation
- [ ] Architecture diagrams
- [ ] Release v1.0.0
