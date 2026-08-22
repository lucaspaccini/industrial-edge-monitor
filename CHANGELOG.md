# Changelog

All notable changes to Industrial Edge Monitor are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-08-22

### Added

- Modular ESP-IDF firmware for ESP32 with BME280 acquisition, machine-status input, SNTP UTC synchronization, MQTT connectivity, component health diagnostics and availability reporting.
- Persistent device configuration with active/candidate/rollback state, authenticated WPA2 SoftAP provisioning, controlled activation, serial recovery and MQTT credential lifecycle tooling.
- Per-device MQTT telemetry, retained health and retained availability topics, including offline Last Will behavior and explicit legacy-topic compatibility.
- Python collector, SQLite persistence and FastAPI endpoints for device registry, telemetry history, statistics, health, availability, alert rules and alert event history.
- Device-scoped threshold alerting with dwell time, hysteresis and persistent active/resolved lifecycle state.
- Next.js dashboard with a device selector that consistently scopes telemetry, statistics, health, availability and alerts.
- Opt-in deterministic `edge-node-02` simulator and an isolated multi-device demonstration covering device separation, alert scoping, abrupt Last Will transitions and restart recovery.
- Reproducible Docker Compose deployment, non-root application containers and GitHub Actions jobs for backend, frontend, firmware and container integration checks.
- Versioned architecture, setup, security, provisioning, operations, CI, failure-recovery and technical-demo documentation.

### Changed

- Replaced permissive device timestamp parsing with a lossless RFC 3339 profile and canonical UTC persistence.
- Enforced non-coercive finite numeric measurements, physical bounds and explicit ASCII device-identity domains while preserving the fixed `legacy-device` compatibility path.
- Hardened BME280 communication-failure recovery so failed samples are rejected and the sensor is fully reinitialized on subsequent acquisition cycles.
- Standardized the frontend development, CI and production-image baseline on Node.js 24.19.0 and the verified Next.js dependency line.
- Evolved the original single-device telemetry path into a coherent multi-device model across ingestion, persistence, APIs, dashboard state and alerts.

### Security

- Added MQTT TLS with CA and hostname verification, mandatory authentication, dedicated client identities and default-deny least-privilege topic ACLs.
- Added transactional device credential add, rotation and revocation flows without storing secrets in the repository or printing them in normal tooling output.
- Added isolated security and lifecycle smoke tests covering TLS failures, authentication failures, positive and negative ACL paths, invalid-payload non-persistence, Last Will behavior and cleanup of temporary resources.
- Restricted application containers to non-root runtime users and added checks for credential leakage in images and service logs.
- Documented the trusted-LAN, single-host security boundary and the absence of API/dashboard authentication, HTTPS ingress, Secure Boot and flash/NVS encryption.
