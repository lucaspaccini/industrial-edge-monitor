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

## Run the REST API

```bash
uvicorn backend.api.main:app --reload
```