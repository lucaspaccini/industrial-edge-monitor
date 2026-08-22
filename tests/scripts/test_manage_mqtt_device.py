import json
from pathlib import Path
import stat
import sys
from types import SimpleNamespace

import pytest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))

import manage_mqtt_device as lifecycle  # noqa: E402


CA = "-----BEGIN CERTIFICATE-----\ntest-public-ca\n-----END CERTIFICATE-----\n"


@pytest.fixture
def bundle(tmp_path):
    root = tmp_path / "mqtt-security"
    for directory in ("ca", "server", "mosquitto", "clients"):
        (root / directory).mkdir(parents=True, exist_ok=True)
    (root / ".generated-version").write_text("1\n", encoding="utf-8")
    (root / "ca/ca.crt").write_text(CA, encoding="utf-8")
    (root / "mosquitto/passwords").write_text(
        "collector:$7$collector\nhealthcheck:$7$healthcheck\n", encoding="utf-8"
    )
    (root / "mosquitto/dynamic-security.json").write_text("{}\n", encoding="utf-8")
    (root / "server/server.key").write_text("private-key-sentinel\n", encoding="utf-8")
    (root / "clients/healthcheck.container.conf").write_text("healthcheck\n", encoding="utf-8")
    (root / "clients/collector.password").write_text("collector-secret\n", encoding="utf-8")
    return root


def fake_hash(bundle, username, password, image):
    path = bundle / "mosquitto/passwords"
    lines = [line for line in path.read_text(encoding="utf-8").splitlines() if line.partition(":")[0] != username]
    lines.append(f"{username}:$7$hash-for-{password}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def arguments(bundle, device_id="edge-node-03", output=None):
    return SimpleNamespace(
        bundle=bundle,
        device_id=device_id,
        broker_uri="mqtts://192.0.2.10:8883",
        output=output,
        mosquitto_image="unused",
    )


def test_package_validation_rejects_wrong_schema_and_wifi_fields():
    package = {
        "schema_version": 2,
        "device_id": "edge-node-03",
        "mqtt": {},
        "wifi_password": "must-not-be-here",
    }
    with pytest.raises(ValueError):
        lifecycle.validate_provisioning_package(package)

    package = {
        "schema_version": True,
        "device_id": "edge-node-03",
        "mqtt": {},
    }
    with pytest.raises(ValueError, match="unsupported schema"):
        lifecycle.validate_provisioning_package(package)

    with pytest.raises(ValueError, match="string"):
        lifecycle.validate_broker_uri(True)


def test_add_list_inspect_rotate_and_revoke(bundle, tmp_path, monkeypatch, capsys):
    passwords = iter(("first-secret", "second-secret"))
    monkeypatch.setattr(lifecycle, "hash_password", fake_hash)
    monkeypatch.setattr(lifecycle, "apply_runtime_permissions", lambda bundle, image: None)
    monkeypatch.setattr(lifecycle.secrets, "token_urlsafe", lambda length: next(passwords))
    output = tmp_path / "edge-node-03.provisioning.json"
    args = arguments(bundle, output=output)

    assert lifecycle.add_or_rotate(args, rotate=False) == output
    first = json.loads(output.read_text(encoding="utf-8"))
    lifecycle.validate_provisioning_package(first)
    assert first["mqtt"]["password"] == "first-secret"
    assert "wifi" not in first
    assert stat.S_IMODE(output.stat().st_mode) == 0o600

    lifecycle.list_devices(SimpleNamespace(bundle=bundle))
    assert capsys.readouterr().out == "edge-node-03\n"
    lifecycle.inspect_device(SimpleNamespace(bundle=bundle, device_id="edge-node-03"))
    inspected = capsys.readouterr().out
    assert "credential_configured" in inspected
    assert "first-secret" not in inspected and "$7$" not in inspected

    lifecycle.add_or_rotate(args, rotate=True)
    second = json.loads(output.read_text(encoding="utf-8"))
    assert second["mqtt"]["password"] == "second-secret"
    password_file = (bundle / "mosquitto/passwords").read_text(encoding="utf-8")
    assert "first-secret" not in password_file
    assert "hash-for-second-secret" in password_file

    lifecycle.revoke(SimpleNamespace(bundle=bundle, device_id="edge-node-03", mosquitto_image="unused"))
    assert "edge-node-03:" not in (bundle / "mosquitto/passwords").read_text(encoding="utf-8")
    assert not (bundle / "devices/edge-node-03.json").exists()


def test_duplicate_and_reserved_identity_are_rejected(bundle, tmp_path, monkeypatch):
    monkeypatch.setattr(lifecycle, "hash_password", fake_hash)
    monkeypatch.setattr(lifecycle, "apply_runtime_permissions", lambda bundle, image: None)
    args = arguments(bundle, output=tmp_path / "package.json")
    lifecycle.add_or_rotate(args, rotate=False)
    with pytest.raises(ValueError, match="already exists"):
        lifecycle.add_or_rotate(args, rotate=False)
    with pytest.raises(ValueError, match="Reserved"):
        lifecycle.add_or_rotate(arguments(bundle, "collector", tmp_path / "bad.json"), rotate=False)
    with pytest.raises(ValueError, match="compatibility identity"):
        lifecycle.add_or_rotate(arguments(bundle, "legacy-device", tmp_path / "legacy.json"), rotate=False)


@pytest.mark.parametrize("edge_node_02_present", [True, False])
def test_permission_normalization_is_idempotent_and_preserves_content(
    bundle, monkeypatch, edge_node_02_present
):
    edge_password = bundle / "clients/edge-node-02.password"
    if edge_node_02_present:
        edge_password.write_text("simulator-secret\n", encoding="utf-8")

    expected_modes = {
        "server/server.key": 0o440,
        "mosquitto/dynamic-security.json": 0o660,
        "clients/healthcheck.container.conf": 0o440,
        "clients/collector.password": 0o440,
    }
    if edge_node_02_present:
        expected_modes["clients/edge-node-02.password"] = 0o440

    def fake_runtime_permissions(target, image):
        assert target == bundle.resolve()
        assert image == "unused"
        for relative_path, mode in expected_modes.items():
            (target / relative_path).chmod(mode)

    monkeypatch.setattr(lifecycle, "apply_runtime_permissions", fake_runtime_permissions)
    before = {
        path.relative_to(bundle): path.read_bytes()
        for path in bundle.rglob("*")
        if path.is_file()
    }
    args = SimpleNamespace(bundle=bundle, mosquitto_image="unused")

    lifecycle.normalize_permissions(args)
    lifecycle.normalize_permissions(args)

    after = {
        path.relative_to(bundle): path.read_bytes()
        for path in bundle.rglob("*")
        if path.is_file()
    }
    assert after == before
    for relative_path, mode in expected_modes.items():
        assert stat.S_IMODE((bundle / relative_path).stat().st_mode) == mode


def test_historical_legacy_bundle_identity_can_be_revoked_but_not_listed(
    bundle, monkeypatch, capsys
):
    password_file = bundle / "mosquitto/passwords"
    password_file.write_text(
        password_file.read_text(encoding="utf-8")
        + "legacy-device:$7$historical-collision\n",
        encoding="utf-8",
    )
    monkeypatch.setattr(
        lifecycle, "apply_runtime_permissions", lambda target, image: None
    )

    lifecycle.list_devices(SimpleNamespace(bundle=bundle))
    assert capsys.readouterr().out == ""
    lifecycle.revoke(
        SimpleNamespace(
            bundle=bundle,
            device_id="legacy-device",
            mosquitto_image="unused",
        )
    )
    assert "legacy-device:" not in password_file.read_text(encoding="utf-8")


def test_bundle_is_preserved_when_mutation_or_package_commit_fails(bundle, tmp_path, monkeypatch):
    sentinel = bundle / "sentinel"
    sentinel.write_text("original", encoding="utf-8")
    before = (bundle / "mosquitto/passwords").read_bytes()

    def fail_hash(*unused):
        raise RuntimeError("simulated hashing failure")

    monkeypatch.setattr(lifecycle, "hash_password", fail_hash)
    monkeypatch.setattr(lifecycle, "apply_runtime_permissions", lambda bundle, image: None)
    with pytest.raises(RuntimeError):
        lifecycle.add_or_rotate(arguments(bundle, output=tmp_path / "failure.json"), rotate=False)
    assert sentinel.read_text(encoding="utf-8") == "original"
    assert (bundle / "mosquitto/passwords").read_bytes() == before


def test_symlinks_and_invalid_uri_are_rejected(bundle, tmp_path):
    linked = bundle / "ca/link"
    linked.symlink_to(bundle / "ca/ca.crt")
    with pytest.raises(ValueError, match="symlink"):
        lifecycle.ensure_managed_bundle(bundle)
    linked.unlink()
    with pytest.raises(ValueError, match="explicit valid port"):
        lifecycle.validate_broker_uri("mqtts://broker.example")
    with pytest.raises(ValueError):
        lifecycle.validate_broker_uri("mqtt://broker.example:1883")
    with pytest.raises(ValueError, match="contain only host"):
        lifecycle.validate_broker_uri("mqtts://broker.example:8883/")

    root_link = tmp_path / "bundle-link"
    root_link.symlink_to(bundle, target_is_directory=True)
    with pytest.raises(ValueError, match="root must not be a symlink"):
        lifecycle.ensure_managed_bundle(root_link)


def test_password_is_supplied_on_stdin_not_process_arguments(bundle, monkeypatch):
    captured = {}

    def fake_run(command, **kwargs):
        captured["command"] = command
        captured.update(kwargs)

    monkeypatch.setattr(lifecycle.subprocess, "run", fake_run)
    lifecycle.hash_password(bundle, "edge-node-03", "do-not-leak-this", "mosquitto:test")
    assert "do-not-leak-this" not in captured["command"]
    assert captured["input"] == "do-not-leak-this\ndo-not-leak-this\n"
    assert captured["text"] is True
    assert "--interactive" in captured["command"]
