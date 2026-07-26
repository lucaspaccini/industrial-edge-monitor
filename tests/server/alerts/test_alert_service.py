import pytest
from fastapi import HTTPException

from backend.api.alert_schemas import AlertRuleCreate, AlertRulePatch
from backend.repositories import alert_repository
from backend.services.alert_engine import alert_engine
from backend.services.alert_service import alert_service

from .test_alert_engine import sample


def test_rule_crud_and_device_policy(create_rule):
    rule = create_rule()
    assert alert_service.get_rule(rule["id"])["name"] == "High temperature"
    assert len(alert_service.get_rules(device_id="edge-01")) == 1

    updated = alert_service.update_rule(
        rule["id"],
        AlertRulePatch(name="Updated", severity="critical"),
    )
    assert updated["name"] == "Updated"
    assert updated["severity"] == "critical"

    with pytest.raises(HTTPException) as error:
        alert_service.create_rule(
            AlertRuleCreate(
                name="Missing",
                device_id="missing-device",
                metric="temperature",
                operator="greater_than",
                threshold=30,
                severity="warning",
            )
        )
    assert error.value.status_code == 404


def test_disabling_pending_resets_without_event(create_rule):
    rule = create_rule()
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=31))
    disabled = alert_service.update_rule(
        rule["id"],
        AlertRulePatch(enabled=False),
    )
    assert disabled["runtime_state"] == "normal"
    assert alert_repository.fetch_events(rule_id=rule["id"]) == []


def test_disabling_active_resolves_event(create_rule):
    rule = create_rule(duration_seconds=0)
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=31))
    alert_service.update_rule(rule["id"], AlertRulePatch(enabled=False))
    event = alert_repository.fetch_events(rule_id=rule["id"])[0]
    assert event["status"] == "resolved"
    assert event["resolution_reason"] == "rule_disabled"


def test_substantial_update_resets_and_resolves_active_event(create_rule):
    rule = create_rule(duration_seconds=0)
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=31))
    updated = alert_service.update_rule(
        rule["id"],
        AlertRulePatch(threshold=40),
    )
    assert updated["runtime_state"] == "normal"
    event = alert_repository.fetch_events(rule_id=rule["id"])[0]
    assert event["resolution_reason"] == "rule_updated"


def test_substantial_update_resets_pending_without_event(create_rule):
    rule = create_rule()
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=31))
    updated = alert_service.update_rule(
        rule["id"],
        AlertRulePatch(duration_seconds=20),
    )
    assert updated["runtime_state"] == "normal"
    assert alert_repository.fetch_events(rule_id=rule["id"]) == []


def test_archiving_normal_rule_hides_it_and_allows_name_reuse(create_rule):
    rule = create_rule()
    alert_service.archive_rule(rule["id"])

    assert alert_service.get_rules(device_id="edge-01") == []
    archived = alert_service.get_rules(
        device_id="edge-01",
        include_archived=True,
    )[0]
    assert archived["archived_at"] is not None
    assert archived["enabled"] is False

    replacement = create_rule()
    assert replacement["name"] == rule["name"]
    assert replacement["id"] != rule["id"]


def test_archiving_pending_rule_resets_state(create_rule):
    rule = create_rule()
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=31))
    alert_service.archive_rule(rule["id"])

    archived = alert_service.get_rule(rule["id"])
    assert archived["runtime_state"] == "normal"
    assert archived["pending_since"] is None
    assert archived["last_telemetry_id"] is None


def test_archiving_active_rule_resolves_and_preserves_history(create_rule):
    rule = create_rule(duration_seconds=0)
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=31))
    alert_service.archive_rule(rule["id"])

    events = alert_repository.fetch_events(rule_id=rule["id"])
    assert len(events) == 1
    assert events[0]["status"] == "resolved"
    assert events[0]["resolution_reason"] == "rule_archived"


def test_archived_rule_is_not_evaluated(create_rule):
    rule = create_rule(duration_seconds=0)
    alert_service.archive_rule(rule["id"])
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=40))

    archived = alert_service.get_rule(rule["id"])
    assert archived["last_telemetry_id"] is None
    assert alert_repository.fetch_events(rule_id=rule["id"]) == []
