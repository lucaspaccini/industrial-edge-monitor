import importlib.util
from pathlib import Path

import pytest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPOSITORY_ROOT / "scripts/check-firmware-layout.py"
SPEC = importlib.util.spec_from_file_location("firmware_layout", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
firmware_layout = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(firmware_layout)


def test_reviewed_no_ota_layout_is_valid():
    partitions = firmware_layout.read_partitions(REPOSITORY_ROOT / "firmware/partitions.csv")
    factory = firmware_layout.validate(partitions)
    assert factory["offset"] == 0x40000
    assert factory["size"] == 0x300000


def test_layout_rejects_overlap_and_unreviewed_partition():
    partitions = firmware_layout.read_partitions(REPOSITORY_ROOT / "firmware/partitions.csv")
    overlapping = [dict(item) for item in partitions]
    overlapping[1]["offset"] = 0x38000
    with pytest.raises(ValueError, match="overlaps"):
        firmware_layout.validate(overlapping)

    extra = [dict(item) for item in partitions]
    extra.append({"name": "ota_0", "type": "app", "subtype": "ota_0", "offset": 0x340000, "size": 0x10000})
    with pytest.raises(ValueError, match="no-OTA"):
        firmware_layout.validate(extra)


def test_layout_rejects_misaligned_data_partition():
    partitions = firmware_layout.read_partitions(REPOSITORY_ROOT / "firmware/partitions.csv")
    changed = [dict(item) for item in partitions]
    changed[0]["size"] = 0x2F001
    with pytest.raises(ValueError, match="4 KiB aligned"):
        firmware_layout.validate(changed)
