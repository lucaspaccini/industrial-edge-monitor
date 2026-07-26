import pytest
from fastapi import HTTPException

from backend.api.alert_schemas import (
    AlertRuleCreate,
    AlertRulePatch,
    AlertRuleResponse,
)
from backend.api.routes.alerts import (
    create_alert_rule,
    archive_alert_rule,
    read_active_alerts,
    read_alert_event,
    read_alert_events,
    read_alert_rule,
    read_alert_rules,
    update_alert_rule,
)
from backend.services.alert_engine import alert_engine

from .test_alert_engine import sample


def rule_request(name: str = "High temperature") -> AlertRuleCreate:
    return AlertRuleCreate(
        name=name,
        device_id="edge-01",
        metric="temperature",
        operator="greater_than",
        threshold=30,
        duration_seconds=0,
        hysteresis=2,
        severity="warning",
        enabled=True,
    )


def test_rule_api_crud_and_filters(alert_database):
    created = create_alert_rule(rule_request())
    AlertRuleResponse.model_validate(created)
    assert read_alert_rule(created["id"])["id"] == created["id"]
    assert [rule["id"] for rule in read_alert_rules("edge-01", None)] == [
        created["id"]
    ]

    updated = update_alert_rule(
        created["id"],
        AlertRulePatch(severity="critical", enabled=False),
    )
    assert updated["severity"] == "critical"
    assert updated["enabled"] is False


def test_alert_api_active_history_order_and_filters(create_rule):
    warning = create_rule(name="Warning", duration_seconds=0)
    critical = create_rule(
        name="Critical",
        threshold=32,
        duration_seconds=0,
        severity="critical",
    )
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=35))

    active = read_active_alerts("edge-01", None, None, 100)
    assert len(active) == 2

    critical_events = read_alert_events(
        "edge-01",
        None,
        "critical",
        None,
        100,
    )
    assert [event["rule_id"] for event in critical_events] == [critical["id"]]

    history = read_alert_events(None, None, None, None, 1)
    assert len(history) == 1
    assert history[0]["rule_id"] in {warning["id"], critical["id"]}
    assert read_alert_event(history[0]["id"])["id"] == history[0]["id"]


def test_alert_api_empty_and_not_found_responses(alert_database):
    assert read_active_alerts(None, None, None, 100) == []
    assert read_alert_events(None, None, None, None, 100) == []

    with pytest.raises(HTTPException) as rule_error:
        read_alert_rule(999)
    assert rule_error.value.status_code == 404

    with pytest.raises(HTTPException) as event_error:
        read_alert_event(999)
    assert event_error.value.status_code == 404


def test_alert_api_conflict(alert_database):
    create_alert_rule(rule_request())
    with pytest.raises(HTTPException) as error:
        create_alert_rule(rule_request())
    assert error.value.status_code == 409


def test_alert_api_delete_and_delete_not_found(alert_database):
    rule = create_alert_rule(rule_request())
    response = archive_alert_rule(rule["id"])
    assert response.status_code == 204
    assert read_alert_rules("edge-01", None, False) == []

    with pytest.raises(HTTPException) as error:
        archive_alert_rule(999)
    assert error.value.status_code == 404


def test_alert_routes_are_registered(alert_database):
    from backend.api.main import app

    paths = app.openapi()["paths"]
    assert "/alert-rules/" in paths
    assert "/alert-rules/{rule_id}" in paths
    assert "/alerts/active" in paths
    assert "/alert-events" in paths
    assert "/alert-events/{event_id}" in paths
