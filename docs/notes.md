# START PROJECT

## API
- source .venv/bin/activate
- uvicorn backend.api.main:app
- http://127.0.0.1:8000/health
- http://127.0.0.1:8000/telemetry/
- http://127.0.0.1:8000/telemetry/latest
- http://127.0.0.1:8000/devices/
- http://127.0.0.1:8000/devices/edge-node-01/health

## COLLECTOR
- source .venv/bin/activate
- python -m backend.collector.subscriber

## PUBLISHER
- docker compose --profile demo up --detach simulator

## FRONTEND
- cd frontend
- npm run dev

# FIRMWARE

## ESP-IDF
- source ~/esp/esp-idf/export.sh
- idf.py fullclean
- idf.py build
- idf.py -p /dev/ttyUSB0 flash
- idf.py -p /dev/ttyUSB0 monitor

# GITHUB

## MESSAGE COMMIT ROULES

<type>(<scope>): <description>

Types:
- feat: new feature
- fix: bug fix
- refactor: architecture improvements / refactoring
- docs: documentation 
- test: test
- chore: tooling, configurations, dependencies

Example:
- feat(api): add telemetry statistics endpoint
- refactor(backend): consolidate shared architecture
- docs(roadmap): update project roadmap
- test(repository): add telemetry repository tests
