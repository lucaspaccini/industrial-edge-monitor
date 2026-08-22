# BME280 Failure and Recovery Checklist

Status: **PASS — operator-provided physical evidence recorded 22 August 2026.** These procedures were executed manually by the operator, not by Sprint 18 automation or Codex.

The operator flashed the Sprint 18 firmware without erasing NVS before the procedures. No physical fault injection involving NVS metadata, unreadable metadata or any unlisted hardware path is claimed.

## Safety and prerequisites

- Use the normal low-voltage BME280 wiring and the already provisioned `edge-node-01`.
- Do not touch the machine-status wiring or any 24 V industrial circuit.
- Keep serial monitoring, the dashboard and collector logs visible without displaying credentials.
- Record the initial telemetry row, health counters and timestamps. A short screen recording or redacted screenshots are acceptable evidence.

## Procedure A — communication-line interruption

| Step | Operator action | Required observation | Result |
| --- | --- | --- | --- |
| 1 | Boot with the BME280 connected and wait for two cycles. | Valid nonzero physical telemetry; sensor health `healthy`; `samples_ok` advances. | PASS |
| 2 | Disconnect one BME280 communication line (SDA or SCL) using a controlled, board-safe method. | Firmware logs a read/acquisition error; the failed sample is rejected and the provider is invalidated. | PASS |
| 3 | Observe MQTT/API/dashboard for at least two telemetry intervals. | No new zero, stale-copy or fabricated telemetry sample; `samples_rejected` advances; sensor component becomes `fault` with `read_failed`; overall health is degraded. | PASS |
| 4 | Reconnect the communication line correctly. | On the next acquisition cycle the firmware registers one new I²C handle, repeats chip-ID/reset/calibration/configuration, and a later acquisition succeeds without reboot or reflashing; sensor health returns `healthy`. | PASS |
| 5 | Observe two further cycles. | Valid telemetry resumes and `samples_ok` advances; health recovery has a newer UTC timestamp. | PASS |

### Operator-provided Procedure A record

The interrupted SDA/SCL line produced `ESP_ERR_INVALID_RESPONSE`. Failed samples were rejected; sensor health became `FAULT`, overall health became `DEGRADED`, and availability remained `ONLINE`. MQTT, system time and machine status remained `HEALTHY`. Subsequent cycles attempted complete reinitialization. After reconnection the BME280 was detected again, calibration data loaded, initialization completed, telemetry resumed, `samples_ok` advanced, and sensor and overall health returned `HEALTHY`. The ESP32 was not manually rebooted.

## Evidence to retain

- firmware revision/HEAD and configured telemetry interval;
- before/failure/recovery timestamps and counter values;
- one redacted collector/API or dashboard record proving the telemetry gap and recovery;
- operator name/date and PASS or FAIL for every row.

## Procedure B — full sensor power interruption

Repeat steps 1–5, but at step 2 remove only the BME280 module power using a board-safe method while leaving the ESP32 powered. This is a distinct case: the sensor loses volatile configuration. Required recovery evidence is therefore a complete next-cycle reinitialization (I²C registration, chip-ID, reset, calibration read and configuration), followed by valid telemetry without an ESP32 reboot. Record Procedure A and B separately as PASS or FAIL.

Status: **PASS.** The operator physically interrupted power to the sensor module while the ESP32 remained operational. The fault was detected and failed samples were rejected; the ESP32 and MQTT stayed operational. After sensor power was restored, the BME280 was initialized again and telemetry plus sensor/overall health recovered automatically without a manual ESP32 reboot.

The code path makes at most one provider initialization attempt per telemetry cycle. A communication/read error invalidates the provider and removes its I²C device handle; a finite/range validation error does the same. The failed sample increments `samples_rejected`, marks sensor health faulty and returns before JSON serialization/publish. The next cycle performs full BME280 initialization before one read; a valid result restores healthy state and publication. A failed handle removal is retained and retried before any new registration, preventing a lost or duplicate handle. The host-side sequence test and the two separately recorded operator procedures are distinct evidence sources.
