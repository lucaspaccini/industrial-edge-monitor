from datetime import datetime, timezone
import re
from typing import Any


DEVICE_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,62}$")
DEVICE_TIMESTAMP_PATTERN = re.compile(
    r"^(?P<date>[0-9]{4}-[0-9]{2}-[0-9]{2})T"
    r"(?P<time>[0-9]{2}:[0-9]{2}:[0-9]{2})"
    r"(?P<fraction>\.[0-9]+)?"
    r"(?P<timezone>Z|[+-](?:[01][0-9]|2[0-3]):[0-5][0-9])$"
)
RESERVED_DEVICE_IDENTITIES = frozenset(
    {"collector", "healthcheck", "simulator", "legacy-test"}
)
LEGACY_DEVICE_IDENTITY = "legacy-device"


def validate_device_id_syntax(value: str) -> str:
    if not isinstance(value, str) or not DEVICE_ID_PATTERN.fullmatch(value):
        raise ValueError(
            "device_id must be 1-63 ASCII letters, digits, '.', '_' or '-'"
        )
    return value


def validate_device_id(value: str) -> str:
    """Validate an identity accepted by backend data-domain compatibility paths."""
    value = validate_device_id_syntax(value)
    if value in RESERVED_DEVICE_IDENTITIES:
        raise ValueError(f"Reserved service identity cannot be a device_id: {value}")
    return value


def validate_provisionable_device_id(value: str) -> str:
    """Validate an ordinary device identity that may be provisioned or publish."""
    value = validate_device_id(value)
    if value == LEGACY_DEVICE_IDENTITY:
        raise ValueError(
            f"Reserved compatibility identity cannot be provisioned: {value}"
        )
    return value


def validate_device_timestamp(value: Any) -> datetime:
    """Validate lossless RFC 3339 device time and normalize it to UTC.

    Accepted input is ``YYYY-MM-DDTHH:MM:SS[.fraction](Z|+/-HH:MM)``.
    A fraction may contain any number of digits, but every digit must be zero.
    """
    if not isinstance(value, str):
        raise ValueError("device timestamp must be an RFC 3339 string")

    match = DEVICE_TIMESTAMP_PATTERN.fullmatch(value)
    if match is None:
        raise ValueError(
            "device timestamp must match "
            "YYYY-MM-DDTHH:MM:SS[.fraction](Z|+/-HH:MM)"
        )

    fraction = match.group("fraction")
    if fraction is not None and any(digit != "0" for digit in fraction[1:]):
        raise ValueError("device timestamp fraction must contain only zeros")

    parse_value = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        parsed = datetime.fromisoformat(parse_value)
    except ValueError as exc:
        raise ValueError("device timestamp must be a valid RFC 3339 value") from exc
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise ValueError("device timestamp must include a timezone")

    normalized = parsed.astimezone(timezone.utc)
    if normalized.microsecond != 0:
        raise ValueError("device timestamp must have whole-second precision")
    return normalized
