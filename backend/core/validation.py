def validate_device_id(value: str) -> str:
    if not value or len(value) > 63 or not all(
        character.isalnum() or character in "._-" for character in value
    ):
        raise ValueError("invalid device_id")
    return value
