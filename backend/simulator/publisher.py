import json
import random
import time
from datetime import datetime

import paho.mqtt.client as mqtt

BROKER = "localhost"
PORT = 1883
TOPIC = "factory/line1/machine1"

client = mqtt.Client()

client.connect(BROKER,PORT)

while True:

    payload = {
            "timestamp": datetime.utcnow().isoformat(),
            "temperature": round(random.uniform(20,35), 1),
            "humidity": round(random.uniform(40,70), 1),
            "machine_status": random.choice(["running", "idle", "alarm"])
            }

    client.publish(
            TOPIC,
            json.dumps(payload)
            )

    print(payload)

    time.sleep(2)

