#!/usr/bin/env bash
set -Eeuo pipefail

umask 077

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

scripts/check-host-ports.py 8000 8883

if [[ -n "${COMPOSE_PROJECT_NAME:-}" ]]; then
  project_name="${COMPOSE_PROJECT_NAME}-multi"
else
  project_name="iem-multi-device-smoke-$$"
fi
security_parent="$(mktemp -d /tmp/iem-multi-device-smoke.XXXXXX)"
security_dir="$security_parent/material"
network_name="${project_name}_edge"
export COMPOSE_PROJECT_NAME="$project_name"
export MQTT_SECURITY_DIR="$security_dir"
export DEVICE_OFFLINE_TIMEOUT_SECONDS=300
export SIMULATOR_INTERVAL_SECONDS=1
export SIMULATOR_TEMPERATURE=42
export SIMULATOR_HUMIDITY=61

cleanup() {
  local status=$?
  trap - EXIT
  if [[ "$status" -ne 0 ]]; then
    docker compose --profile demo ps || true
    docker compose --profile demo logs --no-color mqtt api collector simulator || true
  fi
  docker compose --profile demo down --volumes --remove-orphans >/dev/null 2>&1 || true
  rm -rf -- "$security_parent"
  exit "$status"
}
trap cleanup EXIT

mqtt_pub() {
  timeout 15s docker run --rm \
    --network "$network_name" \
    --volume "$security_dir:/run/mqtt-security:ro" \
    --entrypoint mosquitto_pub \
    eclipse-mosquitto:2.1.2-alpine "$@"
}

wait_for_json() {
  local url="$1"
  local assertion="$2"
  local response

  for _attempt in {1..45}; do
    if response="$(curl --fail --silent --show-error "$url" 2>/dev/null)" \
        && python3 -c "$assertion" "$response" 2>/dev/null; then
      printf '%s' "$response"
      return 0
    fi
    sleep 1
  done
  return 1
}

scripts/generate-mqtt-security.sh --output "$security_dir" >/dev/null
docker compose --profile demo config --quiet
docker compose --profile demo up --detach --build --wait --wait-timeout 180 \
  mqtt api collector simulator

collector_ready=0
for _attempt in {1..30}; do
  if docker compose logs collector 2>&1 \
      | grep --quiet "Subscribed to legacy and per-device topics"; then
    collector_ready=1
    break
  fi
  sleep 1
done
test "$collector_ready" -eq 1

edge_one_telemetry='{"device_id":"edge-node-01","timestamp":"2026-08-22T10:00:00Z","temperature":22.5,"humidity":48.0,"machine_status":"running"}'
edge_one_health='{"schema_version":1,"device_id":"edge-node-01","timestamp":"2026-08-22T10:00:01Z","status":"healthy","availability":"online","components":{"sensor":{"status":"healthy","error_code":"none","updated_at":"2026-08-22T10:00:01Z"}},"counters":{"samples_ok":1},"metrics":{}}'
mqtt_pub -o /run/mqtt-security/clients/edge-node-01.container.conf \
  --topic industrial/devices/edge-node-01/telemetry --message "$edge_one_telemetry"
mqtt_pub -o /run/mqtt-security/clients/edge-node-01.container.conf --retain \
  --topic industrial/devices/edge-node-01/health --message "$edge_one_health"
mqtt_pub -o /run/mqtt-security/clients/edge-node-01.container.conf --retain \
  --topic industrial/devices/edge-node-01/availability \
  --message '{"schema_version":1,"device_id":"edge-node-01","status":"online"}'

wait_for_json \
  'http://127.0.0.1:8000/devices/' \
  'import json,sys; items=json.loads(sys.argv[1]); ids={x["device_id"] for x in items}; assert {"edge-node-01","edge-node-02"} <= ids' >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/telemetry/?device_id=edge-node-01&limit=10' \
  'import json,sys; items=json.loads(sys.argv[1]); assert items and {x["device_id"] for x in items}=={"edge-node-01"}' >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/telemetry/?device_id=edge-node-02&limit=10' \
  'import json,sys; items=json.loads(sys.argv[1]); assert items and {x["device_id"] for x in items}=={"edge-node-02"}' >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/telemetry/statistics?device_id=edge-node-01' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["samples"]==1 and item["temperature"]["max"]==22.5' >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/telemetry/statistics?device_id=edge-node-02' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["samples"]>=1 and item["temperature"]["max"]==42.0' >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/devices/edge-node-01/health' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["availability"]=="online" and "sensor" in item["components"]' >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/devices/edge-node-02/health' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["availability"]=="online" and set(item["components"])=={"simulator"}' >/dev/null
echo "Multi-device smoke: device registry, telemetry, health and statistics isolated"

first_rule_id="$(scripts/ensure-demo-alert-rule.py --id-only)"
second_rule_id="$(scripts/ensure-demo-alert-rule.py --id-only)"
test "$first_rule_id" = "$second_rule_id"
curl --fail --silent --show-error -X PATCH \
  -H 'Content-Type: application/json' \
  -d '{"enabled":false}' \
  "http://127.0.0.1:8000/alert-rules/$first_rule_id" >/dev/null
third_rule_id="$(scripts/ensure-demo-alert-rule.py --id-only)"
test "$first_rule_id" = "$third_rule_id"
wait_for_json \
  'http://127.0.0.1:8000/alert-rules/?device_id=edge-node-02' \
  'import json,sys; items=json.loads(sys.argv[1]); demo=[x for x in items if x["name"]=="Portfolio demo: edge-node-02 high temperature"]; assert len(demo)==1 and demo[0]["enabled"] is True' >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/alerts/active?device_id=edge-node-02' \
  'import json,sys; items=json.loads(sys.argv[1]); assert len(items)==1 and items[0]["device_id"]=="edge-node-02"' >/dev/null
edge_one_alerts="$(curl --fail --silent --show-error \
  'http://127.0.0.1:8000/alerts/active?device_id=edge-node-01')"
python3 -c 'import json,sys; assert json.loads(sys.argv[1]) == []' "$edge_one_alerts"
echo "Multi-device smoke: edge-node-02 alert does not affect edge-node-01"

docker compose --profile demo kill --signal SIGKILL simulator >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/devices/edge-node-02/health' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["availability"]=="offline" and item["reported_availability"]=="offline"' >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/devices/edge-node-01/health' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["availability"]=="online"' >/dev/null
echo "Multi-device smoke: edge-node-02 LWT offline is isolated"

docker compose --profile demo start simulator >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/devices/edge-node-02/health' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["availability"]=="online" and item["reported_availability"]=="online"' >/dev/null
echo "Multi-device smoke: edge-node-02 restart returned online"

simulator_container="$(docker compose --profile demo ps -q simulator)"
test "$(docker inspect --format '{{.Config.User}}' "$simulator_container")" = "app"
docker compose --profile demo stop simulator >/dev/null
wait_for_json \
  'http://127.0.0.1:8000/devices/edge-node-02/health' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["availability"]=="offline" and item["reported_availability"]=="offline"' >/dev/null
echo "Multi-device smoke: graceful shutdown published retained offline"

if docker compose --profile demo logs --no-color \
    | grep -E 'BEGIN (RSA |EC |)PRIVATE KEY|MQTT_PASSWORD='; then
  echo "Sensitive material appeared in demo logs" >&2
  exit 1
fi

echo "Isolated multi-device demo smoke passed."
