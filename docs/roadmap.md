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

# Phase 2 — Software Architecture 🚧

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

### Future Improvements

- [ ] FastAPI Dependency Injection
- [ ] Repository abstraction
- [ ] API versioning

---

# Phase 3 — Software Quality

Goal: improve reliability and maintainability.

- [ ] Unit Tests
- [ ] Integration Tests
- [ ] Docker
- [ ] CI/CD
- [ ] Code Coverage
- [ ] Pre-commit hooks
- [ ] Linting
- [ ] Type Checking

---

# Phase 4 — Industrial Dashboard

Goal: create a production-like monitoring interface.

- [ ] History page
- [ ] Statistics page
- [ ] Multiple charts
- [ ] Device status
- [ ] System health dashboard
- [ ] Responsive layout improvements

---

# Phase 5 — Embedded Integration

Goal: replace the simulator with real hardware.

## Hardware

- [ ] ESP32-WROOM-32U
- [ ] BME280
- [ ] Wi-Fi provisioning
- [ ] MQTT publishing
- [ ] OTA updates (optional)

---

# Phase 6 — Production Ready

Goal: prepare the project for deployment.

- [ ] Docker Compose
- [ ] Raspberry Pi deployment
- [ ] Production configuration
- [ ] Complete documentation
- [ ] Architecture diagrams
- [ ] Release v1.0.0