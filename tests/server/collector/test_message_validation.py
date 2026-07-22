import json

import pytest

from backend.collector.schemas import HealthMessage
from backend.collector.subscriber import process_message


def test_health_accepts_null_timestamp():
    message = HealthMessage.model_validate({
        "schema_version": 1,
        "device_id": "edge-01",
        "timestamp": None,
        "status": "degraded",
        "availability": "online",
        "components": {
            "system_time": {
                "status": "degraded",
                "error_code": "time_not_synchronized",
                "updated_at": None,
            }
        },
        "counters": {"samples_ok": 0},
        "metrics": {"rssi_dbm": None},
    })
    assert message.timestamp is None


def test_topic_payload_device_mismatch_is_rejected():
    payload = json.dumps({
        "device_id": "edge-02",
        "timestamp": "2026-07-22T20:00:00Z",
        "temperature": 24.0,
        "humidity": 50.0,
        "machine_status": "running",
    }).encode()
    with pytest.raises(ValueError, match="do not match"):
        process_message("industrial/devices/edge-01/telemetry", payload)
