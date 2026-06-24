import json
import sqlite3
import paho.mqtt.client as mqtt

DB_PATH = "data/telemetry.db"
TOPIC = "factory/line1/machine1"

def on_connect(client, userdata, flags, rc):
    print("Connected with result code:", rc)
    client.subscribe(TOPIC)
    print("Subscribed to: ", TOPIC)

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())

        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()

        cursor.execute(""" 
               CREATE TABLE IF NOT EXISTS telemetry
               (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp TEXT,
                    temperature REAL,
                    humidity REAL,
                    machine_status TEXT
               )
               """)

        cursor.execute("""
                       INSERT INTO telemetry (
                            timestamp,
                            temperature,
                            humidity,
                            machine_status
                       )
                       VALUES (?, ?, ?, ?)
                       """, (
                       payload["timestamp"],
                       payload["temperature"],
                       payload["humidity"],
                       payload["machine_status"]
                       ))
        conn.commit()
        conn.close()

        print("Saved: ", payload)
    except Exception as e:
        print("Error: ", e)

def main():
    print("Subscriber started")

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect("localhost", 1883, 60)

    print("MQTT subscriber started...")
    client.loop_forever()


if __name__ == "__main__":
    main()