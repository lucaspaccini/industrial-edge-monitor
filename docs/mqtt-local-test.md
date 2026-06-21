# MQTT Local TEst

## Goal

Verity that the local Mosquitto MQTT broker is running correctly on Ubuntu.

## Subscriber

```bash
mosquitto_sub -h localhost -t test/topic
```

## Publisher

```bash
mosquitto_pub -h localhost -t test/topic -m "hello mqtt"
```

## Expected result

The subscriber receives: hello mqtt

