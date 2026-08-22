#!/usr/bin/env bash
set -Eeuo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if [[ "$#" -ne 1 ]]; then
  echo "usage: scripts/portfolio-demo-preflight.sh BROKER_HOST_OR_IP" >&2
  exit 2
fi

broker_host="$1"
bundle="${MQTT_SECURITY_DIR:-.local/mqtt-security}"

test -f .env || {
  echo ".env is missing; create it once with: test -f .env || cp .env.example .env" >&2
  exit 1
}
test -d "$bundle" || {
  echo "$bundle is missing; generate it only for first setup" >&2
  exit 1
}

if [[ -n "$(docker compose --profile demo ps --status running -q)" ]]; then
  echo "This Compose project is already running; stop it before preflight/smokes." >&2
  exit 1
fi

scripts/check-host-ports.py 3000 8000 8883
scripts/manage-mqtt-device.py --bundle "$bundle" normalize-permissions
bundle="$(realpath "$bundle")"

openssl verify -CAfile "$bundle/ca/ca.crt" "$bundle/server/server.crt" >/dev/null
openssl x509 -checkend 0 -noout -in "$bundle/server/server.crt"
if python3 -c 'import ipaddress,sys; ipaddress.ip_address(sys.argv[1])' "$broker_host" 2>/dev/null; then
  openssl x509 -checkip "$broker_host" -noout -in "$bundle/server/server.crt"
else
  openssl x509 -checkhost "$broker_host" -noout -in "$bundle/server/server.crt"
fi

test -f "$bundle/clients/edge-node-02.password" || {
  echo "edge-node-02 is not provisioned in the existing bundle." >&2
  exit 1
}
docker run --rm --user 10001:10001 \
  --volume "$bundle/clients/edge-node-02.password:/run/edge-node-02.password:ro" \
  --entrypoint test eclipse-mosquitto:2.1.2-alpine \
  -r /run/edge-node-02.password

python3 - <<'PY'
from pathlib import Path

compose = Path("compose.yaml").read_text(encoding="utf-8")
simulator = compose[compose.index("  simulator:"):compose.index("  frontend:")]
assert "\n    profiles:\n      - demo\n" in simulator, (
    "simulator must remain behind the demo profile"
)
PY

echo "Portfolio demo preflight passed: environment, managed bundle, certificate, ports, profile and simulator credential readability."
