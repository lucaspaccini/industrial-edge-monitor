import math

import pytest
from pydantic import ValidationError

from backend.api.alert_schemas import AlertRuleCreate


def valid_rule(**overrides) -> dict:
    payload = {
        "name": "Temperature limit",
        "device_id": "edge-01",
        "metric": "temperature",
        "operator": "greater_than",
        "threshold": 30,
        "duration_seconds": 0,
        "hysteresis": 1,
        "severity": "warning",
        "enabled": True,
    }
    payload.update(overrides)
    return payload


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("metric", "pressure"),
        ("operator", "equals"),
        ("severity", "emergency"),
    ],
)
def test_rule_rejects_unknown_enums(field, value):
    with pytest.raises(ValidationError):
        AlertRuleCreate.model_validate(valid_rule(**{field: value}))


@pytest.mark.parametrize("threshold", [math.nan, math.inf, -math.inf, True])
def test_rule_rejects_invalid_threshold(threshold):
    with pytest.raises(ValidationError):
        AlertRuleCreate.model_validate(valid_rule(threshold=threshold))


@pytest.mark.parametrize("hysteresis", [-1, math.nan, math.inf, True])
def test_rule_rejects_invalid_hysteresis(hysteresis):
    with pytest.raises(ValidationError):
        AlertRuleCreate.model_validate(valid_rule(hysteresis=hysteresis))


@pytest.mark.parametrize("duration", [-1, True])
def test_rule_rejects_invalid_duration(duration):
    with pytest.raises(ValidationError):
        AlertRuleCreate.model_validate(valid_rule(duration_seconds=duration))


def test_rule_rejects_metric_threshold_out_of_range():
    with pytest.raises(ValidationError):
        AlertRuleCreate.model_validate(
            valid_rule(metric="humidity", threshold=101)
        )
