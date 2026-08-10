import os
from pathlib import Path
import subprocess

import pytest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
GENERATOR = REPOSITORY_ROOT / "scripts" / "generate-mqtt-security.sh"


@pytest.fixture
def generator_environment(tmp_path):
    fake_bin = tmp_path / "fake-bin"
    fake_bin.mkdir()
    fake_docker = fake_bin / "docker"
    fake_docker.write_text(
        """#!/usr/bin/env python3
from pathlib import Path
import os
import sys

arguments = sys.argv[1:]
if any("mosquitto_passwd -U" in argument for argument in arguments):
    if os.environ.get("FAKE_DOCKER_FAIL_HASH") == "1":
        raise SystemExit(17)
    mount_index = arguments.index("--volume") + 1
    staging = Path(arguments[mount_index].split(":", 1)[0])
    password_file = staging / "mosquitto" / "passwords"
    hashed_lines = []
    for line in password_file.read_text(encoding="utf-8").splitlines():
        username, separator, _password = line.partition(":")
        if not separator:
            raise SystemExit(2)
        hashed_lines.append(f"{username}:$7$1000$test-hash-{username}")
    password_file.write_text("\\n".join(hashed_lines) + "\\n", encoding="utf-8")
""",
        encoding="utf-8",
    )
    fake_docker.chmod(0o700)

    environment = os.environ.copy()
    environment["PATH"] = f"{fake_bin}{os.pathsep}{environment['PATH']}"
    environment["MOSQUITTO_IMAGE"] = "unused-in-tests"
    return environment


def run_generator(output: Path, environment: dict[str, str], *arguments: str):
    return subprocess.run(
        [str(GENERATOR), "--output", str(output), *arguments],
        cwd=REPOSITORY_ROOT,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )


def test_force_refuses_unmanaged_directory_and_preserves_sentinel(
    tmp_path, generator_environment
):
    output = tmp_path / "unmanaged"
    output.mkdir()
    sentinel = output / "keep-me"
    sentinel.write_text("preserve", encoding="utf-8")

    result = run_generator(output, generator_environment, "--force")

    assert result.returncode != 0
    assert sentinel.read_text(encoding="utf-8") == "preserve"


def test_force_refuses_wrong_marker_and_preserves_sentinel(
    tmp_path, generator_environment
):
    output = tmp_path / "wrong-marker"
    output.mkdir()
    (output / ".generated-version").write_text("unexpected\n", encoding="utf-8")
    sentinel = output / "keep-me"
    sentinel.write_text("preserve", encoding="utf-8")

    result = run_generator(output, generator_environment, "--force")

    assert result.returncode != 0
    assert sentinel.read_text(encoding="utf-8") == "preserve"


def test_output_symlink_is_rejected_without_touching_target(
    tmp_path, generator_environment
):
    target = tmp_path / "target"
    target.mkdir()
    sentinel = target / "keep-me"
    sentinel.write_text("preserve", encoding="utf-8")
    output = tmp_path / "output-link"
    output.symlink_to(target, target_is_directory=True)

    result = run_generator(output, generator_environment, "--force")

    assert result.returncode != 0
    assert output.is_symlink()
    assert sentinel.read_text(encoding="utf-8") == "preserve"


def test_home_directory_is_rejected_without_using_real_user_home(
    tmp_path, generator_environment
):
    fake_home = tmp_path / "fake-home"
    fake_home.mkdir()
    sentinel = fake_home / "keep-me"
    sentinel.write_text("preserve", encoding="utf-8")
    isolated_environment = generator_environment.copy()
    isolated_environment["HOME"] = str(fake_home)

    result = run_generator(
        fake_home,
        isolated_environment,
        "--force",
    )

    assert result.returncode != 0
    assert sentinel.read_text(encoding="utf-8") == "preserve"


def test_generated_bundle_can_be_intentionally_replaced(
    tmp_path, generator_environment
):
    output = tmp_path / "managed-bundle"

    initial = run_generator(
        output,
        generator_environment,
        "--device",
        "edge-node-03",
    )
    assert initial.returncode == 0, initial.stderr
    assert (output / ".generated-version").read_text(encoding="utf-8") == "1\n"
    assert (output / "clients" / "edge-node-03.password").is_file()

    sentinel = output / "old-bundle-sentinel"
    sentinel.write_text("old", encoding="utf-8")
    replacement = run_generator(
        output,
        generator_environment,
        "--force",
        "--device",
        "edge-node-03",
    )

    assert replacement.returncode == 0, replacement.stderr
    assert not sentinel.exists()
    assert (output / ".generated-version").read_text(encoding="utf-8") == "1\n"
    assert not list(tmp_path.glob(".mqtt-security.*"))


def test_failed_force_generation_preserves_previous_bundle(
    tmp_path, generator_environment
):
    output = tmp_path / "managed-bundle"
    initial = run_generator(output, generator_environment)
    assert initial.returncode == 0, initial.stderr
    sentinel = output / "old-bundle-sentinel"
    sentinel.write_text("preserve", encoding="utf-8")

    failing_environment = generator_environment.copy()
    failing_environment["FAKE_DOCKER_FAIL_HASH"] = "1"
    replacement = run_generator(output, failing_environment, "--force")

    assert replacement.returncode != 0
    assert sentinel.read_text(encoding="utf-8") == "preserve"
    assert (output / ".generated-version").read_text(encoding="utf-8") == "1\n"
    assert not list(tmp_path.glob(".mqtt-security.*"))


@pytest.mark.parametrize("device_id", ["bad/device", "x" * 64])
def test_invalid_device_id_is_rejected(
    tmp_path, generator_environment, device_id
):
    result = run_generator(
        tmp_path / "bundle",
        generator_environment,
        "--device",
        device_id,
    )

    assert result.returncode != 0
    assert "Invalid device ID" in result.stderr


@pytest.mark.parametrize(
    "reserved_identity", ["collector", "healthcheck", "simulator", "legacy-test"]
)
def test_reserved_service_identity_is_rejected_as_device(
    tmp_path, generator_environment, reserved_identity
):
    result = run_generator(
        tmp_path / "bundle",
        generator_environment,
        "--device",
        reserved_identity,
    )

    assert result.returncode != 0
    assert "Reserved service identity" in result.stderr


def test_duplicate_device_id_is_rejected(tmp_path, generator_environment):
    result = run_generator(
        tmp_path / "bundle",
        generator_environment,
        "--device",
        "edge-node-01",
    )

    assert result.returncode != 0
    assert "Duplicate device ID" in result.stderr
