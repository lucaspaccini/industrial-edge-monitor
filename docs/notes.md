# START PROJECT

## API
- source .venv/bin/activate
- uvicorn backend.api.main:app
- http://127.0.0.1:8000/health
- http://127.0.0.1:8000/telemetry/
- http://127.0.0.1:8000/telemetry/latest

## COLLECTOR
- source .venv/bin/activate
- python -m backend.collector.subscriber

## PUBLISHER
- source .venv/bin/activate
- python -m backend.simulator.publisher

## FRONTEND
- cd frontend
- npm run dev

# FIRMWARE

## ESP-IDF
- source ~/esp/esp-idf/export.sh

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