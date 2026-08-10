import json
import subprocess

import pytest

from scripts.build_mosquitto_security import load_passwords


def test_security_policy_builds_device_and_service_roles(tmp_path):
    password_file = tmp_path / "passwords"
    output_file = tmp_path / "dynamic-security.json"
    password_file.write_text(
        "collector:$7$1000$collector-hash\n"
        "edge-node-01:$7$1000$device-hash\n",
        encoding="utf-8",
    )

    subprocess.run(
        [
            "python3",
            "scripts/build_mosquitto_security.py",
            "--policy",
            "docker/mosquitto/security-policy.json",
            "--password-file",
            str(password_file),
            "--output",
            str(output_file),
        ],
        check=True,
    )

    configuration = json.loads(output_file.read_text(encoding="utf-8"))
    clients = {client["username"]: client for client in configuration["clients"]}
    assert clients["collector"]["roles"][0]["rolename"] == "collector"
    assert clients["edge-node-01"]["roles"][0]["rolename"] == "device"
    assert configuration["defaultACLAccess"]["subscribe"] is False
    assert "collector-hash" in clients["collector"]["encoded_password"]

    device_role = next(
        role for role in configuration["roles"] if role["rolename"] == "device"
    )
    assert all(acl["acltype"] == "publishClientSend" for acl in device_role["acls"])


@pytest.mark.parametrize(
    "encoded_password",
    [
        "$7$1000$sha512-pbkdf2-test-hash",
        "$argon2id$v=19$m=12288,t=3,p=1$test-salt$test-hash",
    ],
)
def test_supported_mosquitto_password_hashes_are_accepted(
    tmp_path, encoded_password
):
    password_file = tmp_path / "passwords"
    password_file.write_text(
        f"edge-node-01:{encoded_password}\n",
        encoding="utf-8",
    )

    assert load_passwords(password_file) == {"edge-node-01": encoded_password}


@pytest.mark.parametrize("encoded_password", ["plaintext", "$6$unsupported-hash"])
def test_plaintext_and_unknown_password_hashes_are_rejected(
    tmp_path, encoded_password
):
    password_file = tmp_path / "passwords"
    password_file.write_text(
        f"edge-node-01:{encoded_password}\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="invalid hashed password entry"):
        load_passwords(password_file)
