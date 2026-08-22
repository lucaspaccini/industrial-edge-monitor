#!/usr/bin/env bash
set -Eeuo pipefail

umask 077

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

scripts/check-host-ports.py 3000 8000 8883

project_name="${COMPOSE_PROJECT_NAME:-iem-security-smoke-$$}"
security_parent="$(mktemp -d /tmp/iem-security-smoke.XXXXXX)"
security_dir="$security_parent/material"
network_name="${project_name}_edge"
lwt_container="${project_name}-lwt-test"
export COMPOSE_PROJECT_NAME="$project_name"
export MQTT_SECURITY_DIR="$security_dir"

cleanup() {
  local status=$?
  trap - EXIT
  docker rm --force "$lwt_container" >/dev/null 2>&1 || true
  if [[ "$status" -ne 0 ]]; then
    docker compose ps || true
    docker compose logs --no-color mqtt api collector frontend || true
  fi
  docker compose down --volumes --remove-orphans >/dev/null 2>&1 || true
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

mqtt_sub() {
  timeout 15s docker run --rm \
    --network "$network_name" \
    --volume "$security_dir:/run/mqtt-security:ro" \
    --entrypoint mosquitto_sub \
    eclipse-mosquitto:2.1.2-alpine "$@"
}

wait_for_json() {
  local url="$1"
  local assertion="$2"
  local response

  for _attempt in {1..30}; do
    if response="$(curl --fail --silent --show-error "$url" 2>/dev/null)" \
        && python3 -c "$assertion" "$response"; then
      printf '%s' "$response"
      return 0
    fi
    sleep 1
  done
  return 1
}

assert_shared_sqlite_volume() {
  local api_container collector_container api_volume collector_volume
  api_container="$(docker compose ps -q api)"
  collector_container="$(docker compose ps -q collector)"
  test -n "$api_container"
  test -n "$collector_container"

  api_volume="$(docker inspect --format \
    '{{range .Mounts}}{{if and (eq .Destination "/data") (eq .Type "volume")}}{{.Name}}{{end}}{{end}}' \
    "$api_container")"
  collector_volume="$(docker inspect --format \
    '{{range .Mounts}}{{if and (eq .Destination "/data") (eq .Type "volume")}}{{.Name}}{{end}}{{end}}' \
    "$collector_container")"
  test -n "$api_volume"
  test "$api_volume" = "$collector_volume"
}

scripts/generate-mqtt-security.sh --output "$security_dir"
docker compose config --quiet
docker compose up --detach --wait --wait-timeout 180
echo "Smoke stage: stack healthy"

for service in mqtt api collector frontend; do
  container_id="$(docker compose ps -q "$service")"
  runtime_uid="$(docker exec "$container_id" sh -c \
    "awk '/^Uid:/{print \$2}' /proc/1/status")"
  if [[ -z "$runtime_uid" || "$runtime_uid" = "0" ]]; then
    echo "$service is unexpectedly running as root" >&2
    exit 1
  fi
done
for image in industrial-edge-monitor-backend:local industrial-edge-monitor-frontend:local; do
  if docker history --no-trunc --format '{{.CreatedBy}}' "$image" \
      | grep -E 'BEGIN (RSA |EC |)PRIVATE KEY|MQTT_PASSWORD='; then
    echo "Sensitive material marker appeared in image history: $image" >&2
    exit 1
  fi
done
echo "Smoke stage: services non-root and image history free of secret markers"

curl --fail --silent --show-error http://127.0.0.1:8000/health >/dev/null
curl --fail --silent --show-error http://127.0.0.1:3000/ >/dev/null
assert_shared_sqlite_volume

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
echo "Smoke stage: collector subscribed"

if mqtt_pub \
    --host mqtt --port 8883 --id anonymous-test \
    --cafile /run/mqtt-security/ca/ca.crt \
    --topic industrial/telemetry --message anonymous; then
  echo "Anonymous MQTT connection was unexpectedly accepted" >&2
  exit 1
fi
echo "Smoke stage: anonymous client rejected"

if mqtt_pub \
    --host mqtt --port 8883 --id wrong-password-test \
    --cafile /run/mqtt-security/ca/ca.crt \
    --username edge-node-01 --pw definitely-wrong \
    --topic industrial/devices/edge-node-01/telemetry --message rejected; then
  echo "Invalid MQTT credentials were unexpectedly accepted" >&2
  exit 1
fi
echo "Smoke stage: invalid password rejected"

openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
  -subj "/CN=Untrusted Test CA" \
  -keyout "$security_parent/untrusted.key" \
  -out "$security_parent/untrusted.crt" >/dev/null 2>&1
sed 's#/run/mqtt-security/ca/ca.crt#/run/mqtt-security/untrusted.crt#' \
  "$security_dir/clients/edge-node-01.container.conf" \
  >"$security_dir/clients/untrusted-ca.container.conf"
cp "$security_parent/untrusted.crt" "$security_dir/untrusted.crt"
chmod 0600 "$security_dir/clients/untrusted-ca.container.conf"

if mqtt_pub -o /run/mqtt-security/clients/untrusted-ca.container.conf \
    --topic industrial/devices/edge-node-01/telemetry --message rejected; then
  echo "Untrusted broker certificate was unexpectedly accepted" >&2
  exit 1
fi
echo "Smoke stage: untrusted CA rejected"

broker_container="$(docker compose ps -q mqtt)"
broker_name="$(docker inspect --format '{{.Name}}' "$broker_container")"
broker_name="${broker_name#/}"
sed "s/^--host mqtt$/--host $broker_name/" \
  "$security_dir/clients/edge-node-01.container.conf" \
  >"$security_dir/clients/wrong-host.container.conf"
chmod 0600 "$security_dir/clients/wrong-host.container.conf"

if mqtt_pub -o /run/mqtt-security/clients/wrong-host.container.conf \
    --topic industrial/devices/edge-node-01/telemetry --message rejected; then
  echo "Broker hostname mismatch was unexpectedly accepted" >&2
  exit 1
fi
echo "Smoke stage: hostname mismatch rejected"

cross_device_result="$(mqtt_pub \
  -o /run/mqtt-security/clients/edge-node-01.container.conf \
  --protocol-version mqttv5 --qos 1 \
  --topic industrial/devices/edge-node-02/telemetry \
  --message rejected 2>&1 || true)"
grep --quiet "Not authorized" <<<"$cross_device_result"
unset cross_device_result
echo "Smoke stage: cross-device publish rejected"

wildcard_result="$(mqtt_sub \
  -o /run/mqtt-security/clients/edge-node-01.container.conf \
  --protocol-version mqttv5 \
  --topic 'industrial/devices/#' -W 2 2>&1 || true)"
if ! grep -Eq "Not authorized|All subscription requests were denied" \
    <<<"$wildcard_result"; then
  echo "Unexpected wildcard-subscription result: $wildcard_result" >&2
  exit 1
fi
unset wildcard_result
echo "Smoke stage: device wildcard subscription rejected"

collector_write_result="$(mqtt_pub \
  -o /run/mqtt-security/clients/collector.container.conf \
  --protocol-version mqttv5 --qos 1 \
  --topic industrial/devices/collector/telemetry \
  --message rejected 2>&1 || true)"
grep --quiet "Not authorized" <<<"$collector_write_result"
unset collector_write_result
echo "Smoke stage: collector write rejected"

telemetry_payload='{"device_id":"edge-node-01","timestamp":"2026-08-10T12:00:00Z","temperature":23.75,"humidity":45.5,"machine_status":"unknown"}'
mqtt_pub \
  -o /run/mqtt-security/clients/edge-node-01.container.conf \
  --topic industrial/devices/edge-node-01/telemetry \
  --message "$telemetry_payload"

telemetry_json="$(wait_for_json \
  'http://127.0.0.1:8000/telemetry/latest?device_id=edge-node-01' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["device_id"]=="edge-node-01" and item["temperature"]==23.75 and item["humidity"]==45.5')"
telemetry_id="$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["id"])' "$telemetry_json")"
test -n "$telemetry_id"
echo "Smoke stage: TLS telemetry persisted"

invalid_before="$(curl --fail --silent --show-error \
  'http://127.0.0.1:8000/telemetry/?device_id=edge-node-02&limit=100')"
python3 -c 'import json,sys; assert json.loads(sys.argv[1]) == []' "$invalid_before"
invalid_payload='{"device_id":"edge-node-02","timestamp":"2026-08-10T12:00:00","temperature":"99","humidity":true,"machine_status":"running"}'
mqtt_pub \
  -o /run/mqtt-security/clients/edge-node-02.container.conf \
  --topic industrial/devices/edge-node-02/telemetry \
  --message "$invalid_payload"
invalid_rejected=0
for _attempt in {1..30}; do
  if docker compose logs collector 2>&1 \
      | grep --quiet 'Rejected invalid MQTT message topic=industrial/devices/edge-node-02/telemetry'; then
    invalid_rejected=1
    break
  fi
  sleep 1
done
test "$invalid_rejected" -eq 1
invalid_after="$(curl --fail --silent --show-error \
  'http://127.0.0.1:8000/telemetry/?device_id=edge-node-02&limit=100')"
python3 -c 'import json,sys; assert json.loads(sys.argv[1]) == []' "$invalid_after"
echo "Smoke stage: invalid telemetry rejected without persistence"

health_payload='{"schema_version":1,"device_id":"edge-node-01","timestamp":"2026-08-10T12:00:01Z","status":"healthy","availability":"online","components":{},"counters":{"samples_ok":1},"metrics":{"wifi_rssi_dbm":null}}'
mqtt_pub \
  -o /run/mqtt-security/clients/edge-node-01.container.conf \
  --retain --topic industrial/devices/edge-node-01/health \
  --message "$health_payload"
mqtt_pub \
  -o /run/mqtt-security/clients/edge-node-01.container.conf \
  --retain --topic industrial/devices/edge-node-01/availability \
  --message '{"schema_version":1,"device_id":"edge-node-01","status":"online"}'

wait_for_json \
  'http://127.0.0.1:8000/devices/edge-node-01/health' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["device_id"]=="edge-node-01" and item["reported_availability"]=="online"' \
  >/dev/null
echo "Smoke stage: retained health and availability accepted"

docker run --detach --name "$lwt_container" \
  --network "$network_name" \
  --volume "$security_dir:/run/mqtt-security:ro" \
  --entrypoint mosquitto_pub \
  eclipse-mosquitto:2.1.2-alpine \
  -o /run/mqtt-security/clients/edge-node-01.container.conf \
  --id edge-node-01-lwt-test \
  --will-topic industrial/devices/edge-node-01/availability \
  --will-payload '{"schema_version":1,"device_id":"edge-node-01","status":"offline"}' \
  --will-retain \
  --repeat 100 --repeat-delay 10 \
  --topic industrial/devices/edge-node-01/telemetry \
  --message "$telemetry_payload" >/dev/null
sleep 2
docker kill "$lwt_container" >/dev/null
docker rm "$lwt_container" >/dev/null

wait_for_json \
  'http://127.0.0.1:8000/devices/edge-node-01/health' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["reported_availability"]=="offline" and item["availability"]=="offline"' \
  >/dev/null
echo "Smoke stage: Last Will marked device offline"

mqtt_pub \
  -o /run/mqtt-security/clients/edge-node-01.container.conf \
  --retain --topic industrial/devices/edge-node-01/availability \
  --message '{"schema_version":1,"device_id":"edge-node-01","status":"online"}'

legacy_payload='{"timestamp":"2026-08-10T12:00:02Z","temperature":21.5,"humidity":50.0,"machine_status":"unknown"}'
mqtt_pub \
  -o /run/mqtt-security/clients/simulator.container.conf \
  --topic industrial/telemetry --message "$legacy_payload"
wait_for_json \
  'http://127.0.0.1:8000/telemetry/latest?device_id=legacy-device' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["device_id"]=="legacy-device" and item["temperature"]==21.5' \
  >/dev/null
echo "Smoke stage: legacy simulator identity accepted"

pre_recreation_json="$(curl --fail --silent --show-error \
  'http://127.0.0.1:8000/telemetry/latest?device_id=edge-node-01')"
pre_recreation_id="$(python3 -c \
  'import json,sys; print(json.loads(sys.argv[1])["id"])' \
  "$pre_recreation_json")"
test -n "$pre_recreation_id"

docker compose stop api collector
docker compose rm --force api collector
docker compose up --detach api collector

recreated_ready=0
for _attempt in {1..30}; do
  if curl --fail --silent --show-error http://127.0.0.1:8000/health >/dev/null 2>&1 \
      && docker compose logs collector 2>&1 \
        | grep --quiet "Subscribed to legacy and per-device topics"; then
    recreated_ready=1
    break
  fi
  sleep 1
done
if [[ "$recreated_ready" -ne 1 ]]; then
  echo "Recreated API/collector did not become ready" >&2
  docker compose logs --no-color api collector >&2 || true
  exit 1
fi
assert_shared_sqlite_volume

persisted_json="$(curl --fail --silent --show-error \
  'http://127.0.0.1:8000/telemetry/latest?device_id=edge-node-01')"
persisted_id="$(python3 -c \
  'import json,sys; print(json.loads(sys.argv[1])["id"])' \
  "$persisted_json")"
if [[ "$persisted_id" != "$pre_recreation_id" ]]; then
  echo "Persistence ID mismatch expected=$pre_recreation_id actual=$persisted_id" >&2
  exit 1
fi
python3 -c \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["id"]==int(sys.argv[2]) and item["device_id"]=="edge-node-01" and item["temperature"]==23.75' \
  "$persisted_json" "$pre_recreation_id"
echo "Smoke stage: SQLite persisted across recreation"

wait_for_json \
  'http://127.0.0.1:8000/devices/edge-node-01/health' \
  'import json,sys; item=json.loads(sys.argv[1]); assert item["reported_availability"]=="online"' \
  >/dev/null

if docker compose logs --no-color \
    | grep -E 'BEGIN (RSA |EC |)PRIVATE KEY|MQTT_PASSWORD='; then
  echo "Sensitive material appeared in container logs" >&2
  exit 1
fi

echo "Secure Compose smoke test passed."
