from backend.repositories import alert_repository
from backend.services.alert_engine import AlertEngine, alert_engine


def sample(
    telemetry_id: int,
    timestamp: str,
    *,
    device_id: str = "edge-01",
    temperature: float = 20.0,
    humidity: float = 50.0,
) -> dict:
    return {
        "id": telemetry_id,
        "device_id": device_id,
        "timestamp": timestamp,
        "temperature": temperature,
        "humidity": humidity,
        "machine_status": "unknown",
    }


def test_pending_returns_to_normal_without_event(create_rule):
    rule = create_rule()
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=35))
    assert alert_repository.fetch_rule(rule["id"])["runtime_state"] == "pending"

    alert_engine.evaluate(sample(2, "2026-07-26T10:00:05Z", temperature=29))
    state = alert_repository.fetch_rule(rule["id"])
    assert state["runtime_state"] == "normal"
    assert alert_repository.fetch_events(rule_id=rule["id"]) == []


def test_pending_activates_once_after_duration_and_survives_restart(create_rule):
    rule = create_rule()
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=35))

    restarted_engine = AlertEngine()
    restarted_engine.evaluate(
        sample(2, "2026-07-26T10:00:11Z", temperature=32)
    )
    restarted_engine.evaluate(
        sample(3, "2026-07-26T10:00:12Z", temperature=33)
    )

    events = alert_repository.fetch_events(rule_id=rule["id"])
    assert len(events) == 1
    assert events[0]["status"] == "active"
    assert events[0]["started_at"] == "2026-07-26T10:00:01Z"
    assert events[0]["extreme_value"] == 35


def test_zero_duration_activates_on_first_violation(create_rule):
    rule = create_rule(duration_seconds=0)
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=31))
    assert alert_repository.fetch_rule(rule["id"])["runtime_state"] == "active"
    assert len(alert_repository.fetch_events(rule_id=rule["id"])) == 1


def test_hysteresis_band_keeps_active_then_recovery_resolves(create_rule):
    rule = create_rule(duration_seconds=0)
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=35))
    alert_engine.evaluate(sample(2, "2026-07-26T10:00:02Z", temperature=29))
    assert alert_repository.fetch_rule(rule["id"])["runtime_state"] == "active"

    alert_engine.evaluate(sample(3, "2026-07-26T10:00:03Z", temperature=28))
    event = alert_repository.fetch_events(rule_id=rule["id"])[0]
    assert event["status"] == "resolved"
    assert event["resolution_reason"] == "condition_recovered"
    assert event["extreme_value"] == 35


def test_less_than_rule_tracks_minimum_and_resolves(create_rule):
    rule = create_rule(
        name="Low humidity",
        metric="humidity",
        operator="less_than",
        threshold=40,
        hysteresis=5,
        duration_seconds=0,
    )
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", humidity=35))
    alert_engine.evaluate(sample(2, "2026-07-26T10:00:02Z", humidity=30))
    alert_engine.evaluate(sample(3, "2026-07-26T10:00:03Z", humidity=44))
    assert alert_repository.fetch_rule(rule["id"])["runtime_state"] == "active"
    alert_engine.evaluate(sample(4, "2026-07-26T10:00:04Z", humidity=45))
    event = alert_repository.fetch_events(rule_id=rule["id"])[0]
    assert event["status"] == "resolved"
    assert event["extreme_value"] == 30


def test_duplicate_equal_older_and_cross_device_samples_are_ignored(create_rule):
    rule = create_rule(duration_seconds=0)
    first = sample(10, "2026-07-26T10:00:10Z", temperature=31)
    alert_engine.evaluate(first)
    alert_engine.evaluate(first)
    alert_engine.evaluate(sample(11, "2026-07-26T10:00:10Z", temperature=20))
    alert_engine.evaluate(sample(12, "2026-07-26T10:00:09Z", temperature=20))
    alert_engine.evaluate(
        sample(
            13,
            "2026-07-26T10:00:20Z",
            device_id="edge-02",
            temperature=20,
        )
    )
    state = alert_repository.fetch_rule(rule["id"])
    assert state["runtime_state"] == "active"
    assert state["last_telemetry_id"] == 10
    assert len(alert_repository.fetch_events(rule_id=rule["id"])) == 1


def test_multiple_rules_for_same_device_are_evaluated(create_rule):
    high = create_rule(name="High", duration_seconds=0)
    low = create_rule(
        name="Low",
        operator="less_than",
        threshold=10,
        duration_seconds=0,
    )
    alert_engine.evaluate(sample(1, "2026-07-26T10:00:01Z", temperature=35))
    assert alert_repository.fetch_rule(high["id"])["runtime_state"] == "active"
    assert alert_repository.fetch_rule(low["id"])["runtime_state"] == "normal"
