#!/usr/bin/env python3
"""Fail if a required local TCP port is already bound; never stop its owner."""

from __future__ import annotations

import argparse
from pathlib import Path


def parse_listening_ports(table: str) -> set[int]:
    result: set[int] = set()
    for line in table.splitlines()[1:]:
        fields = line.split()
        if len(fields) >= 4 and fields[3] == "0A":
            result.add(int(fields[1].rsplit(":", 1)[1], 16))
    return result


def listening_ports() -> set[int]:
    result: set[int] = set()
    for table in (Path("/proc/net/tcp"), Path("/proc/net/tcp6")):
        if not table.is_file():
            continue
        result.update(parse_listening_ports(table.read_text(encoding="ascii")))
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ports", nargs="+", type=int)
    args = parser.parse_args()
    occupied = sorted(set(args.ports) & listening_ports())
    if occupied:
        parser.error(
            "required host port(s) already in use: "
            + ", ".join(map(str, occupied))
            + "; inspect and stop/reconfigure only the owning process"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
