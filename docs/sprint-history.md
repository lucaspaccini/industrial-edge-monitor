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

---

# Sprint 07 — Network Connectivity

## Goal

Implement the networking layer of the firmware, introducing a modular connectivity architecture and establishing the first Wi-Fi connection.

## Completed

- Created the firmware component architecture
- Added the `config` component
- Integrated Kconfig into the project
- Centralized firmware configuration
- Implemented the Wi-Fi component
- Connected the ESP32 to the local Wi-Fi network
- Verified DHCP network configuration
- Validated the firmware architecture on real hardware

## Decisions

- Adopt Kconfig as the official firmware configuration system.
- Keep firmware configuration aligned with the backend configuration model.
- Store Wi-Fi and MQTT parameters through Kconfig instead of hardcoded values.
- Keep each firmware component responsible for its own configuration.

## Takeaway

The firmware now has a modular architecture with a centralized configuration system based on Kconfig. The ESP32 successfully connects to the local Wi-Fi network, validating the embedded architecture before introducing MQTT communication.

---

# Sprint 08 — MQTT Integration

## Goal

Integrate MQTT communication into the firmware and establish the first end-to-end telemetry flow between the ESP32 and the Industrial Edge Monitor backend.

## Completed

- Added the MQTT client component
- Integrated the ESP-MQTT managed component
- Implemented the MQTT connection lifecycle
- Connected the ESP32 to the local MQTT broker
- Implemented telemetry publishing
- Published the first telemetry message from the ESP32
- Successfully received telemetry through the Python collector
- Persisted telemetry into the SQLite database
- Validated the complete end-to-end communication pipeline

## Decisions

- Keep MQTT communication isolated inside a dedicated firmware component.
- Publish telemetry only after a successful MQTT connection.
- Use a static telemetry payload before integrating real sensor data.
- Keep the backend unchanged while replacing the Python simulator with the ESP32.

## Takeaway

The firmware is now fully integrated with the existing backend architecture. The ESP32 successfully publishes telemetry through MQTT, which is collected, processed and stored by the backend, validating the complete communication pipeline from the embedded device to the database.

---

# Sprint 09 — Telemetry Engine

## Goal

Design and implement a modular telemetry engine capable of periodically collecting, assembling and publishing telemetry data, establishing the internal firmware architecture before integrating real hardware sensors.

## Completed

- Replaced the one-shot telemetry publish with a periodic FreeRTOS task
- Implemented the `telemetry` component as the telemetry scheduler
- Introduced the `telemetry_model` module to aggregate telemetry data
- Introduced the `telemetry_json` module for JSON serialization
- Created the `sensor` component with a simulated sensor interface
- Created the `machine_status` component
- Created the `system_time` component
- Decoupled telemetry generation from MQTT communication
- Validated periodic telemetry publishing through the complete backend pipeline

## Decisions

- Keep the telemetry engine independent from the MQTT transport layer.
- Separate telemetry scheduling, data aggregation and JSON serialization into dedicated modules.
- Introduce dedicated provider components (`sensor`, `machine_status`, `system_time`) to isolate data sources from the telemetry model.
- Simulate hardware data until the BME280 driver is integrated.

## Takeaway

The firmware now provides a modular telemetry engine based on clearly separated responsibilities. The telemetry pipeline is independent from the underlying hardware implementation, allowing future sensor integrations without impacting the communication layer or the backend architecture.

# Sprint 10 — Real Sensor Integration

## Goal

Replace the simulated telemetry source with a real hardware sensor by developing and integrating a complete BME280 driver while preserving the modular firmware architecture.

## Completed

- Implemented a complete BME280 device driver
- Added I²C device management through the shared bus component
- Implemented sensor reset, configuration and calibration loading
- Implemented Bosch compensation algorithms for temperature, humidity and pressure
- Replaced the simulated sensor provider with the real BME280
- Extended the sensor abstraction to expose real environmental measurements
- Validated the complete telemetry pipeline from hardware acquisition to MQTT publishing
- Updated the firmware documentation to reflect the current architecture

## Decisions

- Keep the BME280 implementation encapsulated inside its dedicated driver.
- Preserve the `sensor` component as the hardware abstraction layer.
- Maintain a clear separation between hardware drivers, telemetry generation and communication.
- Reuse the shared I²C bus component for device management.

## Takeaway

The firmware now acquires real environmental data through a dedicated hardware driver while preserving a clean and extensible architecture. The complete telemetry pipeline is now fully operational, from physical sensor acquisition to MQTT publication.

## Next Sprint

- Add persistent configuration management
- Introduce secure MQTT communication (TLS)
- Improve telemetry robustness and error handling
- Prepare the firmware for additional sensor integrations

---

# Sprint 11 — Reliable System Time

## Goal

Provide trustworthy UTC timestamps through SNTP without making network time availability a boot dependency.

## Completed

- Replaced uptime-based placeholder timestamps with UTC ISO 8601 timestamps
- Added the SNTP lifecycle to the dedicated `system_time` component
- Added configurable SNTP server and initial synchronization timeout settings through Kconfig
- Prevented telemetry acquisition and publication while the system clock is invalid
- Kept SNTP synchronization active after the initial timeout
- Added automatic telemetry recovery when a later background synchronization succeeds
- Preserved normal boot and MQTT connectivity when the initial SNTP request times out
- Updated the firmware architecture and configuration documentation

## Decisions

- Keep time synchronization, validity state and formatting inside `system_time`.
- Start SNTP only after Wi-Fi connectivity is established.
- Treat the initial synchronization timeout as a recoverable condition rather than a fatal startup error.
- Gate telemetry at the scheduler boundary so invalid samples are neither acquired nor serialized.
- Emit timestamps in UTC using the unambiguous `YYYY-MM-DDTHH:MM:SSZ` representation.

## Takeaway

Telemetry now carries production-compatible timestamps and cannot contaminate the backend with invalid device time. Temporary SNTP unavailability degrades the device safely: connectivity remains active, synchronization continues in the background and publishing resumes automatically when the clock becomes valid.

## Next Sprint

- Add persistent configuration management
- Introduce secure MQTT communication (TLS)
- Prepare the firmware for additional sensor integrations

---

# Sprint 12 — Device Health, Machine Status and Reliable Telemetry

## Goal

Make telemetry fail closed, expose internal device health independently from machine state, and extend the complete platform to multiple device identities.

## Completed

- Changed `telemetry_model_create()` to return `esp_err_t`
- Made model creation transactional so output is written only for complete samples
- Rejected failed sensor reads, non-finite measurements and values outside the BME280 operating ranges
- Rejected samples with invalid UTC timestamps while allowing unavailable machine status as `unknown`
- Removed fallback zero measurements and `unknown` timestamps
- Prevented serialization and publication of rejected samples
- Changed `mqtt_publish_telemetry()` to report invalid arguments, disconnected state and client publish failures
- Added in-memory counters for accepted samples, rejected samples, accepted publishes and failed or skipped publishes
- Added a thread-safe, static `device_health` snapshot with stable component states, error codes, counters and metrics
- Kept infrastructure components independent from diagnostics; application orchestrators translate their results into health updates
- Added configurable disabled/GPIO machine-status providers with independent active-level and pull settings
- Added retained health heartbeats, retained availability and an MQTT Last Will on per-device topics
- Allowed health publication with `timestamp: null` while SNTP is unavailable
- Added stable application `device_id`, distinct from the MQTT client ID
- Added an idempotent SQLite migration assigning historical records to `legacy-device`
- Added validated collector routing with topic/payload identity checks and legacy topic support
- Added current health persistence, effective availability and device-filtered telemetry APIs
- Added a dashboard selector governing telemetry, charts, statistics and health together
- Added backend validation and migration tests plus firmware/frontend production builds
- Separated clock, acquisition, validation, serialization, MQTT disconnection and publication errors in logs
- Removed the circular dependency between telemetry orchestration and MQTT transport
- Preserved automatic recovery on later scheduler cycles after sensor or MQTT faults

## Decisions

- Keep BME280 communication and compensation inside the driver and expose physical measurement limits through the sensor abstraction contract.
- Keep complete-sample validation in `telemetry_model` and write the caller output only after all validation succeeds.
- Change `state_revision` only for component/general/error state transitions; counters and metrics travel on the periodic heartbeat and never trigger a publish loop.
- Treat a disabled machine-status provider as unconfigured (`unknown`), not degraded; only a GPIO provider error degrades health.
- Compute effective availability as retained reported availability `online` plus a fresh `last_seen`; retained `offline` wins immediately.
- Store only the latest health snapshot per device, with extensible JSON sections validated before persistence.
- Count MQTT publication as accepted when ESP-MQTT returns a valid message identifier; broker acknowledgement is not implied.
- Let the periodic scheduler provide bounded retries instead of introducing immediate retry loops.
- Start telemetry from `app_main` so MQTT remains a transport-only component.

## Takeaway

The platform now separates environmental telemetry, external machine state, internal device health and MQTT availability. It remains operational through clock, sensor and broker faults, attributes every record to a device and presents one coherent selected-device view end to end.

## Next Sprint

- Add persistent configuration management
- Introduce secure MQTT communication (TLS)
- Prepare the firmware for additional sensor integrations

---

# Sprint 13 — Configurable Alert Rules and Event Lifecycle

## Goal

Transform valid per-device telemetry into deterministic, persistent alert events through configurable thresholds, dwell time and hysteresis, without adding alert responsibilities to the firmware.

## Completed

- Added strict rule models for temperature and humidity with stable operators and severities
- Added idempotent SQLite tables for rules, persisted runtime state and alert-event history
- Added a partial unique index preventing multiple active events for one rule
- Implemented pure threshold, recovery, hysteresis and extreme-value functions
- Implemented the persisted `normal → pending → active` runtime state machine
- Added active/resolved events with copied rule configuration and machine-readable resolution reasons
- Added duration-zero immediate activation and dwell-time cancellation
- Ignored duplicate, equal-time and out-of-order samples without generating events
- Evaluated only enabled rules belonging to the telemetry device
- Returned the inserted telemetry ID and integrated alert evaluation after telemetry persistence
- Preserved valid telemetry when alert evaluation fails and made evaluation safe to retry
- Added rule CRUD, active-alert and event-history APIs with filters and controlled limits
- Added a device-scoped dashboard panel for active alerts, event history and rule management
- Added logical rule archiving with preserved history, reusable names and `rule_archived` resolution
- Added structured, transition-only alert evaluation logs
- Added isolated validation, state-machine, reliability, collector, migration, service and API tests

## Decisions

- Keep the alert engine entirely in the backend and independent from MQTT, FastAPI and React.
- Use validated telemetry time as event time; configuration updates use timezone-aware backend UTC time.
- Ignore timestamps older than or equal to the persisted last evaluation time.
- Keep rule state and event mutation in one SQLite transaction.
- Commit telemetry before alert evaluation so an engine failure cannot discard a valid sample; defer outbox-based guaranteed delivery.
- Reset runtime state on substantial rule updates and retain event history with `rule_updated` or `rule_disabled`.
- Store no persistent `resolved` runtime state because resolution belongs to event history.

## Takeaway

Industrial Edge Monitor now turns telemetry into durable, actionable state without coupling alert logic to the edge device. Short violations are filtered, sustained conditions open one event, hysteresis stabilizes recovery, and runtime state survives backend restarts while remaining isolated per device.

## Next Sprint

- Add alert acknowledgements and operator workflow
- Add external notification delivery only when a concrete channel is selected
- Continue deployment and security hardening
