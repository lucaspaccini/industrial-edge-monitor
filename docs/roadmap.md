# Roadmap and Closure State

Industrial Edge Monitor is bounded as a portfolio-grade, single-host connected-device reference platform. The completion strategy is **audit → package → demonstrate → close verified gaps → stop**.

## Completed product baseline

- ESP-IDF 6.0.2 firmware: BME280 acquisition, machine-status provider, SNTP UTC, telemetry validation, health diagnostics, MQTTS and reconnect/LWT behavior.
- Device-independent persistent configuration: versioned NVS active/candidate/rollback state, authenticated local WPA2 SoftAP provisioning and serial recovery.
- Authenticated MQTTS broker with verified CA/hostname, dedicated credentials, default-deny least-privilege ACLs and transactional identity add/rotate/revoke tooling.
- Multi-device collector, SQLite persistence, FastAPI telemetry/history/statistics/health endpoints, threshold alert state/events and one selector-scoped Next.js dashboard.
- Reproducible non-root Compose deployment and CI jobs for backend, frontend, firmware and containers.

## Sprint 18 REQUIRED

| Requirement | Status | Closure evidence |
| --- | --- | --- |
| Strict timestamp, numeric and ASCII identity-domain collector contract | PASS | Exact-second/no-truncation validation, ordinary-vs-legacy identity separation, unit/integration coverage and invalid-payload Compose database assertion. |
| Credible opt-in `edge-node-02` simulator | PASS | Per-device TLS identity/topics, retained health/online, LWT offline, periodic configurable telemetry and graceful shutdown. |
| Multi-device isolation and lifecycle | PASS | Isolated smoke covers registry, history, statistics, health, alert scoping, SIGKILL/LWT and restart. |
| BME280 failure and automatic recovery | PASS | Host sequence test plus operator-provided 22 August 2026 physical communication-line and sensor-power interruption procedures; automatic full reinitialization, telemetry and health recovery occurred without ESP32 reboot. |
| Portfolio demo, architecture and trade-off narrative | PASS | Versioned 5–10 minute guide and GitHub-rendered Mermaid architecture. |
| Operator-run complete portfolio demo | PASS | Operator-provided 22 August 2026 record covers both devices, selector isolation, simulator-only health, scoped alert, SIGKILL/LWT, restart, SIGTERM and graceful restart. |
| Four real application screenshots | PASS | All required PNG paths pass structural, visual, framing and sensitive-content inspection. |
| Local automated verification | PASS | Final Sprint 18 command matrix is recorded in sprint history. |
| GitHub-hosted Sprint 18 CI | PASS | Workflow run 8 for pushed commit `2b75029` completed with Success; backend, frontend, firmware and container jobs are green. |
| `github-actions-green.png` | PASS | Real 3756×1446 RGBA capture passed structural, visual and sensitive-content inspection. |

All REQUIRED rows are PASS. **SPRINT 18 COMPLETED. PORTFOLIO COMPLETE. MAINTENANCE MODE.** No tag or GitHub Release is implied by this closure; either remains a separate operator decision.

## Maintenance mode

Maintenance mode means:

- no new sprint is planned automatically;
- real bug fixes, reliability regressions and documentation corrections remain valid work;
- security advisories and compatible dependency/toolchain updates are handled with the existing gates;
- extensions are considered only when justified by a concrete operating need,
  a relevant technical requirement emerging from professional practice, a
  recurring technical request or a verified market signal;
- documented limits do not automatically become backlog to implement.

Maintenance does not imply new product scope.

## Optional portfolio extensions

These items are explicitly optional and are not the next sprint, release blockers or implied commitments:

- AWS IoT Core adapter;
- C++ Linux edge agent;
- telemetry buffering/store-and-forward;
- automatic backup and restore;
- API authentication and hardened HTTPS ingress;
- PostgreSQL or multi-host storage;
- UI redesign and richer visual polish;
- OTA;
- continuous delivery;
- additional sensor providers or hardware self-tests.

They should be undertaken only for a specific career signal or real operating requirement. Multi-tenancy, fleet management, billing, organizations, enterprise RBAC, Kubernetes, mobile apps and generalized cloud integrations remain outside this project's intended scope.

## Preserved trade-offs

The reference deployment remains single-host, not directly Internet-exposed, without API/dashboard authentication, HTTPS ingress, Secure Boot, flash/NVS encryption, OTA, store-and-forward, automatic backup or multi-host storage. Provisioning HTTP is authenticated and constrained to the unique WPA2 SoftAP, but not encrypted at application level. These limits are explicit architecture choices and do not by themselves block portfolio completion.
