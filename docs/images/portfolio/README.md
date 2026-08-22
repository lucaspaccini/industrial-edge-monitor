# Portfolio Screenshot Evidence

Status: **APPLICATION SCREENSHOTS PASS; HOSTED-CI SCREENSHOT IN PROGRESS.** All four required application paths are verified. The hosted-CI frame is still future evidence. No placeholder is presented as a final screenshot.

Capture PNG at 1440×900 or 1600×1000, crop to the browser content, use consistent browser zoom and remove bookmarks/profile details. Never capture passwords, setup secrets, provisioning packages, private certificates/keys, session or CSRF tokens, SSIDs, unnecessary IP addresses, `.env` content or credential-bearing terminals.

Evidence register:

| File | Status | Direct inspection |
| --- | --- | --- |
| `dashboard-two-devices.png` | PASS | Real readable PNG, 2740×1821 RGBA. Open selector shows `edge-node-01` and `edge-node-02` online with `edge-node-02` telemetry, statistics and simulator-only health. |
| `device-health.png` | PASS | Real readable PNG, 2745×1932 RGBA. Shows online physical `edge-node-01`, sensor/machine/system-time/MQTT health and diagnostic counters. |
| `device-offline.png` | PASS | Real readable PNG, 2745×1880 RGBA. Shows `edge-node-02` offline with its rule/alert retained after the Last Will transition. |
| `alert-active-history.png` | PASS | Real readable PNG, 2745×1836 RGBA. Shows the active high-temperature warning, device-scoped rule and event history for `edge-node-02`. |
| `provisioning-redacted.png` | Optional / absent | Include only if a future capture is demonstrably free of setup, credential, CA, session and network details. |
| `github-actions-green.png` | IN PROGRESS | Capture only after the Sprint 18 revision is pushed and every GitHub-hosted job is verified green. |

The four supplied application images were decoded and visually inspected on 22 August 2026. Their framing is consistent and suitable for GitHub/portfolio use. No visible password, setup secret, token, cookie, CSRF value, SSID, private key, provisioning package, credential-bearing terminal or other sensitive value was found. They are ordinary application UI captures, not placeholders or generated illustrations.

Do not change the hosted-CI rows until a future pushed workflow is inspected and `github-actions-green.png` exists.
