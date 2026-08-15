"""Shared validation for MQTT device identities and provisioning packages."""

from __future__ import annotations

import re
import sys


DEVICE_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,62}$")
RESERVED_IDENTITIES = frozenset({"collector", "healthcheck", "simulator", "legacy-test"})


def validate_device_id(value: str) -> str:
    if not isinstance(value, str) or not DEVICE_ID_PATTERN.fullmatch(value):
        raise ValueError("device_id must be 1-63 ASCII letters, digits, '.', '_' or '-'")
    if value in RESERVED_IDENTITIES:
        raise ValueError(f"Reserved service identity cannot be used as a device_id: {value}")
    return value


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
