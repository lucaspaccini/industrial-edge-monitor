#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


SUPPORTED_PASSWORD_HASH_PREFIXES = ("$7$", "$argon2id$")


def load_passwords(path: Path) -> dict[str, str]:
    passwords: dict[str, str] = {}
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        if not raw_line:
            continue
        username, separator, encoded_password = raw_line.partition(":")
        if (
            not separator
            or not username
            or not encoded_password.startswith(SUPPORTED_PASSWORD_HASH_PREFIXES)
        ):
            raise ValueError(f"invalid hashed password entry on line {line_number}")
        if username in passwords:
            raise ValueError(f"duplicate MQTT username on line {line_number}")
        passwords[username] = encoded_password
    if not passwords:
        raise ValueError("password file contains no clients")
    return passwords


def build_configuration(policy: dict, passwords: dict[str, str]) -> dict:
    roles = policy["roles"]
    role_names = {role["rolename"] for role in roles}
    if len(role_names) != len(roles):
        raise ValueError("security policy contains duplicate roles")

    default_role = policy["default_role"]
    service_roles = policy["service_roles"]
    selected_roles = {default_role, *service_roles.values()}
    if not selected_roles.issubset(role_names):
        raise ValueError("security policy references an undefined role")

    clients = []
    for username, encoded_password in sorted(passwords.items()):
        role = service_roles.get(username, default_role)
        clients.append(
            {
                "username": username,
                "encoded_password": encoded_password,
                "roles": [{"rolename": role, "priority": 0}],
            }
        )

    return {
        "defaultACLAccess": policy["default_acl_access"],
        "clients": clients,
        "groups": [
            {
                "groupname": "unauthenticated",
                "roles": [],
            }
        ],
        "roles": roles,
        "anonymousGroup": "unauthenticated",
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy", required=True, type=Path)
    parser.add_argument("--password-file", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    policy = json.loads(args.policy.read_text(encoding="utf-8"))
    passwords = load_passwords(args.password_file)
    configuration = build_configuration(policy, passwords)

    temporary_output = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary_output.write_text(
        json.dumps(configuration, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary_output.replace(args.output)


if __name__ == "__main__":
    main()
