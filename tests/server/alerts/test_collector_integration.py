import json

from backend.api.alert_schemas import AlertRuleCreate
from backend.collector.subscriber import process_message
from backend.repositories import alert_repository, telemetry_repository
from backend.services.alert_service import alert_service


def telemetry_payload(timestamp: str = "2026-07-26T10:00:01Z") -> bytes:
    return json.dumps({
        "device_id": "edge-01",
        "timestamp": timestamp,
        "temperature": 35,
        "humidity": 50,
        "machine_status": "unknown",
    }).encode()


def test_collector_evaluates_saved_telemetry(alert_database):
    rule = alert_service.create_rule(AlertRuleCreate(
        name="High",
        device_id="edge-01",
        metric="temperature",
        operator="greater_than",
        threshold=30,
        duration_seconds=0,
        hysteresis=1,
        severity="critical",
    ))

    process_message(
        "industrial/devices/edge-01/telemetry",
        telemetry_payload(),
    )

    saved = telemetry_repository.fetch_latest_telemetry("edge-01")
    assert saved is not None
    event = alert_repository.fetch_events(rule_id=rule["id"])[0]
    assert event["status"] == "active"


def test_zero_duration_rule_activates_on_next_collector_sample(alert_database):
    rule = alert_service.create_rule(AlertRuleCreate(
        name="Positive temperature",
        device_id="edge-01",
        metric="temperature",
        operator="greater_than",
        threshold=0,
        duration_seconds=0,
        hysteresis=0,
        severity="warning",
    ))
    process_message(
        "industrial/devices/edge-01/telemetry",
        telemetry_payload(),
    )
    state = alert_repository.fetch_rule(rule["id"])
    assert state["runtime_state"] == "active"
    assert state["last_telemetry_id"] is not None
    assert len(alert_repository.fetch_events(rule_id=rule["id"])) == 1


def test_positive_duration_persists_pending_then_activates(alert_database):
    rule = alert_service.create_rule(AlertRuleCreate(
        name="Dwell",
        device_id="edge-01",
        metric="temperature",
        operator="greater_than",
        threshold=0,
        duration_seconds=10,
        hysteresis=0,
        severity="warning",
    ))
    process_message(
        "industrial/devices/edge-01/telemetry",
        telemetry_payload("2026-07-26T10:00:01Z"),
    )
    pending = alert_repository.fetch_rule(rule["id"])
    assert pending["runtime_state"] == "pending"
    assert pending["pending_since"] == "2026-07-26T10:00:01Z"
    assert pending["last_evaluated_at"] == "2026-07-26T10:00:01Z"
    first_telemetry_id = pending["last_telemetry_id"]

    process_message(
        "industrial/devices/edge-01/telemetry",
        telemetry_payload("2026-07-26T10:00:11Z"),
    )
    active = alert_repository.fetch_rule(rule["id"])
    assert active["runtime_state"] == "active"
    assert active["last_telemetry_id"] > first_telemetry_id
    assert len(alert_repository.fetch_events(rule_id=rule["id"])) == 1


def test_alert_failure_does_not_rollback_telemetry(alert_database, monkeypatch):
    from backend.collector import subscriber

    def fail(_sample):
        raise RuntimeError("simulated alert engine failure")

    monkeypatch.setattr(subscriber.alert_engine, "evaluate", fail)
    process_message(
        "industrial/devices/edge-01/telemetry",
        telemetry_payload(),
    )

    assert telemetry_repository.fetch_latest_telemetry("edge-01") is not None
