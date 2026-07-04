# Sprint History

## Sprint 01 — Foundation

### Goal

Build the first end-to-end version of the platform.

### Completed

- MQTT simulator
- MQTT collector
- SQLite persistence
- FastAPI REST API
- Next.js dashboard
- Real-time telemetry visualization

---

## Sprint 02 — Backend Architecture

### Goal

Introduce a scalable backend architecture.

### Completed

- Service Layer
- Repository Layer
- Health endpoint
- Centralized logging
- Initial testing infrastructure

---

## Sprint 03 — Shared Backend Architecture

### Goal

Share the application architecture across backend components.

### Completed

- Shared `core` package
- Shared configuration
- Shared logging
- Shared services
- Shared repositories
- Refactored MQTT collector
- Centralized database initialization
- Centralized MQTT configuration

---

## Sprint 04 — Service Layer Consolidation

### Goal

Consolidate the service layer and prepare the backend for future dependency injection.

### Completed

- Introduced service objects (`TelemetryService` and `HealthService`)
- Added constructor-based dependency injection
- Replaced service functions with service instances
- Updated API routes to use service objects
- Updated MQTT collector to use `TelemetryService`
- Unified the interaction between API and Collector through the same service layer

### Takeaway

The service layer is now shared by all backend entry points (REST API and MQTT Collector).

The architecture is prepared for introducing FastAPI Dependency Injection in a future sprint, when multiple services will justify the additional complexity.

---

## Sprint 05 — Telemetry Domain

### Goal

Introduce domain-oriented telemetry features and expose aggregated statistics.

### Completed

- Designed telemetry statistics endpoint
- Added telemetry statistics schema
- Implemented statistics query in the repository
- Added statistics business logic to `TelemetryService`
- Exposed `GET /telemetry/statistics`
- Added frontend service and hook
- Implemented real-time telemetry statistics card
- Extended the dashboard with domain KPIs

### Takeaway

The backend now provides domain-level information instead of only exposing raw telemetry samples. The dashboard evolved from a telemetry viewer into an industrial monitoring dashboard with aggregated KPIs.

---

# Sprint 06 — Firmware Development Environment

## Goal

Prepare a reproducible ESP-IDF development environment and establish the foundation for firmware development on the ESP32.

## Completed

- Created the `firmware/` project structure
- Defined the firmware modular architecture
- Introduced the `components/` directory following ESP-IDF best practices
- Added `firmware/README.md`
- Added `firmware/toolchain.yml`
- Added `docs/firmware-setup.md`
- Added `firmware/.gitignore`
- Added `sdkconfig.defaults`
- Installed ESP-IDF v6.0.2
- Installed the ESP32 toolchain
- Verified the ESP-IDF installation (`idf.py --version`)
- Configured serial port permissions (`dialout` group)
- Created the first ESP-IDF project
- Configured the project for the ESP32 target
- Successfully built the firmware
- Successfully flashed the firmware to the ESP32
- Verified firmware execution through the serial monitor
- Executed the first firmware application on real hardware

## Decisions

- Adopt the latest ESP-IDF stable release for the entire project lifecycle.
- Keep the ESP-IDF toolchain outside the repository.
- Document the development environment to ensure reproducibility.
- Use a component-based firmware architecture (`components/`) instead of a monolithic `main/` implementation.
- Follow the official ESP-IDF project structure whenever possible.
- Validate every development milestone before introducing new functionality.

## Takeaway

A complete and reproducible ESP-IDF development environment has been established. The firmware architecture has been defined, the project successfully builds, flashes and runs on the ESP32, providing a solid foundation for future firmware development.

## Next Sprint

- Initialize the Wi-Fi module
- Connect the ESP32 to the local network
- Implement the MQTT client
- Publish a first telemetry message
- Verify end-to-end communication with the MQTT broker