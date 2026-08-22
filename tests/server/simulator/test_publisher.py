import json

import pytest
from pydantic import ValidationError

from backend.collector.schemas import (
    AvailabilityMessage,
    HealthMessage,
    TelemetryMessage,
)
from backend.core.config import Settings
from backend.simulator.publisher import (
    _availability_payload,
    _health_payload,
    _telemetry_payload,
    _topics,
)


def simulator_settings(**overrides) -> Settings:
    values = {
        "APP_ENV": "test",
        "CORS_ORIGINS": "http://localhost:3000",
        "SIMULATOR_DEVICE_ID": "edge-node-02",
        "SIMULATOR_TEMPERATURE": 41.5,
        "SIMULATOR_HUMIDITY": 60,
        "SIMULATOR_MACHINE_STATUS": "running",
    }
    values.update(overrides)
    return Settings(_env_file=None, **values)


def test_simulator_uses_per_device_topics_and_valid_payload_contracts():
    config = simulator_settings()
    assert _topics(config) == {
        "telemetry": "industrial/devices/edge-node-02/telemetry",
        "health": "industrial/devices/edge-node-02/health",
        "availability": "industrial/devices/edge-node-02/availability",
    }

    telemetry = TelemetryMessage.model_validate_json(_telemetry_payload(config))
    health = HealthMessage.model_validate_json(_health_payload(config, 3))
    availability = AvailabilityMessage.model_validate_json(
        _availability_payload(config, "offline")
    )

    assert telemetry.device_id == health.device_id == availability.device_id
    assert telemetry.temperature == 41.5
    assert health.components.keys() == {"simulator"}
    assert health.counters == {"samples_ok": 3}
    assert availability.status == "offline"


def test_simulator_payload_is_deterministic_except_for_timestamp(monkeypatch):
    config = simulator_settings()
    monkeypatch.setattr(
        "backend.simulator.publisher._now",
        lambda: "2026-08-22T10:00:00Z",
    )
    first = json.loads(_telemetry_payload(config))
    second = json.loads(_telemetry_payload(config))
    assert first == second


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("SIMULATOR_TEMPERATURE", 85.1),
        ("SIMULATOR_HUMIDITY", -0.1),
        ("SIMULATOR_DEVICE_ID", "nodo-é"),
        ("SIMULATOR_DEVICE_ID", "simulator"),
        ("SIMULATOR_DEVICE_ID", "legacy-device"),
    ],
)
def test_invalid_simulator_contract_is_rejected(field, value):
    with pytest.raises(ValidationError):
        simulator_settings(**{field: value})
