import json
import random
import signal
import ssl
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt

from backend.core.config import settings
from backend.core.logging import configure_logging, get_logger
from backend.mqtt.client import connection_error_category, create_mqtt_client


configure_logging()
logger = get_logger(__name__)


def _payload() -> dict[str, object]:
    return {
        "timestamp": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "temperature": round(random.uniform(20, 35), 1),
        "humidity": round(random.uniform(40, 70), 1),
        "machine_status": random.choice(["running", "stopped", "unknown"]),
    }


def main() -> None:
    client = create_mqtt_client()
    stopping = False

    def stop_client(signum, _frame):
        nonlocal stopping
        stopping = True
        logger.info("Stopping simulator after signal=%s", signum)

    signal.signal(signal.SIGINT, stop_client)
    signal.signal(signal.SIGTERM, stop_client)

    try:
        client.connect(
            settings.MQTT_HOST,
            settings.MQTT_PORT,
            settings.MQTT_KEEPALIVE_SECONDS,
        )
    except (ssl.SSLError, OSError) as exc:
        logger.error(
            "MQTT simulator connection failed category=%s",
            connection_error_category(exc),
        )
        raise SystemExit(1) from exc

    client.loop_start()
    try:
        while not stopping:
            payload = _payload()
            result = client.publish(settings.MQTT_TOPIC, json.dumps(payload))
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                logger.info("Published simulated telemetry")
            else:
                logger.warning("Simulator publish skipped result=%s", result.rc)
            time.sleep(2)
    finally:
        client.disconnect()
        client.loop_stop()
        logger.info("Simulator stopped")


if __name__ == "__main__":
    main()
