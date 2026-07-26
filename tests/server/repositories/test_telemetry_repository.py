from backend.repositories.telemetry_repository import (
    fetch_latest_telemetry,
    fetch_telemetry_history,
    insert_telemetry,
)


def sample_payload(timestamp: str = "2026-07-26T10:00:00Z") -> dict:
    return {
        "device_id": "test-device",
        "timestamp": timestamp,
        "temperature": 23.5,
        "humidity": 48.0,
        "machine_status": "unknown",
    }


def test_fetch_latest_telemetry_returns_data(isolated_database):
    telemetry_id = insert_telemetry(sample_payload())

    telemetry = fetch_latest_telemetry()

    assert telemetry is not None
    assert telemetry_id == telemetry["id"]
    assert telemetry["device_id"] == "test-device"
    assert "temperature" in telemetry
    assert "humidity" in telemetry
    assert "machine_status" in telemetry
    assert "timestamp" in telemetry

def test_fetch_telemetry_history_returns_list(isolated_database):
    insert_telemetry(sample_payload("2026-07-26T10:00:00Z"))
    insert_telemetry(sample_payload("2026-07-26T10:00:01Z"))

    telemetry_history = fetch_telemetry_history(limit=10)

    assert isinstance(telemetry_history, list)
    assert len(telemetry_history) == 2

    first_item = telemetry_history[0]
    assert first_item["timestamp"] == "2026-07-26T10:00:01Z"
    assert "temperature" in first_item
    assert "humidity" in first_item
    assert "machine_status" in first_item
