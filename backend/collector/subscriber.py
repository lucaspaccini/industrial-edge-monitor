import json
import sqlite3
import paho.mqtt.client as mqtt

from backend.core.config import settings
from backend.services.telemetry_service import telemetry_service
from backend.core.logging import configure_logging, get_logger
from backend.database.init_db import initialize_database

configure_logging()
logger = get_logger(__name__)

initialize_database();

def on_connect(client, userdata, flags, rc):
    logger.info("Connected with result code: %d", rc)
    client.subscribe(settings.MQTT_TOPIC)
    logger.info("Subscribed to: %s", settings.MQTT_TOPIC)

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())

        telemetry_service.save_telemetry(payload)

        logger.info("Telemetry saved successfully: %s", payload)

    except Exception as exc:
        logger.exception("Failed to process telemetry message")

def main():
    logger.info("Subscriber started")

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(
        settings.MQTT_HOST,
        settings.MQTT_PORT,
        60,
    )

    logger.info("MQTT subscriber started...")
    client.loop_forever()


if __name__ == "__main__":
    main()