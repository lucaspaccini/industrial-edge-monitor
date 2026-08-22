#!/usr/bin/env python3
"""Create or reuse the one controlled edge-node-02 portfolio-demo alert rule."""

from __future__ import annotations

import argparse
import json
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


RULE = {
    "name": "Portfolio demo: edge-node-02 high temperature",
    "device_id": "edge-node-02",
    "metric": "temperature",
    "operator": "greater_than",
    "threshold": 40.0,
    "duration_seconds": 0,
    "hysteresis": 1.0,
    "severity": "warning",
    "enabled": True,
}


def compatible(rule: dict, *, include_enabled: bool = True) -> bool:
    return all(
        rule.get(key) == value
        for key, value in RULE.items()
        if include_enabled or key != "enabled"
    )


def request_json(
    url: str, *, payload: dict | None = None, method: str = "GET"
):
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    request = Request(url, data=data, headers=headers, method=method)
    with urlopen(request, timeout=10) as response:
        return json.load(response)


def ensure_rule(api_url: str) -> tuple[int, str]:
    base = api_url.rstrip("/")
    query = urlencode({"device_id": RULE["device_id"]})
    rules = request_json(f"{base}/alert-rules/?{query}")
    matches = [rule for rule in rules if rule.get("name") == RULE["name"]]
    if len(matches) > 1:
        raise RuntimeError("multiple active rules use the controlled demo name")
    if matches:
        if not compatible(matches[0], include_enabled=False):
            raise RuntimeError(
                "the controlled demo rule exists with incompatible settings; "
                "inspect it explicitly before changing or archiving it"
            )
        if not matches[0].get("enabled"):
            updated = request_json(
                f"{base}/alert-rules/{matches[0]['id']}",
                payload={"enabled": True},
                method="PATCH",
            )
            return int(updated["id"]), "re-enabled"
        return int(matches[0]["id"]), "reused"

    created = request_json(f"{base}/alert-rules/", payload=RULE, method="POST")
    return int(created["id"]), "created"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--api-url", default="http://127.0.0.1:8000")
    parser.add_argument("--id-only", action="store_true")
    args = parser.parse_args()
    try:
        rule_id, action = ensure_rule(args.api_url)
    except (HTTPError, URLError, ValueError, RuntimeError) as error:
        parser.error(str(error))
    if args.id_only:
        print(rule_id)
    else:
        print(f"demo_rule_id={rule_id} action={action}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
