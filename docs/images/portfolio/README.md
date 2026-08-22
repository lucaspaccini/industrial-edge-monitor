# Portfolio Screenshot Evidence

Status: **PASS.** All four required application frames and the GitHub-hosted CI frame are verified. No placeholder is presented as final evidence.

Capture PNG at 1440×900 or 1600×1000, crop to the browser content, use consistent browser zoom and remove bookmarks/profile details. Never capture passwords, setup secrets, provisioning packages, private certificates/keys, session or CSRF tokens, SSIDs, unnecessary IP addresses, `.env` content or credential-bearing terminals.

Evidence register:

| File | Status | Direct inspection |
| --- | --- | --- |
| `dashboard-two-devices.png` | PASS | Real readable PNG, 2740×1821 RGBA. Open selector shows `edge-node-01` and `edge-node-02` online with `edge-node-02` telemetry, statistics and simulator-only health. |
| `device-health.png` | PASS | Real readable PNG, 2745×1932 RGBA. Shows online physical `edge-node-01`, sensor/machine/system-time/MQTT health and diagnostic counters. |
| `device-offline.png` | PASS | Real readable PNG, 2745×1880 RGBA. Shows `edge-node-02` offline with its rule/alert retained after the Last Will transition. |
| `alert-active-history.png` | PASS | Real readable PNG, 2745×1836 RGBA. Shows the active high-temperature warning, device-scoped rule and event history for `edge-node-02`. |
| `provisioning-redacted.png` | Optional / absent | Include only if a future capture is demonstrably free of setup, credential, CA, session and network details. |
| `github-actions-green.png` | PASS | Real readable PNG, 3756×1446 RGBA. Shows workflow run 8 for pushed Sprint 18 commit `2b75029`, status Success, and green Backend tests, Frontend checks, Firmware build, and Container build and smoke test jobs. |

All five supplied images were decoded and visually inspected on 22 August 2026. Their framing is consistent and suitable for GitHub/portfolio use. No visible password, setup secret, token, cookie, CSRF value, SSID, private key, provisioning package, credential-bearing terminal or other sensitive value was found. The four application frames are ordinary dashboard captures, and the CI frame is a real GitHub Actions capture; none is a placeholder or generated illustration. The visible public repository owner and public commit hash in the CI frame are not treated as secrets.
