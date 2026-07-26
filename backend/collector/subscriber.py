import json
from datetime import datetime, timezone

import paho.mqtt.client as mqtt
from pydantic import ValidationError

from backend.collector.schemas import (
    AvailabilityMessage,
    HealthMessage,
    TelemetryMessage,
)
from backend.core.config import settings
from backend.core.logging import configure_logging, get_logger
from backend.database.init_db import initialize_database
from backend.repositories import device_repository
from backend.services.telemetry_service import telemetry_service
from backend.services.alert_engine import alert_engine


configure_logging()
logger = get_logger(__name__)


def _now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _route_topic(topic: str) -> tuple[str, str] | None:
    if topic == settings.MQTT_TOPIC:
        return settings.LEGACY_DEVICE_ID, "telemetry"
    prefix = settings.MQTT_TOPIC_PREFIX.strip("/") + "/"
    if not topic.startswith(prefix):
        return None
    suffix = topic[len(prefix):].split("/")
    if len(suffix) != 2 or suffix[1] not in {"telemetry", "health", "availability"}:
        return None
    return suffix[0], suffix[1]


def process_message(topic: str, raw_payload: bytes) -> None:
    route = _route_topic(topic)
    if route is None:
        raise ValueError(f"unsupported topic: {topic}")
    topic_device_id, message_type = route
    payload = json.loads(raw_payload.decode("utf-8"))
    received_at = _now()

    if topic == settings.MQTT_TOPIC:
        payload["device_id"] = settings.LEGACY_DEVICE_ID

    payload_device_id = payload.get("device_id")
    if payload_device_id != topic_device_id:
        logger.warning(
            "Rejected device identity mismatch topic_device_id=%s payload_device_id=%s",
            topic_device_id,
            payload_device_id,
        )
        raise ValueError("topic and payload device_id do not match")

    if message_type == "telemetry":
        message = TelemetryMessage.model_validate(payload)
        stored = message.model_dump(mode="json")
        telemetry_id = telemetry_service.save_telemetry(stored)
        stored["id"] = telemetry_id
        logger.debug(
            "Telemetry persisted telemetry_id=%s device_id=%s",
            telemetry_id,
            topic_device_id,
        )
        device_repository.mark_seen(
            topic_device_id,
            received_at,
            assume_online=topic == settings.MQTT_TOPIC,
        )
        try:
            logger.debug(
                "Starting alert evaluation telemetry_id=%s device_id=%s",
                telemetry_id,
                topic_device_id,
            )
            alert_engine.evaluate(stored)
        except Exception:
            logger.exception(
                "Alert evaluation failed for telemetry_id=%s; telemetry remains persisted",
                telemetry_id,
            )
    elif message_type == "health":
        message = HealthMessage.model_validate(payload)
        device_repository.upsert_health(
            topic_device_id, message.model_dump(mode="json"), received_at
        )
    else:
        message = AvailabilityMessage.model_validate(payload)
        device_repository.set_availability(
            topic_device_id, message.status, received_at
        )


def on_connect(client, userdata, flags, reason_code, properties):
    logger.info("Connected with result code: %s", reason_code)
    topics = [
        (settings.MQTT_TOPIC, 0),
        (settings.MQTT_TOPIC_PREFIX.strip("/") + "/+/telemetry", 0),
        (settings.MQTT_TOPIC_PREFIX.strip("/") + "/+/health", 0),
        (settings.MQTT_TOPIC_PREFIX.strip("/") + "/+/availability", 0),
    ]
    client.subscribe(topics)
    logger.info("Subscribed to legacy and per-device topics")


def on_message(client, userdata, msg):
    try:
        process_message(msg.topic, msg.payload)
    except (ValueError, json.JSONDecodeError, UnicodeDecodeError, ValidationError):
        logger.exception("Rejected invalid MQTT message topic=%s", msg.topic)
    except Exception:
        logger.exception("Failed to process MQTT message topic=%s", msg.topic)


def main():
    initialize_database()
    logger.info("Subscriber started")
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(settings.MQTT_HOST, settings.MQTT_PORT, 60)
    client.loop_forever()


if __name__ == "__main__":
    main()
