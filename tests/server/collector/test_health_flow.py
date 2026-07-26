import json

from backend.collector.subscriber import process_message
from backend.services.device_service import device_service


def test_health_is_persisted_with_received_at_and_null_device_time(isolated_database):
    payload = {
        "schema_version": 1,
        "device_id": "edge-01",
        "timestamp": None,
        "status": "degraded",
        "availability": "online",
        "components": {
            "future_component": {
                "status": "unknown",
                "error_code": "not_initialized",
                "updated_at": None,
            }
        },
        "counters": {"future_counter": 1},
        "metrics": {"future_metric": None},
    }
    process_message(
        "industrial/devices/edge-01/health", json.dumps(payload).encode()
    )

    health = device_service.get_health("edge-01")
    assert health["timestamp"] is None
    assert health["received_at"]
    assert health["components"]["future_component"]["status"] == "unknown"
