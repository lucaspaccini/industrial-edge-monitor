from backend.api.repositories.telemetry_repository import (
    fetch_latest_telemetry,
    fetch_telemetry_history,
)


def test_fetch_latest_telemetry_returns_data():
    telemetry = fetch_latest_telemetry()

    assert telemetry is not None
    assert "temperature" in telemetry
    assert "humidity" in telemetry
    assert "machine_status" in telemetry
    assert "timestamp" in telemetry

def test_fetch_telemetry_history_returns_list():
    telemetry_history = fetch_telemetry_history(limit=10)

    assert isinstance(telemetry_history, list)
    assert len(telemetry_history) <= 10

    if telemetry_history:
        first_item = telemetry_history[0]

        assert "temperature" in first_item
        assert "humidity" in first_item
        assert "machine_status" in first_item
        assert "timestamp" in first_item