#!/usr/bin/env bash
set -Eeuo pipefail

umask 077

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="$repo_root/.local/mqtt-security"
mosquitto_image="${MOSQUITTO_IMAGE:-eclipse-mosquitto:2.1.2-alpine}"
bundle_version="1"
force=0
lan_hosts=()
devices=(edge-node-01 edge-node-02)
reserved_identities=(collector healthcheck simulator legacy-test)

usage() {
  echo "Usage: $0 [--output DIR] [--lan-host HOST_OR_IP] [--device DEVICE_ID] [--force]"
}

validate_device_id() {
  local device_id="$1"
  if ! python3 "$repo_root/scripts/mqtt_device_common.py" "$device_id" >/dev/null; then
    echo "Invalid device ID: $device_id" >&2
    exit 2
  fi
}

validate_output_path() {
  local path="$1"
  local parent
  local user_home_dir

  parent="$(dirname "$path")"
  user_home_dir="$(realpath -ms "${HOME:?HOME is not set}")"

  if [[ -L "$path" ]]; then
    echo "Refusing symlink output directory: $path" >&2
    exit 2
  fi

  case "$path" in
    /|/tmp|/var|/home|/usr|/etc|/opt|/run|/srv|/root|"$repo_root"|"$user_home_dir")
      echo "Refusing unsafe output directory: $path" >&2
      exit 2
      ;;
  esac

  if [[ "$repo_root" == "$path/"* || "$user_home_dir" == "$path/"* ]]; then
    echo "Refusing output directory that is an ancestor of a protected path: $path" >&2
    exit 2
  fi

  if [[ "$parent" == "/" ]]; then
    echo "Refusing excessively broad output directory: $path" >&2
    exit 2
  fi

  if [[ -e "$path" && ! -d "$path" ]]; then
    echo "Output path exists and is not a directory: $path" >&2
    exit 2
  fi
}

has_valid_marker() {
  local directory="$1"
  local marker="$directory/.generated-version"

  [[ -f "$marker" && ! -L "$marker" ]] \
    && [[ "$(<"$marker")" == "$bundle_version" ]]
}

while (($#)); do
  case "$1" in
    --output)
      output_dir="$2"
      shift 2
      ;;
    --lan-host)
      lan_hosts+=("$2")
      shift 2
      ;;
    --device)
      devices+=("$2")
      shift 2
      ;;
    --force)
      force=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

declare -A device_seen=()
for device_id in "${devices[@]}"; do
  validate_device_id "$device_id"

  if [[ -n "${device_seen[$device_id]+present}" ]]; then
    echo "Duplicate device ID: $device_id" >&2
    exit 2
  fi
  device_seen["$device_id"]=1
done

identities=("${reserved_identities[@]}" "${devices[@]}")

output_dir="$(realpath -ms "$output_dir")"
validate_output_path "$output_dir"

required_files=(
  ca/ca.crt
  server/server.crt
  server/server.key
  mosquitto/passwords
  mosquitto/dynamic-security.json
  clients/collector.password
  clients/healthcheck.container.conf
  .generated-version
)

if [[ -d "$output_dir" && "$force" -eq 1 ]] && ! has_valid_marker "$output_dir"; then
  echo "Refusing to replace an unmanaged bundle or invalid marker: $output_dir" >&2
  exit 1
fi

if [[ -d "$output_dir" && "$force" -eq 0 ]]; then
  if ! has_valid_marker "$output_dir"; then
    echo "Existing output is not a managed MQTT security bundle; no files changed." >&2
    exit 1
  fi

  complete=1
  for relative_path in "${required_files[@]}"; do
    [[ -f "$output_dir/$relative_path" ]] || complete=0
  done
  if [[ "$complete" -eq 1 ]]; then
    echo "MQTT security material already exists; no files changed."
    exit 0
  fi
  echo "Output directory is incomplete; inspect it and rerun with --force to replace it." >&2
  exit 1
fi

mkdir -p "$(dirname "$output_dir")"
staging_dir="$(mktemp -d "$(dirname "$output_dir")/.mqtt-security.tmp.XXXXXX")"
backup_dir=""
cleanup() {
  local status=$?
  trap - EXIT

  if [[ "$status" -ne 0 && -n "$backup_dir" && -d "$backup_dir" \
      && ! -e "$output_dir" && ! -L "$output_dir" ]]; then
    mv -- "$backup_dir" "$output_dir" || true
    backup_dir=""
  fi

  if [[ -n "$staging_dir" && -d "$staging_dir" ]]; then
    rm -rf -- "$staging_dir"
  fi

  exit "$status"
}
trap cleanup EXIT

mkdir -p \
  "$staging_dir/ca" \
  "$staging_dir/server" \
  "$staging_dir/mosquitto" \
  "$staging_dir/clients"

san_entries=("DNS:mqtt" "DNS:localhost" "IP:127.0.0.1")
for lan_host in "${lan_hosts[@]}"; do
  if python3 -c 'import ipaddress,sys; ipaddress.ip_address(sys.argv[1])' "$lan_host" 2>/dev/null; then
    san_entries+=("IP:$lan_host")
  elif [[ "$lan_host" =~ ^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$ ]]; then
    san_entries+=("DNS:$lan_host")
  else
    echo "Invalid LAN hostname or IP: $lan_host" >&2
    exit 2
  fi
done

san_list="$(IFS=,; echo "${san_entries[*]}")"
cat >"$staging_dir/server/extensions.cnf" <<EOF
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=$san_list
EOF

openssl req -x509 -newkey rsa:3072 -sha256 -nodes \
  -days 825 \
  -subj "/CN=Industrial Edge Monitor Local Development CA" \
  -addext "basicConstraints=critical,CA:TRUE,pathlen:0" \
  -addext "keyUsage=critical,keyCertSign,cRLSign" \
  -addext "subjectKeyIdentifier=hash" \
  -keyout "$staging_dir/ca/ca.key" \
  -out "$staging_dir/ca/ca.crt" >/dev/null 2>&1

openssl req -newkey rsa:3072 -sha256 -nodes \
  -subj "/CN=mqtt" \
  -keyout "$staging_dir/server/server.key" \
  -out "$staging_dir/server/server.csr" >/dev/null 2>&1

openssl x509 -req -sha256 \
  -days 397 \
  -in "$staging_dir/server/server.csr" \
  -CA "$staging_dir/ca/ca.crt" \
  -CAkey "$staging_dir/ca/ca.key" \
  -CAcreateserial \
  -extfile "$staging_dir/server/extensions.cnf" \
  -out "$staging_dir/server/server.crt" >/dev/null 2>&1

openssl verify -CAfile "$staging_dir/ca/ca.crt" \
  "$staging_dir/server/server.crt" >/dev/null
openssl x509 -in "$staging_dir/ca/ca.crt" -noout -text \
  | grep -A1 "Basic Constraints" \
  | grep -q "CA:TRUE"
openssl x509 -in "$staging_dir/server/server.crt" -noout -checkhost mqtt >/dev/null
openssl x509 -in "$staging_dir/server/server.crt" -noout -checkhost localhost >/dev/null
openssl x509 -in "$staging_dir/server/server.crt" -noout -checkip 127.0.0.1 >/dev/null

printf '' >"$staging_dir/mosquitto/passwords"
for identity in "${identities[@]}"; do
  password="$(openssl rand -hex 32)"
  printf '%s\n' "$password" >"$staging_dir/clients/$identity.password"
  printf '%s:%s\n' "$identity" "$password" >>"$staging_dir/mosquitto/passwords"

  cat >"$staging_dir/clients/$identity.container.conf" <<EOF
--host mqtt
--port 8883
--id iem-$identity-container
--cafile /run/mqtt-security/ca/ca.crt
--tls-version tlsv1.2
--username $identity
--pw $password
EOF
  cat >"$staging_dir/clients/$identity.host.conf" <<EOF
--host localhost
--port 8883
--id iem-$identity-host
--cafile $output_dir/ca/ca.crt
--tls-version tlsv1.2
--username $identity
--pw $password
EOF
done

cat >"$staging_dir/clients/healthcheck.container.conf" <<EOF
--host 127.0.0.1
--port 8883
--id iem-healthcheck-compose
--cafile /run/mqtt-security/ca/ca.crt
--tls-version tlsv1.2
--username healthcheck
--pw $(<"$staging_dir/clients/healthcheck.password")
EOF

docker run --rm --entrypoint sh \
  --volume "$staging_dir:/work" \
  "$mosquitto_image" \
  -c 'chown root:root /work/mosquitto/passwords; chmod 0600 /work/mosquitto/passwords; mosquitto_passwd -U /work/mosquitto/passwords' >/dev/null

local_uid="$(id -u)"
local_gid="$(id -g)"
docker run --rm --entrypoint sh \
  --volume "$staging_dir:/work" \
  "$mosquitto_image" \
  -c 'chown "$1:$2" /work/mosquitto/passwords' \
  sh "$local_uid" "$local_gid"

python3 "$repo_root/scripts/build_mosquitto_security.py" \
  --policy "$repo_root/docker/mosquitto/security-policy.json" \
  --password-file "$staging_dir/mosquitto/passwords" \
  --output "$staging_dir/mosquitto/dynamic-security.json"

printf '%s\n' "$bundle_version" >"$staging_dir/.generated-version"
rm -f \
  "$staging_dir/server/server.csr" \
  "$staging_dir/server/extensions.cnf" \
  "$staging_dir/ca/ca.srl"

chmod 0755 "$staging_dir" "$staging_dir/ca" "$staging_dir/server" \
  "$staging_dir/mosquitto" "$staging_dir/clients"
chmod 0644 "$staging_dir/ca/ca.crt" "$staging_dir/server/server.crt"
chmod 0600 "$staging_dir/ca/ca.key" "$staging_dir/server/server.key"
chmod 0600 \
  "$staging_dir/mosquitto/passwords" \
  "$staging_dir/mosquitto/dynamic-security.json" \
  "$staging_dir/clients/"*

docker run --rm --entrypoint sh \
  --volume "$staging_dir:/work" \
  "$mosquitto_image" \
  -c 'chown "1883:$1" /work/server/server.key /work/mosquitto/dynamic-security.json /work/clients/healthcheck.container.conf && chmod 0440 /work/server/server.key /work/clients/healthcheck.container.conf && chmod 0660 /work/mosquitto/dynamic-security.json && chown "10001:$1" /work/clients/collector.password && chmod 0440 /work/clients/collector.password' \
  sh "$local_gid"

for relative_path in "${required_files[@]}"; do
  if [[ ! -f "$staging_dir/$relative_path" ]]; then
    echo "Generated bundle is incomplete: $relative_path" >&2
    exit 1
  fi
done
if ! has_valid_marker "$staging_dir"; then
  echo "Generated bundle marker is invalid" >&2
  exit 1
fi

if [[ -d "$output_dir" ]]; then
  backup_dir="$(mktemp -d "$(dirname "$output_dir")/.mqtt-security.backup.XXXXXX")"
  rmdir -- "$backup_dir"
  mv -- "$output_dir" "$backup_dir"
fi

mv -- "$staging_dir" "$output_dir"
staging_dir=""

if [[ -n "$backup_dir" ]]; then
  rm -rf -- "$backup_dir"
  backup_dir=""
fi

trap - EXIT

echo "Generated MQTT CA, server certificate and client credentials in $output_dir"
echo "No passwords or private keys were printed."
