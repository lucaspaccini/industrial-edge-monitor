import json
import signal
import ssl
import threading
from datetime import datetime, timezone

import paho.mqtt.client as mqtt

from backend.core.config import Settings, settings
from backend.core.logging import configure_logging, get_logger
from backend.mqtt.client import connection_error_category, create_mqtt_client


configure_logging()
logger = get_logger(__name__)


def _now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace(
        "+00:00", "Z"
    )


def _topics(config: Settings = settings) -> dict[str, str]:
    root = f"{config.MQTT_TOPIC_PREFIX.strip('/')}/{config.SIMULATOR_DEVICE_ID}"
    return {
        "telemetry": f"{root}/telemetry",
        "health": f"{root}/health",
        "availability": f"{root}/availability",
    }


def _availability_payload(config: Settings, status: str) -> str:
    return json.dumps(
        {
            "schema_version": 1,
            "device_id": config.SIMULATOR_DEVICE_ID,
            "status": status,
        },
        separators=(",", ":"),
    )


def _telemetry_payload(config: Settings) -> str:
    return json.dumps(
        {
            "device_id": config.SIMULATOR_DEVICE_ID,
            "timestamp": _now(),
            "temperature": config.SIMULATOR_TEMPERATURE,
            "humidity": config.SIMULATOR_HUMIDITY,
            "machine_status": config.SIMULATOR_MACHINE_STATUS,
        },
        separators=(",", ":"),
    )


def _health_payload(config: Settings, samples_published: int) -> str:
    timestamp = _now()
    return json.dumps(
        {
            "schema_version": 1,
            "device_id": config.SIMULATOR_DEVICE_ID,
            "timestamp": timestamp,
            "status": "healthy",
            "availability": "online",
            "components": {
                "simulator": {
                    "status": "healthy",
                    "error_code": "none",
                    "updated_at": timestamp,
                }
            },
            "counters": {"samples_ok": samples_published},
            "metrics": {},
        },
        separators=(",", ":"),
    )


def _publish(client: mqtt.Client, topic: str, payload: str, *, retain: bool) -> None:
    result = client.publish(topic, payload, qos=1, retain=retain)
    if result.rc != mqtt.MQTT_ERR_SUCCESS:
        raise RuntimeError(f"MQTT publish was not queued for topic {topic}")
    result.wait_for_publish(timeout=10)
    if not result.is_published():
        raise RuntimeError(f"MQTT publish did not complete for topic {topic}")


def main() -> None:
    config = settings
    topics = _topics(config)
    client = create_mqtt_client(config)
    connected = threading.Event()
    stopping = threading.Event()
    samples_published = 0

    client.will_set(
        topics["availability"],
        _availability_payload(config, "offline"),
        qos=1,
        retain=True,
    )

    def on_connect(_client, _userdata, _flags, reason_code, _properties):
        if reason_code.is_failure:
            logger.error("Simulator MQTT connection rejected reason=%s", reason_code)
            return
        online_result = _client.publish(
            topics["availability"],
            _availability_payload(config, "online"),
            qos=1,
            retain=True,
        )
        health_result = _client.publish(
            topics["health"],
            _health_payload(config, samples_published),
            qos=1,
            retain=True,
        )
        if any(
            result.rc != mqtt.MQTT_ERR_SUCCESS
            for result in (online_result, health_result)
        ):
            logger.error("Simulator failed to queue retained online state")
            return
        connected.set()
        logger.info("Simulator device online device_id=%s", config.SIMULATOR_DEVICE_ID)

    def stop_client(signum, _frame):
        logger.info("Stopping simulator gracefully after signal=%s", signum)
        stopping.set()

    client.on_connect = on_connect
    signal.signal(signal.SIGINT, stop_client)
    signal.signal(signal.SIGTERM, stop_client)

    try:
        client.connect(
            config.MQTT_HOST,
            config.MQTT_PORT,
            config.MQTT_KEEPALIVE_SECONDS,
        )
    except (ssl.SSLError, OSError) as exc:
        logger.error(
            "MQTT simulator connection failed category=%s",
            connection_error_category(exc),
        )
        raise SystemExit(1) from exc

    client.loop_start()
    try:
        if not connected.wait(timeout=15):
            raise RuntimeError("Simulator MQTT connection did not become ready")
        while not stopping.is_set():
            _publish(
                client,
                topics["telemetry"],
                _telemetry_payload(config),
                retain=False,
            )
            samples_published += 1
            _publish(
                client,
                topics["health"],
                _health_payload(config, samples_published),
                retain=True,
            )
            logger.info(
                "Published simulator sample device_id=%s sequence=%s",
                config.SIMULATOR_DEVICE_ID,
                samples_published,
            )
            stopping.wait(config.SIMULATOR_INTERVAL_SECONDS)
    finally:
        if connected.is_set():
            try:
                _publish(
                    client,
                    topics["availability"],
                    _availability_payload(config, "offline"),
                    retain=True,
                )
            except RuntimeError:
                logger.exception("Simulator graceful offline publication failed")
        client.disconnect()
        client.loop_stop()
        logger.info("Simulator stopped device_id=%s", config.SIMULATOR_DEVICE_ID)


if __name__ == "__main__":
    main()
