"""Shared validation for MQTT device identities and provisioning packages."""

from __future__ import annotations

import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
if str(REPOSITORY_ROOT) not in sys.path:
    sys.path.insert(0, str(REPOSITORY_ROOT))

from backend.core.validation import (
    validate_provisionable_device_id as validate_shared_device_id,
)


def validate_device_id(value: str) -> str:
    return validate_shared_device_id(value)


def mqtt_client_id(device_id: str) -> str:
    return f"iem-{validate_device_id(device_id)}"


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: mqtt_device_common.py DEVICE_ID", file=sys.stderr)
        return 2
    try:
        print(validate_device_id(sys.argv[1]))
    except ValueError as error:
        print(error, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
