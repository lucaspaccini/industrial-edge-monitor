import pytest

from backend.api.alert_schemas import AlertRuleCreate
from backend.repositories import device_repository
from backend.services.alert_service import alert_service


@pytest.fixture
def alert_database(isolated_database):
    device_repository.set_availability(
        "edge-01",
        "online",
        "2026-07-26T10:00:00Z",
    )
    device_repository.set_availability(
        "edge-02",
        "online",
        "2026-07-26T10:00:00Z",
    )
    return isolated_database


@pytest.fixture
def create_rule(alert_database):
    def factory(**overrides):
        payload = {
            "name": "High temperature",
            "device_id": "edge-01",
            "metric": "temperature",
            "operator": "greater_than",
            "threshold": 30.0,
            "duration_seconds": 10,
            "hysteresis": 2.0,
            "severity": "warning",
            "enabled": True,
        }
        payload.update(overrides)
        return alert_service.create_rule(AlertRuleCreate.model_validate(payload))

    return factory
