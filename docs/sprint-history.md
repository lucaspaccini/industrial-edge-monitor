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

---

# Sprint 14 — Reproducible Deployment, Environment Configuration and Continuous Integration

## Goal

Make the complete single-host platform reproducibly buildable, configurable and verifiable from a clean checkout, without presenting the trusted-LAN deployment as production-secure.

## Completed

- Added a single-stage, non-root Python backend image reused by API and collector, plus a multi-stage, non-root Next.js standalone image
- Added a four-service Compose stack for Mosquitto, collector, API and frontend with health-aware startup ordering
- Shared one named SQLite volume between API and collector and kept retained Mosquitto data in a separate named volume
- Added strict environment validation and documented the build-time nature of `NEXT_PUBLIC_API_URL`
- Added graceful collector shutdown, SQLite WAL mode, a bounded busy timeout and automatic database-directory creation
- Added a continuous integration workflow for backend, frontend, firmware and container verification. Equivalent checks passed locally; the GitHub Actions run after push is the final repository gate
- Added an MQTT-to-SQLite-to-API container smoke test and exact-record persistence verification across API and collector recreation
- At Sprint 14 closure, kept the then-current `1883:1883` plaintext mapping; Sprint 15 subsequently replaced it with authenticated TLS on `8883:8883`
- Updated Next.js to the latest stable compatible patch and separated build-only shadcn tooling from runtime dependencies
- Inspected the final frontend image and documented the residual PostCSS and sharp advisories without hiding or force-fixing them

## Decisions

- Continuous integration workflow implementation is complete and the Sprint 14 baseline passed on GitHub-hosted runners; the workflow performs no deployment, and continuous delivery remains future work.
- Environment configuration is not persistent device configuration.
- `APP_ENV=production` selects stricter validation and runtime defaults; it is not evidence of production security.
- At Sprint 14 closure, TLS and authentication were out of scope. Sprint 15 subsequently secured MQTT; hardened HTTP ingress, backup automation and multi-host storage remain future work.
- `images.unoptimized` is retained because it makes `/_next/image` return `404` before the sharp-backed optimizer is invoked; it limits reachability but does not resolve or remove the sharp advisory.
- No downgrade, forced audit fix, preview release or unsupported transitive override is used for residual npm findings.

## Verification

The following equivalent workflow checks were completed locally. They do not claim that a GitHub Actions run has already completed.

- Backend: `pytest -q` — 88 passed
- Frontend: `npm test` — 1 passed; lint and production build passed with Next.js 16.2.12
- Dependency audit: `npm audit --omit=dev` — three high-severity package entries remain and are classified below
- Firmware: ESP-IDF 6.0.2 build passed; application binary uses 89% of its partition
- Containers: backend and frontend image builds passed; Mosquitto, API and frontend became healthy and the collector subscribed successfully
- End to end: a legacy MQTT sample was persisted, returned by the API and retained with the same ID after API and collector recreation
- Storage: API and collector mounted the same isolated SQLite volume read/write; smoke containers, network, volumes and temporary environment file were removed afterward
- Configuration and workspace: `docker compose config --quiet` and `git diff --check` passed

## Residual Risk

- npm reports `next` as an aggregate affected node because Next.js 16.2.12 pins PostCSS 8.4.31 and allows sharp 0.34.x.
- The three PostCSS advisories require processing attacker-controlled CSS. The project builds only repository-controlled CSS, and PostCSS is absent from the final standalone runtime image. They are classified as build-time only and not applicable to the current runtime.
- Sharp 0.34.5 and libvips 8.17.3 remain in the standalone image through Next.js dependency tracing. The application does not use `next/image`, accept image uploads or configure external image sources, and its image optimizer endpoint is disabled. This remains tracked, temporarily accepted runtime risk for the non-public single-host scope, not a resolved finding.
- No current stable Next.js version admits both fixed PostCSS and sharp versions without an unsupported override; reassess on the next compatible stable release.

## Takeaway

The repository now has a reproducible, CI-enabled and locally verified single-host deployment baseline. It is not yet a production-secure or multi-host deployment.

## Future Work

- API/dashboard authentication (MQTT TLS and broker authentication were completed in Sprint 15)
- Hardened ingress
- Automated backup and restore
- Multi-host storage and database design
- Persistent device configuration
- Continuous delivery

---

# Sprint 15 — Secure MQTT Communication and Device Authentication

## Goal

Protect the complete MQTT path with verified TLS, per-client authentication and least-privilege broker authorization while preserving all device topics, payloads, backend APIs, dashboard behavior and recovery semantics.

## Completed

- Replaced the standard plaintext listener with authenticated MQTT over TLS on `8883:8883`, requiring TLS 1.2 or newer and disabling anonymous access
- Added a reproducible local generator for a constrained development CA, SAN server certificate, random per-client credentials, hashed password material and Mosquitto authorization state
- Added default-deny roles for devices, collector, legacy simulator/test and broker health check
- Restricted each device username to its own telemetry, health and availability topics through scalable `%u` rules
- Prevented device subscriptions, cross-device publishing and collector publishing while authorizing only the collector's required subscriptions
- Preserved topic/payload identity validation in the collector as a second layer beyond broker authorization
- Added validated Python MQTT settings, file-backed secrets, TLS hostname verification, Callback API v2, bounded reconnect and categorized failure logs
- Migrated collector and simulator to the shared secure Paho client factory without adding transport responsibilities to application services
- Added ESP-MQTT TLS/CA verification and username/password authentication using ESP-IDF 6.0.2, with secure-URI and embedded-CA validation and no plaintext fallback
- Preserved telemetry, retained health/availability, Last Will, reconnect and publication semantics without changing payloads or APIs
- Mounted only the runtime material needed by each Compose service; excluded `.local/` secrets from Git and Docker build contexts
- Limited `--device` to initial full-bundle identities and rejected reserved service names, invalid IDs and duplicates
- Hardened `--force` with managed-bundle markers, symlink/broad-path rejection, same-parent staging, controlled backup/rename promotion and rollback-safe failure behavior
- Verified Mosquitto 2.1.2 hash behavior and restricted policy input to supported `$7$` and Argon2id encodings
- Updated the firmware CI job to generate a temporary CA and compile the `MQTT_BROKER_CA_EMBEDDED=1` branch
- Added isolated configuration/client/policy tests and a CI-ready security smoke script generating and deleting only temporary secrets/resources
- Hardened concurrent in-process database bootstrap after the full suite exposed a transient first-WAL lock; schema and normal SQLite access remain unchanged

## Decisions

- Use a locally generated CA and server-only TLS authentication for this sprint; clients authenticate with unique username/password pairs inside the verified TLS channel rather than mTLS.
- Keep `device_id`, MQTT client ID and username as distinct domains. Device usernames equal `device_id` only to support broker `%u` authorization; client IDs remain session identifiers.
- Use Mosquitto Dynamic Security generated from a versioned ACL policy and hashed password source. Its `subscribeLiteral` rules can reject unauthorized wildcard subscription requests, which static delivery ACL filtering cannot guarantee.
- Keep the API process MQTT-disabled so it receives no MQTT credentials. Collector, simulator, devices and health check each receive only their own trust/credential material.
- Fail immediately on missing/incoherent production TLS/authentication configuration and never downgrade to plaintext or disable hostname verification.
- Treat local build-time ESP32 credentials as an explicit interim limitation, not persistent device provisioning.
- Defer post-generation device creation, revocation, rotation and synchronization to “Persistent Device Configuration, Provisioning and Credential Lifecycle”; complete-bundle `--force` replacement is not a per-device lifecycle.
- Keep the secure Compose deployment single-host and non-public until API authentication, HTTPS ingress and production secret management exist.

## Verification

All automated checks passed locally and the pushed Sprint 15 revision passed its independent GitHub Actions run; the manual ESP32 hardware test also passed.

- Backend: `.venv/bin/python -m pytest -q` — 123 passed, including generator path/marker/rollback and password-hash hardening
- Frontend: `npm test` — 1 passed; `npm run lint` and Next.js 16.2.12 production build passed
- Firmware: ESP-IDF v6.0.2 build passed for ESP32 with a temporary CA and `MQTT_BROKER_CA_EMBEDDED=1`; application binary has 11% of its partition free
- Hardware TLS: a real ESP32 connected to Mosquitto with `mqtts` on port `8883`, negotiated TLS 1.2 with `ECDHE-RSA-AES256-GCM-SHA384`, verified the broker CA and SAN, authenticated with its dedicated device identity and published on its authorized device topics
- Hardware end to end: real BME280 telemetry reached the collector, SQLite, API and dashboard; health and retained availability were verified, the offline Last Will appeared after keepalive-based loss detection, online state returned after power restoration, and the ESP32 reconnected automatically after a broker restart
- Containers: backend/API/collector and frontend image builds passed; `docker compose config --quiet` passed
- MQTT security smoke: stack health, collector subscription, anonymous/bad-password rejection, untrusted-CA and hostname rejection, cross-device/subscribe/collector-write ACL denial, valid TLS ingestion, retained health/availability, Last Will, legacy identity and SQLite persistence all passed
- Cleanup: the isolated smoke containers, network, volumes and temporary security material were removed by the project-scoped trap
- Workspace: shell syntax, YAML parsing, policy generation and secret-marker log checks passed; final diff/status checks are recorded at handoff

## Takeaway

Industrial Edge Monitor now has a secure-by-default MQTT path for its documented local single-host deployment. Broker identity is verified, every client is authenticated and topic access is constrained without weakening the application's existing device identity checks.

## Future Work

- Persistent Device Configuration, Provisioning and Credential Lifecycle, including per-device addition, rotation, revocation and synchronization
- Secure device credential storage instead of build-time `sdkconfig`
- Production secret distribution and optional mTLS assessment
- API/dashboard authentication and hardened HTTPS ingress
- Automated backup and restore
- Multi-host storage and deployment architecture
- Continuous delivery

---

# Sprint 16 — Runtime and Dependency Baseline Upgrade

## Goal

Move the frontend to one controlled and reproducible Node.js baseline, refresh compatible direct and transitive dependencies, and close the previously tracked Next.js/PostCSS/sharp findings before persistent device configuration and credential lifecycle work begins.

## Completed

- Adopted Node.js 24.19.0 for local setup, GitHub Actions and all three frontend container stages
- Added a repository-root `.nvmrc` pinned to `24.19.0` and a frontend engine range of `>=24.19.0 <25`
- Regenerated `package-lock.json` only with npm 11.17.0 under Node.js 24.19.0, with dependency lifecycle scripts disabled
- Upgraded the direct runtime set: Next.js 16.2.12 to 16.3.0, React/React DOM 19.2.4 to 19.2.8, Lucide React 1.21.0 to 1.31.0, Radix UI 1.6.0 to 1.6.7 and Recharts 3.9.0 to 3.10.1
- Upgraded the compatible build-time set: eslint-config-next 16.2.12 to 16.3.0, ESLint 9.39.4 to 9.39.5, Tailwind CSS and its PostCSS plugin 4.3.1 to 4.3.3, shadcn 4.12.0 to 4.17.0, Node types 20.19.43 to 24.13.3, React types 19.2.17 to 19.2.18 and React DOM types 19.2.3 to 19.2.4
- Updated supported transitive packages within their published dependency ranges; no override or forced audit remediation was added
- Preserved Next.js standalone output, non-root execution and `images.unoptimized`
- Configured the GitHub Actions frontend job for Node.js 24.19.0 and clean installation with lifecycle scripts disabled; the workflow is configured and locally verified, not yet externally verified for this unpushed revision
- Updated local Node, npm, Docker, CI and frontend dependency documentation

The advisory-relevant dependency inventory changed as follows:

| Package and chain | Before | After | Phase and decision |
| --- | --- | --- | --- |
| direct `next` | 16.2.12 | 16.3.0 stable | Build and runtime framework; compatible minor upgrade selected after official engine/peer metadata and production verification |
| `next -> postcss` | 8.4.31 | 8.5.23 | Build-time only and absent from the standalone image; four PostCSS advisories resolved |
| `@tailwindcss/postcss` / `shadcn -> postcss` | 8.5.15 | 8.5.26 | Build-time only; patched through supported direct and transitive ranges |
| `next -> sharp` | 0.34.5 | 0.35.3 | Optional dependency retained in the runtime trace; libvips advisory resolved and optimizer endpoint remains disabled |
| `postcss -> nanoid` | 3.3.15 | 3.3.18 | Production-tree transitive finding resolved |
| `shadcn -> @modelcontextprotocol/sdk -> @hono/node-server / hono` | 1.19.14 / 4.12.27 | 2.1.0 / 4.13.2 | Development CLI only; findings resolved within supported ranges |
| `shadcn -> ajv / express-rate-limit / dotenvx` transitives | fast-uri 3.1.2, ip-address 10.2.0, undici 7.28.0 | 3.1.5, 10.5.0, 7.29.0 | Development CLI only; findings resolved within supported ranges |
| ESLint/shadcn parsing and glob transitives | brace-expansion 1.1.15/5.0.6, js-yaml 4.3.0 | 1.1.18/5.0.9, 4.3.1 | Lint/build tooling only; findings resolved within supported ranges |

## Decisions

- Validate the baseline against the official [Node.js 24.19.0 LTS release](https://nodejs.org/en/blog/release/v24.19.0), [Next.js 16.3 release notes](https://nextjs.org/blog/next-16-3) and published npm engine/peer metadata before selecting versions.
- Use `>=24.19.0 <25` rather than an exact engine value: `.nvmrc`, Docker and CI pin reproducible 24.19.0 execution, while the package contract does not reject later compatible Node 24 patches.
- Use stable Next.js 16.3.0 because its official package metadata supports Node.js 24 and React 19, and it replaces the affected PostCSS/sharp ranges. Preview and canary releases were excluded.
- Keep React and React DOM on matched exact versions and keep eslint-config-next exactly aligned with Next.js.
- Keep ESLint on major 9 and TypeScript on major 5. ESLint 10 and TypeScript 7 are unrelated major migrations and are not required for Node.js 24 or Next.js 16.3 compatibility.
- Align `@types/node` with the Node 24 runtime despite that direct major type-package change; compilation and the complete production build passed without application changes.
- Disable dependency lifecycle scripts for lock generation, clean installation, Docker and CI. Next.js builds successfully from its packaged SWC/sharp binaries without approving install scripts.
- Retain `images.unoptimized`: sharp is patched, but the dashboard still has no image-optimization use case and the setting preserves the previously verified reduced attack surface.
- Make no firmware, Python backend, MQTT contract, database, API, dashboard behavior, TLS/ACL or product-function changes.

## Verification

- Runtime: `node --version` under the pinned local toolchain returned `v24.19.0`; npm returned `11.17.0`
- Clean dependency install: `npm ci --ignore-scripts` installed the committed lockfile successfully
- Frontend: `npm test` passed 1 test; `npm run lint` passed; `tsc --noEmit` passed
- Production: the default `npm run build` completed inside the clean Node.js 24.19.0 Docker build, including Turbopack compilation, TypeScript and static generation with Next.js 16.3.0
- Audit before: full tree reported 12 affected package entries (3 moderate, 9 high); production tree reported 4 high entries through Next.js/PostCSS/sharp/nanoid
- Audit after: `npm audit` and `npm audit --omit=dev` both reported 0 vulnerabilities
- Standalone: the production container returned HTTP 200, reported Node.js `v24.19.0`, contained sharp 0.35.3, did not contain PostCSS and returned 404 from `/_next/image`
- Containers: the dedicated frontend image build and `docker compose build --pull` passed; `docker compose config --quiet` passed
- End to end: the isolated secure Compose smoke passed broker TLS/authentication/ACL rejection cases, collector subscription, device and legacy ingestion, retained health/availability, Last Will, shared SQLite storage, API/frontend HTTP checks and persistence across API/collector recreation
- Cleanup: isolated containers, network, volumes and temporary credentials were removed; the pre-existing local stack was restarted with its original containers and volumes and returned healthy
- Repository hygiene: tracked-file checks found no `.env`, credential material, `.local`, local `sdkconfig`, database, `node_modules` or build artifacts; Docker ignore rules exclude the same relevant local inputs; `git diff --check` passed

## Takeaway

The frontend now has one locally reproducible, CI-configured and container-verified Node.js 24.19.0 baseline. Stable supported dependency updates close the previously accepted production advisory gap without changing product behavior or relying on forced remediation.

## Next Sprint

Persistent Device Configuration, Provisioning and Credential Lifecycle

---

# Sprint 17 — Persistent Device Configuration, Local Web Provisioning and Credential Lifecycle

## Completed

Sprint 17 is complete. Implementation and deterministic local verification passed, the operator-provided main ESP32 hardware checklist passed, and the operator reports that the published Sprint 17 commit completed GitHub Actions without errors.

## Goal

Build one device-independent ESP-IDF image that stores validated, versioned runtime configuration in NVS; provides protected local SoftAP provisioning and bounded APSTA maintenance; exposes redacted status and bounded live diagnostics; and supports transactional per-device MQTT add, rotation and revocation without changing telemetry, health, availability, topic, backend API, database or dashboard contracts.

## Completed Work

- Added a typed `device_config` owner for NVS active, candidate, rollback metadata and unique setup secret; Wi-Fi, MQTT, telemetry and machine status no longer read device credentials from Kconfig.
- Added explicit provisioning states and boot orchestration. Unconfigured devices start only WPA2 SoftAP/local HTTP, while configured maintenance uses APSTA and closes by changing to STA without stopping the station.
- Added complete-candidate staging, bounded boot attempts and activation only after station IP, valid SNTP time and authenticated MQTT TLS; failures retain the previous active configuration and record a redacted rollback reason.
- Added an authenticated SoftAP-only HTTP API with expiring HttpOnly/SameSite session cookie, CSRF, login lockout, body limits, strong factory-reset confirmation and redacted responses.
- Added a bounded thread-safe diagnostic sink that preserves serial logs, copies at most eight records per request, reports cursor loss/overwrites and exposes one authenticated asynchronous SSE worker with generation-matched shutdown waits and UI filters.
- Removed large runtime models from task stacks: candidate activation and HTTP parsing use checked heap allocations with explicit secret clearing; target sizes and task stack high-water marks are observable.
- Added explicit serial full-NVS recovery without selecting an unverified GPIO; NVS initialization failures now remain fail-closed and never trigger an automatic erase.
- Added a stable versioned NVS storage envelope and fixed-width serializer for configuration and metadata instead of persisting ABI-dependent runtime structs.
- Added documented hardware entropy around first-boot setup-secret generation and synchronization for provisioning/session/SSE state and MQTT error snapshots.
- Added transactional MQTT device administration and mode-`0600` versioned packages without Wi-Fi credentials, password/hash output from list/inspect or `.env` mutation.
- Replaced the former 2 MiB/default partition baseline with a versioned 4 MiB, no-OTA layout: 192 KiB NVS, 3 MiB factory app and 768 KiB deliberate unallocated reserve.
- Added deterministic host tests for state/concurrency, diagnostic batching/loss, stable storage and interrupted commits, entropy ordering, asynchronous-source guards and lifecycle tooling, plus a CI layout/application-capacity gate.
- Replaced cross-task C11 busy-wait locks with static FreeRTOS mutexes, preserving blocking mutual exclusion in the host harness through pthread-backed semaphore stubs.
- Hardened setup-secret boot recovery, revision-bound candidate activation, exact-worker SSE completion, session-bound EventSource reconnection, provisioning secret-buffer cleanup and dual-stack SoftAP local-endpoint classification after the first phone test reproduced an IPv4-mapped IPv6 rejection.
- Guarded station RSSI collection so disconnected and unprovisioned states retain JSON `null` without calling an invalid ESP-IDF station API or creating a fault.
- Cleared prior configuration text, file selection and masked password inputs on unprovisioned state, session replacement, logout, factory reset and relevant page failures.
- Split the MQTT security model from a code-derived operations runbook covering trust material, exact file ownership/mounts, identity lifecycle, safe rotation, certificate checks, recovery and smoke testing.

## Decisions

- Use HTTP only inside the unique WPA2 SoftAP for this controlled local workflow; it is authenticated but not application-layer encrypted or production-grade.
- Keep the web service unavailable from the station side and stop it after maintenance; MQTT lifecycle remains coupled only to station/IP state.
- Store complete versioned, explicitly serialized blobs instead of partial keys or raw runtime structs so candidates cannot combine fields from different revisions and future readers can inspect the format before decoding. Retain public CA and credentials in ordinary NVS for now; physical extraction resistance is explicitly deferred with Secure Boot, flash encryption and NVS encryption.
- Require an explicit port in `mqtts://host:port`, username equality with `device_id`, a distinct client ID and a parseable public CA before a candidate can be committed.
- Use Server-Sent Events for one-way live logs because the UI never needs a command channel; no shell or arbitrary execution endpoint is introduced.
- Use ESP-IDF asynchronous HTTP requests and a dedicated worker for SSE so status/config/logout remain responsive; every worker exit converges on exactly-once request completion. Each stop attempt waits at most four seconds for the exact active worker generation, while exceptional cleanup retries after SoftAP removal have no fixed retry ceiling.
- Feed `mosquitto_passwd` interactively through stdin so the generated device password is absent from host/container argv and suppressed output.
- Keep OTA out of scope and leave 768 KiB unallocated rather than silently consuming the full 4 MiB physical flash.
- Treat the layout migration as destructive: old NVS is not claimed preservable and requires `erase-flash` plus a complete flash.
- Treat native IPv6 provisioning as unimplemented and fail closed; accept only the live SoftAP IPv4 endpoint in either native IPv4 or IPv4-mapped IPv6 form, never a hard-coded authorization address or client address alone.

## Verification

### Local Automated Verification

- Clean ESP-IDF 6.0.2 build from versioned defaults passed; both bootloader and application headers report 4 MiB.
- Generated table passed ESP-IDF and repository layout checks with non-overlapping aligned offsets.
- Application image after final hardening: 1,022,592 bytes in a 3,145,728-byte partition, leaving 2,123,136 bytes (67%) application headroom; 786,432 bytes remain unallocated at the end of flash.
- Target debug types report `device_config_t` as 4,876 bytes, one diagnostic record as 208 bytes and the eight-record batch as 1,720 bytes; the batch also has a 2 KiB compile-time ceiling.
- Full pytest suite: 141 passed, including compiled production C for blocking mutex synchronization, stable storage/validation, setup-secret failure modes, revision-bound activation and interrupted candidate/metadata/active commits, activation/rollback, boot-attempt limits, redaction, dual-stack endpoint classification, concurrent state/error access, diagnostic batching/wrap/loss, entropy ordering, generation-bound EventSource authorization and deterministic exact-worker SSE lifecycle interleavings, plus flash-layout rejection, disconnected-state RSSI gating and provisioning secret/DOM-cleanup guards.
- Frontend: one test, lint and direct TypeScript check passed under Node.js 24.19.0; a no-cache container build completed the Next.js 16.3.0 TypeScript, static generation and standalone production build. Direct host Turbopack remains blocked by this execution sandbox's local-bind policy, not by a compilation error.
- Containers: `docker compose config --quiet` and image builds passed; isolated secure smoke passed TLS/hostname/authentication/ACL negatives, telemetry, health, availability, Last Will, collector, SQLite persistence, API and frontend checks.
- MQTT lifecycle smoke passed real broker add, rotation with rejection of the prior password, and revocation with publish rejection.
- Repository and image scans found no tracked or embedded `.env`, private key, provisioning package, local `sdkconfig`, database, `node_modules` or generated build tree; `git diff --check` passed.
- Unreadable-NVS-metadata fault injection passed in deterministic automated tests only; no physical fault-injection result is claimed.

### Operator-Provided Manual Hardware Verification

- Physical flash identification supplied by the operator: ESP32-D0WD-V3 revision 3.1, 4 MiB, 3.3 V.
- Manual hardware verification passed for the 4 MiB layout, first boot and one-time secret, corrected phone/SoftAP path, web controls/redaction, status/log/SSE, provisioning and gated activation, NVS persistence, interrupted validation and rollback, APSTA-to-STA continuity, machine GPIO, MQTT rotation/revocation/restoration, web factory reset, confirmed serial full-NVS recovery and complete reprovisioning.
- This evidence was supplied by the operator and remains distinct from local automation and CI. Unreadable-NVS-metadata fault injection was not executed physically.

### GitHub-Hosted Verification

- The operator reports that the Sprint 17 commit was published and GitHub Actions completed without errors.
- Backend tests: passed.
- Frontend checks: passed.
- Firmware build: passed.
- Container build and smoke test: passed.
- No run ID, commit SHA, duration or additional workflow output is recorded here because none was supplied.

## Takeaway

Sprint 17 closes with a controlled provisioning architecture, reproducible 4 MiB firmware baseline, passed local automation, an operator-accepted hardware path and a successful GitHub-hosted verification of the published commit.

---

# Sprint 18 — Portfolio Completion, Multi-Device Demonstration and Final Validation

## Status

**SPRINT 18 COMPLETED**

Sprint 18 software and local gates are implemented and verified. Operator-provided demo and physical BME280 evidence dated 22 August 2026 are PASS, all four required application screenshots are verified, and GitHub Actions workflow run 8 completed successfully for pushed implementation commit `2b75029`. Its real `docs/images/portfolio/github-actions-green.png` evidence also passed structural, visual and sensitive-content inspection.

All REQUIRED evidence is PASS: **PORTFOLIO COMPLETE. MAINTENANCE MODE.** No tag or GitHub Release was created or is implied; either remains a separate operator decision. The final documentation-only closure changes are local until the operator separately commits or pushes them.

## Goal and boundary

Package the existing system as professional evidence, demonstrate its real multi-device contract, close verified validation gaps and stop product expansion. The result remains a portfolio-grade, single-host connected-device reference platform, not SaaS, multi-tenancy, fleet management or a generalized cloud product.

## Completed work

- Replaced collector timestamp coercion with one reusable, lossless device-timestamp validator. Non-null input must match `YYYY-MM-DDTHH:MM:SS[.fraction](Z|±HH:MM)` exactly; `T`, uppercase `Z` or an hours-and-minutes offset are mandatory, and a fraction may contain only zeros. The lexical check precedes parsing, including digits beyond microseconds; UTC normalization is checked again for zero microseconds and persists as canonical `YYYY-MM-DDTHH:MM:SSZ`. Health and component timestamps reuse it, while health null and backend `received_at` semantics remain intact.
- Made telemetry temperature/humidity strict finite JSON numbers with existing physical ranges. Health counters/metrics remain strict, reject booleans/strings/non-finite values and preserve only their existing null allowances.
- Unified backend and lifecycle tooling on the firmware/tooling ASCII device pattern `[A-Za-z0-9][A-Za-z0-9._-]{0,62}` and explicit identity domains. Backend configuration fixes `LEGACY_DEVICE_ID` to exactly `legacy-device`; any override to another value fails validation at startup. That identity remains internal to the legacy topic, migrations and historical backend data, but ordinary per-device publishing, simulator configuration, firmware provisioning and lifecycle add/rotate reject it. A historical accidental bundle entry can still be inspected/revoked. Valid existing devices, `%u` ACL behavior and service identities remain supported in their own domains.
- Converted the simulator from legacy random telemetry into an explicit `edge-node-02` device path with deterministic configurable measurements, dedicated TLS credentials, CA/hostname verification, per-device least-privilege topics, retained health/online state, offline Last Will, periodic updates, reconnect online state and graceful retained offline shutdown.
- Added the opt-in Compose `demo` profile; ordinary stack startup still excludes the simulator. Health identifies only a software `simulator` component and makes no BME280/GPIO/Wi-Fi claims.
- Fixed one integration-discovered runtime incompatibility: the generated `edge-node-02.password` now uses the same non-root UID 10001/host-group `0440` model as the collector secret. The explicit idempotent `normalize-permissions` command validates a managed nonsymlink bundle and changes only owner/mode metadata; tests prove byte-for-byte content preservation with `edge-node-02` present, already normalized or intentionally revoked.
- Hardened BME280 recovery: read/communication or invalid-measurement faults invalidate the provider and safely remove its I²C handle; the next telemetry cycle performs one full chip-ID/reset/calibration/configuration attempt. Removal failure retains the handle and blocks duplicate registration. A host-side sequence test covers initialization, read failure, deinitialization, next-cycle reinitialization and valid recovery.
- Made the demo repeatable and nondestructive: conditional environment/bundle setup, existing-trust-domain preflight, fail-fast port checks, an idempotent controlled alert rule, explicit smoke-before-demo ordering, SIGKILL/stopped/start/graceful timing and exact non-volume cleanup.
- Extended the security smoke with a coercion-prone invalid MQTT payload and a deterministic API/database absence assertion in addition to collector rejection.
- Added an isolated multi-device smoke covering device registry, per-device telemetry/history/statistics/health/availability, an `edge-node-02`-only alert, SIGKILL/LWT isolation, restart recovery, graceful shutdown, non-root execution, log-secret markers and project-scoped cleanup.
- Added the technical demo, GitHub Mermaid portfolio diagram, BME280 failure/recovery runbook, screenshot capture/redaction specification, README portfolio framing and closure-focused roadmap.

## Validation contract: before and after

| Domain | Before | After | Status |
| --- | --- | --- | --- |
| Device timestamps | Naive datetimes accepted; offsets retained as supplied; fractional values could be silently truncated | Exact lossless RFC 3339 profile required before parsing; offsets normalize to UTC `Z`; absent/all-zero fractions accepted; nonzero fractions at any digit, offset seconds/fractions, permissive forms, unparsable and naive values rejected | PASS |
| Health time | Null supported but non-null used permissive datetime parsing | Null preserved; health and component non-null timestamps share strict normalization | PASS |
| Telemetry numbers | Pydantic coerced booleans/numeric strings | JSON int/float accepted; boolean/string/NaN/±Infinity rejected; physical bounds preserved | PASS |
| Health numbers | Existing strict counters/metrics | Strict behavior retained and covered together with telemetry regressions | PASS |
| Device identity | Backend Unicode `isalnum()` diverged from firmware/tooling; `legacy-device` could collide with ordinary provisioning or be reassigned by configuration | Shared explicit ASCII syntax plus separate ordinary, fixed legacy compatibility and service domains; `LEGACY_DEVICE_ID` accepts only `legacy-device` | PASS |

## Verification record

- Backend/host tests: `pytest -q` with an initially nonexistent database below `/tmp` — **219 passed**, including **15** firmware host-side tests; the development database was not read or modified.
- Frontend under official Node.js `v24.19.0`, npm `11.17.0`: `npm ci --ignore-scripts` PASS; `npm test` — 1 passed; lint PASS; `npx tsc --noEmit` PASS; Next.js 16.3.0 production build PASS; full and production npm audits — 0 vulnerabilities.
- Firmware: ESP-IDF `v6.0.2`; `fullclean` and build PASS; application 1,022,944 bytes in a 3,145,728-byte factory partition; 2,122,784 bytes (67%) headroom; 786,432-byte unallocated reserve; 4 MiB layout gate PASS. No flash or erase was performed; the local build tree and generated `sdkconfig` were removed afterward.
- Compose: configuration PASS; `docker compose build --pull` PASS for non-root backend/frontend images.
- Security smoke: TLS, hostname/CA verification, authentication, positive/negative ACL, telemetry ingestion, invalid payload non-persistence, health, retained availability, Last Will, legacy compatibility, SQLite persistence across recreation, effective non-root service UIDs and image-history secret-marker scan PASS.
- Credential lifecycle smoke: add, rotate with old-password rejection/new-password acceptance and revoke rejection PASS.
- Multi-device smoke: `edge-node-01`/`edge-node-02` isolation, separate history/statistics/health/availability, idempotent device-scoped alert reuse, SIGKILL/LWT, restart, graceful offline, non-root runtime and log marker scan PASS.
- Cleanup: no Sprint 18 smoke container, network, volume or temporary security directory remained after traps completed.
- Existing-bundle portfolio preflight PASS using a public certificate SAN: version-1/symlink boundary, chain/validity/hostname, conditional permission normalization, free ports, stopped stack, demo-only simulator profile and non-root password-file readability were verified without printing credential content.

### GitHub-hosted verification — 22 August 2026

- Pushed Sprint 18 implementation commit: `2b75029` (`feat(portfolio): add multi-device demo and validation hardening`).
- GitHub Actions workflow run 8: **Success**.
- `Backend tests`: PASS.
- `Frontend checks`: PASS.
- `Firmware build`: PASS.
- `Container build and smoke test`: PASS.
- The real workflow capture at `docs/images/portfolio/github-actions-green.png` was inspected directly. It shows the matching commit, Success status and all four green jobs; it contains no visible sensitive data. This is CI evidence only and does not claim a deployment, tag or GitHub Release.

### Operator-provided manual evidence — 22 August 2026

- **BME280 communication failure/recovery — PASS.** Sprint 18 firmware was flashed without erasing NVS. Physically interrupting SDA/SCL produced `ESP_ERR_INVALID_RESPONSE`; failed samples were rejected, sensor health became `FAULT`, overall health became `DEGRADED`, and availability stayed `ONLINE`. MQTT, system time and machine status stayed `HEALTHY`. Complete reinitialization attempts continued. After reconnection the BME280 was detected, calibration data loaded, initialization completed, telemetry resumed, `samples_ok` advanced, and sensor/overall health returned `HEALTHY` without a manual ESP32 reboot.
- **BME280 sensor-module power interruption/recovery — PASS.** The operator removed sensor power while leaving the ESP32 operational. The fault was detected and samples rejected while ESP32 and MQTT remained operational. Restoring power triggered BME280 reinitialization and automatic telemetry/health recovery without a manual ESP32 reboot.
- **Operator portfolio demo — PASS.** The operator verified physical `edge-node-01`, opt-in simulated `edge-node-02`, selector-scoped telemetry/statistics/health/availability, simulator-only component health, an `edge-node-02`-only high-temperature rule and alert, SIGKILL with retained LWT isolation, restart/telemetry recovery, SIGTERM retained offline, and graceful restart/telemetry recovery.
- **Evidence boundary.** No physical NVS-metadata fault injection or other unreported physical test is claimed.

### Screenshot inspection — 22 August 2026

- `dashboard-two-devices.png` — **PASS**, 2740×1821 RGBA PNG; both online identities visible in the open selector and `edge-node-02` data rendered.
- `device-health.png` — **PASS**, 2745×1932 RGBA PNG; physical `edge-node-01` component health and counters visible.
- `alert-active-history.png` — **PASS**, 2745×1836 RGBA PNG; active rule and event history visibly scoped to `edge-node-02`.
- `device-offline.png` — **PASS**, 2745×1880 RGBA PNG; `edge-node-02` is visibly offline with its alert and rule retained.
- All four supplied application images passed direct pixel review for coherent framing and absence of visible passwords, setup secrets, tokens, cookies, CSRF values, SSIDs, private keys, provisioning packages and credential-bearing terminals. They are real dashboard captures, not placeholders or generated illustrations.
- `github-actions-green.png` — **PASS**, 3756×1446 RGBA PNG; workflow run 8, pushed commit `2b75029`, Success and all four green jobs are visible with no sensitive data.

## Definition of Done

| Required evidence | Status | Evidence or exact blocker |
| --- | --- | --- |
| Sprint 18 software implementation | PASS | Strict validation, simulator, reliability hardening and documentation package are locally implemented. |
| Strict collector timestamp/numeric/device-ID contract | PASS | Unit, integration and Compose non-persistence tests. |
| Multi-device simulator and isolation | PASS | Opt-in `edge-node-02` plus isolated registry, telemetry, history, statistics, health and availability acceptance smoke. |
| Alert lifecycle | PASS | Device-scoped rule activation/history and `edge-node-01` non-contamination verified automatically and by the operator demo. |
| MQTT Last Will and graceful shutdown | PASS | SIGKILL/LWT offline isolation, restart recovery, SIGTERM retained offline and graceful restart verified. |
| Sensor failure code-path inspection and runbook | PASS | Handle-safe next-cycle full reinitialization/reject/health path inspected and host-tested; two physical procedures versioned. |
| Physical BME280 communication failure/recovery | PASS | Operator-provided 22 August 2026 SDA/SCL interruption and automatic recovery record. |
| Physical BME280 sensor-power interruption/recovery | PASS | Operator-provided 22 August 2026 sensor-power interruption and automatic recovery record. |
| Demo guide and architecture diagram | PASS | Versioned narrative, exact commands, Mermaid, limits and cleanup. |
| Operator-run complete portfolio demo | PASS | Operator-provided 22 August 2026 end-to-end physical/simulator lifecycle record. |
| Four real application screenshots | PASS | All exact paths pass structural, visual, framing and sensitive-content inspection. |
| Local backend/frontend/firmware/Docker gates | PASS | Results above. |
| Repository hygiene and internal links | PASS | Tracked-artifact/secret patterns, diff whitespace and relative links checked locally. |
| GitHub-hosted Sprint 18 workflow | PASS | Workflow run 8 for pushed commit `2b75029` completed with Success and all four jobs green. |
| `github-actions-green.png` | PASS | Real 3756×1446 RGBA capture passed structural, visual and sensitive-content inspection. |

A tag or GitHub Release is not a Sprint 18 REQUIRED gate and has not been created. It remains a separate operator decision.

## Preserved trade-offs

Single-host SQLite and named-volume persistence; no direct Internet exposure; no API/dashboard authentication or HTTPS ingress; no Secure Boot, flash/NVS encryption, OTA, telemetry store-and-forward, automatic backup or multi-host storage; authenticated provisioning HTTP protected by the unique WPA2 SoftAP but without application-layer encryption. These are explicit portfolio boundaries, not Sprint 18 feature requests.
