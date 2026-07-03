import json
import random
import time
from datetime import datetime

import paho.mqtt.client as mqtt

from backend.core.config import settings
from backend.core.logging import configure_logging, get_logger

configure_logging()
logger = get_logger(__name__)

client = mqtt.Client()

client.connect(settings.MQTT_HOST, settings.MQTT_PORT)

while True:

    payload = {
            "timestamp": datetime.utcnow().isoformat(),
            "temperature": round(random.uniform(20,35), 1),
            "humidity": round(random.uniform(40,70), 1),
            "machine_status": random.choice(["running", "idle", "alarm"])
            }

    client.publish(
            settings.MQTT_TOPIC,
            json.dumps(payload)
            )

    logger.info("Publishing telemetry: %s", payload)
    time.sleep(2)

