import json
import math

import pytest
from pydantic import ValidationError

from backend.collector.schemas import HealthMessage, TelemetryMessage
from backend.collector.subscriber import process_message
from backend.repositories import telemetry_repository


def telemetry_payload(**overrides):
    payload = {
        "device_id": "edge-node-01",
        "timestamp": "2026-08-22T10:00:00Z",
        "temperature": 20.0,
        "humidity": 50.0,
        "machine_status": "running",
    }
    payload.update(overrides)
    return payload


def test_utc_z_timestamp_is_accepted_and_canonical():
    message = TelemetryMessage.model_validate(telemetry_payload())
    assert message.model_dump(mode="json")["timestamp"] == "2026-08-22T10:00:00Z"


def test_offset_timestamp_is_normalized_to_utc_before_persistence(isolated_database):
    payload = telemetry_payload(timestamp="2026-08-22T12:30:45+02:00")
    process_message(
        "industrial/devices/edge-node-01/telemetry",
        json.dumps(payload).encode(),
    )
    stored = telemetry_repository.fetch_latest_telemetry("edge-node-01")
    assert stored["timestamp"] == "2026-08-22T10:30:45Z"


@pytest.mark.parametrize(
    "timestamp",
    [
        "2026-08-22T10:00:00.000Z",
        "2026-08-22T10:00:00.000000000Z",
        "2026-08-22T12:00:00.000000+02:00",
    ],
)
def test_zero_fraction_timestamp_is_accepted_without_loss(timestamp):
    message = TelemetryMessage.model_validate(telemetry_payload(timestamp=timestamp))
    assert message.model_dump(mode="json")["timestamp"] == "2026-08-22T10:00:00Z"


@pytest.mark.parametrize(
    "timestamp",
    [
        "2026-08-22T10:00:00.0000009Z",
        "2026-08-22T10:00:00.000001Z",
        "2026-08-22T10:00:00.100Z",
        "2026-08-22T12:00:00.900+02:00",
    ],
)
def test_nonzero_fraction_timestamp_is_rejected_instead_of_truncated(timestamp):
    with pytest.raises(ValidationError, match="fraction must contain only zeros"):
        TelemetryMessage.model_validate(telemetry_payload(timestamp=timestamp))


def test_distinct_subsecond_timestamps_cannot_collapse_to_one_canonical_value():
    for timestamp in (
        "2026-08-22T10:00:00.100Z",
        "2026-08-22T10:00:00.900Z",
    ):
        with pytest.raises(ValidationError, match="fraction must contain only zeros"):
            TelemetryMessage.model_validate(telemetry_payload(timestamp=timestamp))


@pytest.mark.parametrize(
    "timestamp",
    [
        "2026-08-22T10:00:00",
        "not-a-timestamp",
        "2026-Z08-22T10:00:00Z",
        "2026-08-22T10:00:00z",
        "2026-08-22 10:00:00Z",
        "2026-08-22T10:00:00+02:00:30",
        "2026-08-22T10:00:00+02:00:00.5",
    ],
)
def test_invalid_device_timestamp_is_rejected(timestamp):
    with pytest.raises(ValidationError):
        TelemetryMessage.model_validate(telemetry_payload(timestamp=timestamp))


@pytest.mark.parametrize("field", ["temperature", "humidity"])
@pytest.mark.parametrize("value", [True, False, "20.5"])
def test_telemetry_rejects_boolean_and_numeric_string(field, value):
    with pytest.raises(ValidationError):
        TelemetryMessage.model_validate(telemetry_payload(**{field: value}))


@pytest.mark.parametrize("field", ["temperature", "humidity"])
@pytest.mark.parametrize("value", [math.nan, math.inf, -math.inf])
def test_telemetry_rejects_non_finite_numbers(field, value):
    with pytest.raises(ValidationError):
        TelemetryMessage.model_validate(telemetry_payload(**{field: value}))


@pytest.mark.parametrize(
    ("temperature", "humidity"),
    [(-40, 0), (85, 100), (-40.0, 100.0), (85.0, 0.0)],
)
def test_telemetry_accepts_physical_boundaries(temperature, humidity):
    message = TelemetryMessage.model_validate(
        telemetry_payload(temperature=temperature, humidity=humidity)
    )
    assert message.temperature == float(temperature)
    assert message.humidity == float(humidity)


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("temperature", -40.01),
        ("temperature", 85.01),
        ("humidity", -0.01),
        ("humidity", 100.01),
    ],
)
def test_telemetry_rejects_out_of_range_values(field, value):
    with pytest.raises(ValidationError):
        TelemetryMessage.model_validate(telemetry_payload(**{field: value}))


@pytest.mark.parametrize(
    "device_id", ["edge-node-01", "legacy-device", "A", "a.b_C-9"]
)
def test_ascii_device_id_is_accepted(device_id):
    message = TelemetryMessage.model_validate(telemetry_payload(device_id=device_id))
    assert message.device_id == device_id


@pytest.mark.parametrize("device_id", ["nodo-é", "edge/node", "-edge", "edge node"])
def test_unicode_or_nonconforming_device_id_is_rejected(device_id):
    with pytest.raises(ValidationError):
        TelemetryMessage.model_validate(telemetry_payload(device_id=device_id))


@pytest.mark.parametrize(
    "device_id", ["collector", "healthcheck", "simulator", "legacy-test"]
)
def test_reserved_service_identity_is_rejected_as_device_id(device_id):
    with pytest.raises(ValidationError, match="Reserved service identity"):
        TelemetryMessage.model_validate(telemetry_payload(device_id=device_id))


def test_health_accepts_null_timestamp():
    message = HealthMessage.model_validate({
        "schema_version": 1,
        "device_id": "edge-01",
        "timestamp": None,
        "status": "degraded",
        "availability": "online",
        "components": {
            "system_time": {
                "status": "degraded",
                "error_code": "time_not_synchronized",
                "updated_at": None,
            }
        },
        "counters": {"samples_ok": 0},
        "metrics": {"rssi_dbm": None},
    })
    assert message.timestamp is None


def test_non_null_component_timestamp_is_validated_and_normalized():
    payload = {
        "schema_version": 1,
        "device_id": "edge-node-01",
        "timestamp": "2026-08-22T12:00:00+02:00",
        "status": "healthy",
        "availability": "online",
        "components": {
            "sensor": {
                "status": "healthy",
                "error_code": "none",
                "updated_at": "2026-08-22T11:00:01+01:00",
            }
        },
        "counters": {},
        "metrics": {},
    }
    message = HealthMessage.model_validate(payload)
    dumped = message.model_dump(mode="json")
    assert dumped["timestamp"] == "2026-08-22T10:00:00Z"
    assert dumped["components"]["sensor"]["updated_at"] == "2026-08-22T10:00:01Z"


@pytest.mark.parametrize(
    "updated_at",
    ["2026-08-22T10:00:00", "2026-08-22T10:00:00.1Z", "invalid"],
)
def test_non_null_component_timestamp_rejects_invalid_values(updated_at):
    payload = {
        "schema_version": 1,
        "device_id": "edge-node-01",
        "timestamp": None,
        "status": "degraded",
        "availability": "online",
        "components": {
            "sensor": {
                "status": "fault",
                "error_code": "read_failed",
                "updated_at": updated_at,
            }
        },
        "counters": {},
        "metrics": {},
    }
    with pytest.raises(ValidationError):
        HealthMessage.model_validate(payload)


def test_topic_payload_device_mismatch_is_rejected():
    payload = json.dumps({
        "device_id": "edge-02",
        "timestamp": "2026-07-22T20:00:00Z",
        "temperature": 24.0,
        "humidity": 50.0,
        "machine_status": "running",
    }).encode()
    with pytest.raises(ValueError, match="do not match"):
        process_message("industrial/devices/edge-01/telemetry", payload)


def test_legacy_identity_is_internal_only_on_mqtt_topics(isolated_database):
    ordinary_payload = json.dumps(
        telemetry_payload(device_id="legacy-device")
    ).encode()
    with pytest.raises(ValueError, match="compatibility identity"):
        process_message(
            "industrial/devices/legacy-device/telemetry", ordinary_payload
        )

    legacy_payload = telemetry_payload(device_id="payload-value-is-ignored")
    process_message("industrial/telemetry", json.dumps(legacy_payload).encode())
    stored = telemetry_repository.fetch_latest_telemetry("legacy-device")
    assert stored is not None
    assert stored["device_id"] == "legacy-device"
