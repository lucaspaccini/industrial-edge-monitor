"""Transactional MQTT device identity lifecycle for managed Sprint 15 bundles."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import stat
import subprocess
import tempfile
from urllib.parse import urlparse

from build_mosquitto_security import build_configuration, load_passwords
from mqtt_device_common import mqtt_client_id, validate_device_id
from backend.core.validation import LEGACY_DEVICE_IDENTITY, validate_device_id_syntax


BUNDLE_VERSION = "1"
PACKAGE_SCHEMA_VERSION = 1
DEFAULT_IMAGE = "eclipse-mosquitto:2.1.2-alpine"


def validate_existing_device_id(value: str) -> str:
    """Allow inspection/removal of a historical legacy collision, never creation."""
    if validate_device_id_syntax(value) == LEGACY_DEVICE_IDENTITY:
        return value
    return validate_device_id(value)


def validate_broker_uri(value: str) -> str:
    if not isinstance(value, str):
        raise ValueError("broker URI must be a string")
    parsed = urlparse(value)
    if parsed.scheme != "mqtts" or not parsed.hostname or parsed.username or parsed.password:
        raise ValueError("broker URI must be mqtts://host[:port] without credentials")
    try:
        port = parsed.port
    except ValueError as error:
        raise ValueError("broker URI port is invalid") from error
    if port is None or not 1 <= port <= 65535 or parsed.path != "" or parsed.query or parsed.fragment:
        raise ValueError("broker URI must contain only host and an explicit valid port")
    return value


def validate_provisioning_package(package: dict) -> None:
    if not isinstance(package, dict):
        raise ValueError("provisioning package must be a JSON object")
    schema_version = package.get("schema_version")
    if (set(package) != {"schema_version", "device_id", "mqtt"}
            or type(schema_version) is not int or schema_version != PACKAGE_SCHEMA_VERSION):
        raise ValueError("provisioning package has an unsupported schema")
    device_id = validate_device_id(package.get("device_id"))
    mqtt = package.get("mqtt")
    if not isinstance(mqtt, dict) or set(mqtt) != {
        "broker_uri", "username", "password", "client_id", "ca_certificate"
    }:
        raise ValueError("provisioning package MQTT object is incomplete")
    if validate_broker_uri(mqtt["broker_uri"]) != mqtt["broker_uri"]:
        raise ValueError("provisioning package broker URI is invalid")
    if not isinstance(mqtt["username"], str) or not isinstance(mqtt["client_id"], str):
        raise ValueError("provisioning package identity fields must be strings")
    if mqtt["username"] != device_id or mqtt["client_id"] != mqtt_client_id(device_id):
        raise ValueError("provisioning package identity fields are inconsistent")
    if not isinstance(mqtt["password"], str) or not mqtt["password"]:
        raise ValueError("provisioning package password is missing")
    if not isinstance(mqtt["ca_certificate"], str) or "-----BEGIN CERTIFICATE-----" not in mqtt["ca_certificate"]:
        raise ValueError("provisioning package CA certificate is missing")


def ensure_managed_bundle(bundle: Path) -> Path:
    if bundle.is_symlink():
        raise ValueError("managed bundle root must not be a symlink")
    bundle = bundle.resolve(strict=True)
    marker = bundle / ".generated-version"
    if not bundle.is_dir() or marker.is_symlink() or marker.read_text(encoding="utf-8").strip() != BUNDLE_VERSION:
        raise ValueError("bundle is not a managed Sprint 15 MQTT security bundle")
    for path in bundle.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"managed bundle contains a symlink: {path.relative_to(bundle)}")
    for required in ("ca/ca.crt", "mosquitto/passwords", "mosquitto/dynamic-security.json"):
        if not (bundle / required).is_file():
            raise ValueError(f"managed bundle is incomplete: {required}")
    return bundle


def read_identities(bundle: Path) -> dict[str, str]:
    return load_passwords(bundle / "mosquitto/passwords")


def write_dynamic_security(bundle: Path, repository_root: Path) -> None:
    policy = json.loads((repository_root / "docker/mosquitto/security-policy.json").read_text(encoding="utf-8"))
    configuration = build_configuration(policy, read_identities(bundle))
    target = bundle / "mosquitto/dynamic-security.json"
    target.write_text(json.dumps(configuration, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    target.chmod(0o600)


def apply_runtime_permissions(bundle: Path, image: str) -> None:
    subprocess.run(
        [
            "docker", "run", "--rm", "--entrypoint", "sh",
            "--volume", f"{bundle}:/work", image, "-c",
            'chown "1883:$1" /work/server/server.key /work/mosquitto/dynamic-security.json /work/clients/healthcheck.container.conf '
            '&& chmod 0440 /work/server/server.key /work/clients/healthcheck.container.conf '
            '&& chmod 0660 /work/mosquitto/dynamic-security.json '
            '&& chown "10001:$1" /work/clients/collector.password '
            '&& chmod 0440 /work/clients/collector.password '
            '&& if [ -f /work/clients/edge-node-02.password ]; then '
            'chown "10001:$1" /work/clients/edge-node-02.password '
            '&& chmod 0440 /work/clients/edge-node-02.password; fi',
            "sh", str(os.getgid()),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )


def normalize_permissions(args) -> None:
    """Repair runtime ownership/modes without replacing or rewriting bundle data."""
    bundle = ensure_managed_bundle(args.bundle)
    for relative_path in (
        "server/server.key",
        "mosquitto/dynamic-security.json",
        "clients/healthcheck.container.conf",
        "clients/collector.password",
    ):
        if not (bundle / relative_path).is_file():
            raise ValueError(f"managed bundle is incomplete: {relative_path}")
    apply_runtime_permissions(bundle, args.mosquitto_image)


def hash_password(bundle: Path, username: str, password: str, image: str) -> None:
    uid = os.getuid()
    gid = os.getgid()
    subprocess.run(
        [
            "docker", "run", "--rm", "--interactive", "--entrypoint", "sh",
            "--volume", f"{bundle}:/work", image, "-c",
            'chown root:root /work/mosquitto/passwords '
            '&& mosquitto_passwd /work/mosquitto/passwords "$1" >/dev/null '
            '&& chown "$2:$3" /work/mosquitto/passwords',
            "sh", username, str(uid), str(gid),
        ],
        check=True,
        input=f"{password}\n{password}\n",
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    (bundle / "mosquitto/passwords").chmod(0o600)


def device_metadata_path(bundle: Path, device_id: str) -> Path:
    return bundle / "devices" / f"{device_id}.json"


def load_metadata(bundle: Path, device_id: str) -> dict:
    path = device_metadata_path(bundle, device_id)
    if not path.is_file():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def write_device_files(bundle: Path, device_id: str, password: str, broker_uri: str) -> dict:
    client_id = mqtt_client_id(device_id)
    ca = (bundle / "ca/ca.crt").read_text(encoding="utf-8")
    metadata = {
        "schema_version": PACKAGE_SCHEMA_VERSION,
        "device_id": device_id,
        "broker_uri": broker_uri,
        "client_id": client_id,
    }
    metadata_path = device_metadata_path(bundle, device_id)
    metadata_path.parent.mkdir(mode=0o700, exist_ok=True)
    metadata_path.write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    metadata_path.chmod(0o600)

    clients = bundle / "clients"
    clients.mkdir(mode=0o755, exist_ok=True)
    (clients / f"{device_id}.password").write_text(password + "\n", encoding="utf-8")
    (clients / f"{device_id}.container.conf").write_text(
        f"--host mqtt\n--port 8883\n--id {client_id}-container\n"
        f"--cafile /run/mqtt-security/ca/ca.crt\n--tls-version tlsv1.2\n"
        f"--username {device_id}\n--pw {password}\n",
        encoding="utf-8",
    )
    for path in clients.glob(f"{device_id}.*"):
        path.chmod(0o600)
    return {
        "schema_version": PACKAGE_SCHEMA_VERSION,
        "device_id": device_id,
        "mqtt": {
            "broker_uri": broker_uri,
            "username": device_id,
            "password": password,
            "client_id": client_id,
            "ca_certificate": ca,
        },
    }


def write_package(package: dict, output: Path) -> None:
    validate_provisioning_package(package)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.is_symlink():
        raise ValueError("refusing provisioning package symlink")
    descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_TRUNC | os.O_NOFOLLOW, 0o600)
    with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
        json.dump(package, stream, indent=2, sort_keys=True)
        stream.write("\n")
    output.chmod(0o600)


def transactional_update(
    bundle: Path,
    mutator,
    repository_root: Path,
    mosquitto_image: str,
    prepare=None,
    commit=None,
) -> object:
    bundle = ensure_managed_bundle(bundle)
    parent = bundle.parent
    staging = Path(tempfile.mkdtemp(prefix=".mqtt-security.lifecycle.", dir=parent))
    backup: Path | None = None
    try:
        shutil.rmtree(staging)
        shutil.copytree(bundle, staging, symlinks=True)
        result = mutator(staging)
        ensure_managed_bundle(staging)
        write_dynamic_security(staging, repository_root)
        apply_runtime_permissions(staging, mosquitto_image)
        if prepare is not None:
            prepare(result)
        backup = Path(tempfile.mkdtemp(prefix=".mqtt-security.backup.", dir=parent))
        backup.rmdir()
        bundle.rename(backup)
        try:
            staging.rename(bundle)
        except BaseException:
            backup.rename(bundle)
            backup = None
            raise
        try:
            if commit is not None:
                commit(result)
        except BaseException:
            staging = Path(tempfile.mkdtemp(prefix=".mqtt-security.failed.", dir=parent))
            staging.rmdir()
            bundle.rename(staging)
            backup.rename(bundle)
            backup = None
            shutil.rmtree(staging)
            raise
        shutil.rmtree(backup)
        backup = None
        return result
    finally:
        if staging.exists():
            shutil.rmtree(staging)
        if backup is not None and backup.exists() and not bundle.exists():
            backup.rename(bundle)


def remove_identity(password_file: Path, device_id: str) -> None:
    lines = password_file.read_text(encoding="utf-8").splitlines()
    remaining = [line for line in lines if line.partition(":")[0] != device_id]
    password_file.write_text("\n".join(remaining) + "\n", encoding="utf-8")


def add_or_rotate(args, rotate: bool) -> Path:
    device_id = validate_device_id(args.device_id)
    broker_uri = validate_broker_uri(args.broker_uri)
    output = args.output or Path(".local/provisioning-packages") / f"{device_id}.provisioning.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.is_symlink():
        raise ValueError("refusing provisioning package symlink")
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{device_id}.",
        suffix=".provisioning.tmp",
        dir=output.parent,
    )
    os.close(descriptor)
    temporary_output = Path(temporary_name)
    password = secrets.token_urlsafe(32)
    repository_root = Path(__file__).resolve().parents[1]

    def mutate(staging: Path):
        identities = read_identities(staging)
        exists = device_id in identities
        if rotate != exists:
            action = "rotate" if rotate else "add"
            raise ValueError(f"cannot {action} device {device_id}: identity {'is missing' if rotate else 'already exists'}")
        hash_password(staging, device_id, password, args.mosquitto_image)
        return write_device_files(staging, device_id, password, broker_uri)

    try:
        transactional_update(
            args.bundle,
            mutate,
            repository_root,
            args.mosquitto_image,
            prepare=lambda package: write_package(package, temporary_output),
            commit=lambda package: os.replace(temporary_output, output),
        )
        output.chmod(0o600)
    finally:
        if temporary_output.exists():
            temporary_output.unlink()
    return output


def revoke(args) -> None:
    device_id = validate_existing_device_id(args.device_id)
    repository_root = Path(__file__).resolve().parents[1]

    def mutate(staging: Path):
        if device_id not in read_identities(staging):
            raise ValueError(f"cannot revoke device {device_id}: identity is missing")
        remove_identity(staging / "mosquitto/passwords", device_id)
        for path in (staging / "clients").glob(f"{device_id}.*"):
            path.unlink()
        metadata = device_metadata_path(staging, device_id)
        if metadata.exists():
            metadata.unlink()

    transactional_update(args.bundle, mutate, repository_root, args.mosquitto_image)


def list_devices(args) -> None:
    identities = read_identities(ensure_managed_bundle(args.bundle))
    reserved = {
        "collector", "healthcheck", "simulator", "legacy-test",
        LEGACY_DEVICE_IDENTITY,
    }
    for identity in sorted(set(identities) - reserved):
        print(identity)


def inspect_device(args) -> None:
    device_id = validate_existing_device_id(args.device_id)
    bundle = ensure_managed_bundle(args.bundle)
    if device_id not in read_identities(bundle):
        raise ValueError(f"device identity is missing: {device_id}")
    metadata = load_metadata(bundle, device_id)
    print(json.dumps({
        "device_id": device_id,
        "client_id": metadata.get("client_id", mqtt_client_id(device_id)),
        "broker_uri": metadata.get("broker_uri"),
        "credential_configured": True,
    }, indent=2, sort_keys=True))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, default=Path(".local/mqtt-security"))
    parser.add_argument("--mosquitto-image", default=os.environ.get("MOSQUITTO_IMAGE", DEFAULT_IMAGE))
    subparsers = parser.add_subparsers(dest="command", required=True)
    for name in ("add", "rotate"):
        command = subparsers.add_parser(name)
        command.add_argument("device_id")
        command.add_argument("--broker-uri", required=True)
        command.add_argument("--output", type=Path)
    subparsers.add_parser("list")
    subparsers.add_parser("normalize-permissions")
    inspect_parser = subparsers.add_parser("inspect")
    inspect_parser.add_argument("device_id")
    revoke_parser = subparsers.add_parser("revoke")
    revoke_parser.add_argument("device_id")
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    try:
        if args.command == "add":
            output = add_or_rotate(args, rotate=False)
            print(f"Device identity added. Provisioning package written to {output}")
        elif args.command == "rotate":
            output = add_or_rotate(args, rotate=True)
            print(f"Device credential rotated. Provisioning package written to {output}")
        elif args.command == "revoke":
            revoke(args)
            print("Device identity revoked.")
        elif args.command == "list":
            list_devices(args)
        elif args.command == "normalize-permissions":
            normalize_permissions(args)
            print("Runtime permissions normalized; bundle contents were not rewritten.")
        else:
            inspect_device(args)
        if args.command in {"add", "rotate", "revoke"}:
            print("Recreate mqtt so Mosquitto loads the updated transactional bundle.")
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
