#!/usr/bin/env bash
set -Eeuo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
security_root="$(mktemp -d /tmp/iem-device-lifecycle.XXXXXX)"
bundle="$security_root/mqtt-security"
package="$security_root/edge-node-03.provisioning.json"
old_options="$security_root/old.conf"
new_options="$security_root/new.conf"
suffix="$$"
network="iem-device-lifecycle-$suffix"
broker="iem-device-lifecycle-broker-$suffix"
volume="iem-device-lifecycle-data-$suffix"
image="${MOSQUITTO_IMAGE:-eclipse-mosquitto:2.1.2-alpine}"

cleanup() {
  docker rm --force "$broker" >/dev/null 2>&1 || true
  docker network rm "$network" >/dev/null 2>&1 || true
  docker volume rm "$volume" >/dev/null 2>&1 || true
  rm -rf -- "$security_root"
}
trap cleanup EXIT

start_broker() {
  docker rm --force "$broker" >/dev/null 2>&1 || true
  docker run --detach --name "$broker" \
    --network "$network" --network-alias mqtt \
    --volume "$repo_root/docker/mosquitto/mosquitto.conf:/mosquitto/config/mosquitto.conf:ro" \
    --volume "$bundle/ca/ca.crt:/run/mqtt-security/ca/ca.crt:ro" \
    --volume "$bundle/server/server.crt:/run/mqtt-security/server/server.crt:ro" \
    --volume "$bundle/server/server.key:/run/mqtt-security/server/server.key:ro" \
    --volume "$bundle/mosquitto/dynamic-security.json:/run/mqtt-security/mosquitto/dynamic-security.json" \
    --volume "$bundle/clients/healthcheck.container.conf:/run/mqtt-security/clients/healthcheck.container.conf:ro" \
    --volume "$volume:/mosquitto/data" \
    "$image" >/dev/null
  for _attempt in $(seq 1 20); do
    if docker exec "$broker" mosquitto_pub \
      -o /run/mqtt-security/clients/healthcheck.container.conf \
      -t industrial/healthcheck -m ready >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  docker logs "$broker" >&2
  return 1
}

publish_with_options() {
  local options="$1"
  docker run --rm --network "$network" \
    --volume "$bundle/ca/ca.crt:/run/mqtt-security/ca/ca.crt:ro" \
    --volume "$options:/device.conf:ro" \
    --entrypoint mosquitto_pub "$image" \
    -o /device.conf \
    -t industrial/devices/edge-node-03/telemetry \
    -m lifecycle-smoke >/dev/null 2>&1
}

"$repo_root/scripts/generate-mqtt-security.sh" --output "$bundle" >/dev/null
docker network create "$network" >/dev/null
docker volume create "$volume" >/dev/null

"$repo_root/scripts/manage-mqtt-device.py" --bundle "$bundle" add edge-node-03 \
  --broker-uri mqtts://192.0.2.10:8883 --output "$package" >/dev/null
test "$(stat -c '%a' "$package")" = "600"
cp "$bundle/clients/edge-node-03.container.conf" "$old_options"
start_broker
publish_with_options "$old_options"
echo "Lifecycle smoke: added identity accepted"

"$repo_root/scripts/manage-mqtt-device.py" --bundle "$bundle" rotate edge-node-03 \
  --broker-uri mqtts://192.0.2.10:8883 --output "$package" >/dev/null
cp "$bundle/clients/edge-node-03.container.conf" "$new_options"
start_broker
if publish_with_options "$old_options"; then
  echo "Rotated identity accepted its previous password" >&2
  exit 21
fi
publish_with_options "$new_options"
echo "Lifecycle smoke: previous password rejected and rotated identity accepted"

"$repo_root/scripts/manage-mqtt-device.py" --bundle "$bundle" revoke edge-node-03 >/dev/null
start_broker
if publish_with_options "$new_options"; then
  echo "Revoked identity was still able to publish" >&2
  exit 22
fi
echo "Lifecycle smoke: revoked identity rejected"
