import math

import pytest
from pydantic import ValidationError

from backend.collector.schemas import HealthMessage


def health_payload(
    *,
    counters: dict | None = None,
    metrics: dict | None = None,
    error_code: str = "none",
) -> dict:
    return {
        "schema_version": 1,
        "device_id": "edge-01",
        "timestamp": None,
        "status": "healthy",
        "availability": "online",
        "components": {
            "sensor": {
                "status": "healthy",
                "error_code": error_code,
                "updated_at": None,
            }
        },
        "counters": counters if counters is not None else {},
        "metrics": metrics if metrics is not None else {},
    }


def test_counter_accepts_zero():
    message = HealthMessage.model_validate(
        health_payload(counters={"samples_rejected": 0})
    )
    assert message.counters["samples_rejected"] == 0


def test_counter_rejects_negative_value():
    with pytest.raises(ValidationError):
        HealthMessage.model_validate(
            health_payload(counters={"samples_rejected": -1})
        )


def test_counter_rejects_boolean_value():
    with pytest.raises(ValidationError):
        HealthMessage.model_validate(
            health_payload(counters={"samples_rejected": True})
        )


@pytest.mark.parametrize("value", [-57, -57.5])
def test_metric_accepts_finite_value(value):
    message = HealthMessage.model_validate(
        health_payload(metrics={"wifi_rssi_dbm": value})
    )
    assert message.metrics["wifi_rssi_dbm"] == value


def test_metric_accepts_null_value():
    message = HealthMessage.model_validate(
        health_payload(metrics={"wifi_rssi_dbm": None})
    )
    assert message.metrics["wifi_rssi_dbm"] is None


def test_metric_rejects_nan():
    with pytest.raises(ValidationError):
        HealthMessage.model_validate(
            health_payload(metrics={"wifi_rssi_dbm": math.nan})
        )


@pytest.mark.parametrize("value", [math.inf, -math.inf])
def test_metric_rejects_infinity(value):
    with pytest.raises(ValidationError):
        HealthMessage.model_validate(
            health_payload(metrics={"wifi_rssi_dbm": value})
        )


def test_metric_rejects_boolean_value():
    with pytest.raises(ValidationError):
        HealthMessage.model_validate(
            health_payload(metrics={"wifi_rssi_dbm": False})
        )


def test_component_accepts_known_error_code():
    message = HealthMessage.model_validate(
        health_payload(error_code="communication_failed")
    )
    assert message.components["sensor"].error_code == "communication_failed"


def test_component_rejects_unknown_error_code():
    with pytest.raises(ValidationError):
        HealthMessage.model_validate(
            health_payload(error_code="future_unregistered_error")
        )
