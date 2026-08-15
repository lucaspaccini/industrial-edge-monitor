#!/usr/bin/env python3
"""Validate the versioned ESP32 flash layout and built application size."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


FLASH_SIZE = 4 * 1024 * 1024
PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_SIZE = 0x1000
APPLICATION_ALIGNMENT = 0x10000
DATA_ALIGNMENT = 0x1000
EXPECTED_LAYOUT = {
    "nvs": ("data", "nvs", 0x9000, 0x30000),
    "phy_init": ("data", "phy", 0x39000, 0x1000),
    "factory": ("app", "factory", 0x40000, 0x300000),
}


def number(value: str) -> int:
    return int(value.strip(), 0)


def read_partitions(path: Path) -> list[dict[str, int | str]]:
    rows: list[dict[str, int | str]] = []
    with path.open(encoding="utf-8", newline="") as stream:
        lines = (line for line in stream if not line.lstrip().startswith("#"))
        for raw in csv.reader(lines):
            if not raw or not raw[0].strip():
                continue
            if len(raw) < 5:
                raise ValueError(f"invalid partition row: {raw!r}")
            rows.append(
                {
                    "name": raw[0].strip(),
                    "type": raw[1].strip(),
                    "subtype": raw[2].strip(),
                    "offset": number(raw[3]),
                    "size": number(raw[4]),
                }
            )
    return rows


def validate(partitions: list[dict[str, int | str]]) -> dict[str, int | str]:
    if not partitions:
        raise ValueError("partition table is empty")
    previous_end = PARTITION_TABLE_OFFSET + PARTITION_TABLE_SIZE
    names: set[str] = set()
    factory: dict[str, int | str] | None = None
    for partition in sorted(partitions, key=lambda item: int(item["offset"])):
        name = str(partition["name"])
        offset = int(partition["offset"])
        size = int(partition["size"])
        end = offset + size
        if name in names:
            raise ValueError(f"duplicate partition name: {name}")
        names.add(name)
        if size <= 0 or offset < previous_end:
            raise ValueError(f"partition {name} overlaps a previous flash region")
        if end > FLASH_SIZE:
            raise ValueError(f"partition {name} exceeds the declared 4 MiB flash")
        if partition["type"] == "data" and (offset % DATA_ALIGNMENT or size % DATA_ALIGNMENT):
            raise ValueError(f"data partition {name} is not 4 KiB aligned")
        if partition["type"] == "app":
            if offset % APPLICATION_ALIGNMENT or size % APPLICATION_ALIGNMENT:
                raise ValueError(f"application partition {name} is not 64 KiB aligned")
            if partition["subtype"] == "factory":
                factory = partition
        previous_end = end
    if names != set(EXPECTED_LAYOUT):
        raise ValueError("partition names changed; the no-OTA layout requires explicit review")
    for name, expected in EXPECTED_LAYOUT.items():
        partition = next(item for item in partitions if item["name"] == name)
        actual = (
            str(partition["type"]),
            str(partition["subtype"]),
            int(partition["offset"]),
            int(partition["size"]),
        )
        if actual != expected:
            raise ValueError(f"partition {name} differs from the reviewed layout")
    if factory is None:
        raise ValueError("factory application partition is missing")
    if int(factory["size"]) != 0x300000:
        raise ValueError("factory application partition must remain exactly 3 MiB")
    if previous_end != 0x340000:
        raise ValueError("allocated layout end changed; review the intentional reserve")
    return factory


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--partitions", type=Path, default=Path("firmware/partitions.csv"))
    parser.add_argument("--application", type=Path)
    arguments = parser.parse_args()
    partitions = read_partitions(arguments.partitions)
    factory = validate(partitions)
    app_size = None
    if arguments.application is not None:
        app_size = arguments.application.stat().st_size
        if app_size > int(factory["size"]):
            raise ValueError(
                f"application is {app_size} bytes and exceeds the {factory['size']}-byte factory partition"
            )
    allocated_end = max(
        int(partition["offset"]) + int(partition["size"])
        for partition in partitions
    )
    print(f"flash_size_bytes={FLASH_SIZE}")
    print(f"factory_offset=0x{int(factory['offset']):X}")
    print(f"factory_size_bytes={int(factory['size'])}")
    if app_size is not None:
        print(f"application_size_bytes={app_size}")
        print(f"application_free_bytes={int(factory['size']) - app_size}")
    print(f"unallocated_reserve_bytes={FLASH_SIZE - allocated_end}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
