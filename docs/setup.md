# Setup

## Clone repository

```bash
git clone <repository>
cd industrial-edge-monitor
```

## Create virtual environment

```bash
python -m venv .venv
source .venv/bin/activate
```

## Install dependencies

```bash
pip install -r requirements.txt
```

## Start Mosquitto

```bash
sudo systemctl start mosquitto
```

## Run the simulator

```bash
python backend/simulator/publisher.py
```

## Run the collector

```bash
python backend/collector/subscriber.py
```

The collector subscribes to the legacy topic and to all per-device telemetry, health and availability topics. Database initialization performs migrations in place; do not delete `data/telemetry.db`.

The collector does not hot-reload Python modules. Restart `python -m backend.collector.subscriber` after updating collector, telemetry-service or alert-engine code.

## Run the REST API

```bash
uvicorn backend.api.main:app --reload
```
