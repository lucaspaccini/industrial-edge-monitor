import inspect
from pathlib import Path

import pytest
from pydantic import ValidationError

from backend.api.routes.telemetry import read_telemetry
from backend.core.config import Settings, settings


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]


def make_settings(**overrides) -> Settings:
    values = {
        "APP_ENV": "development",
        "DATABASE_PATH": "data/telemetry.db",
        "MQTT_HOST": "localhost",
        "CORS_ORIGINS": "http://localhost:3000",
    }
    values.update(overrides)
    return Settings(_env_file=None, **values)


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("MQTT_PORT", 0),
        ("MQTT_PORT", 65536),
        ("MQTT_PORT", True),
        ("MQTT_KEEPALIVE_SECONDS", 0),
        ("MQTT_KEEPALIVE_SECONDS", False),
        ("DEFAULT_HISTORY_LIMIT", 0),
        ("DEVICE_OFFLINE_TIMEOUT_SECONDS", -1),
        ("DATABASE_TIMEOUT_SECONDS", 0),
        ("DATABASE_TIMEOUT_SECONDS", float("nan")),
        ("DATABASE_TIMEOUT_SECONDS", float("inf")),
        ("DATABASE_TIMEOUT_SECONDS", float("-inf")),
    ],
)
def test_invalid_numeric_configuration_fails_fast(field, value):
    with pytest.raises(ValidationError):
        make_settings(**{field: value})


def test_numeric_environment_strings_are_parsed():
    configured = make_settings(
        MQTT_PORT="1884",
        MQTT_KEEPALIVE_SECONDS="30",
        DATABASE_TIMEOUT_SECONDS="5.5",
    )

    assert configured.MQTT_PORT == 1884
    assert configured.MQTT_KEEPALIVE_SECONDS == 30
    assert configured.DATABASE_TIMEOUT_SECONDS == 5.5


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("MQTT_HOST", "mqtt://localhost"),
        ("MQTT_TOPIC", "industrial/+/telemetry"),
        ("MQTT_TOPIC_PREFIX", "industrial/devices/#"),
        ("CORS_ORIGINS", "localhost:3000"),
        ("CORS_ORIGINS", "http://localhost:3000/path"),
    ],
)
def test_invalid_network_configuration_fails_fast(field, value):
    with pytest.raises(ValidationError):
        make_settings(**{field: value})


@pytest.mark.parametrize(
    ("field", "value"),
    [("APP_ENV", "staging"), ("LOG_LEVEL", "VERBOSE")],
)
def test_invalid_application_configuration_fails_fast(field, value):
    with pytest.raises(ValidationError):
        make_settings(**{field: value})


def test_cors_origins_are_normalized_and_deduplicated():
    configured = make_settings(
        CORS_ORIGINS="http://localhost:3000/, https://example.test,https://example.test"
    )

    assert configured.cors_origins_list == [
        "http://localhost:3000",
        "https://example.test",
    ]


def test_production_rejects_development_connectivity_defaults():
    with pytest.raises(ValidationError, match="DATABASE_PATH"):
        make_settings(APP_ENV="production")


def test_production_rejects_local_mqtt_host_with_external_database(tmp_path):
    with pytest.raises(ValidationError, match="MQTT_HOST"):
        make_settings(
            APP_ENV="production",
            DATABASE_PATH=str(tmp_path / "telemetry.db"),
        )


def test_production_rejects_database_inside_source_tree():
    with pytest.raises(ValidationError, match="DATABASE_PATH"):
        make_settings(
            APP_ENV="production",
            DATABASE_PATH="data/production.db",
            MQTT_HOST="mqtt",
        )


def test_production_accepts_explicit_service_configuration(tmp_path):
    configured = make_settings(
        APP_ENV="production",
        DATABASE_PATH=str(tmp_path / "telemetry.db"),
        MQTT_HOST="mqtt",
    )

    assert configured.APP_ENV == "production"
    assert configured.MQTT_HOST == "mqtt"


def test_production_mqtt_client_requires_tls(tmp_path):
    with pytest.raises(ValidationError, match="MQTT_TLS_ENABLED"):
        make_settings(
            APP_ENV="production",
            DATABASE_PATH=str(tmp_path / "telemetry.db"),
            MQTT_HOST="mqtt",
            MQTT_CLIENT_ENABLED=True,
        )


def test_shared_environment_keeps_api_mqtt_disabled():
    configured = Settings(_env_file=REPOSITORY_ROOT / ".env.example")

    assert configured.MQTT_CLIENT_ENABLED is False


def test_disabled_mqtt_process_does_not_read_security_files(tmp_path):
    configured = make_settings(
        MQTT_CLIENT_ENABLED=False,
        MQTT_TLS_ENABLED=True,
        MQTT_CA_CERT_PATH=str(tmp_path / "missing-ca.crt"),
        MQTT_USERNAME="collector",
        MQTT_PASSWORD_FILE=str(tmp_path / "missing-password"),
    )

    assert configured.MQTT_CLIENT_ENABLED is False


def test_tls_client_requires_ca_certificate():
    with pytest.raises(ValidationError, match="MQTT_CA_CERT_PATH"):
        make_settings(
            MQTT_CLIENT_ENABLED=True,
            MQTT_TLS_ENABLED=True,
            MQTT_USERNAME="collector",
            MQTT_PASSWORD_FILE="missing.password",
        )


def test_tls_client_requires_complete_authentication(tmp_path):
    ca_certificate = tmp_path / "ca.crt"
    ca_certificate.write_text("test-ca", encoding="utf-8")

    with pytest.raises(ValidationError, match="username and password-file"):
        make_settings(
            MQTT_CLIENT_ENABLED=True,
            MQTT_TLS_ENABLED=True,
            MQTT_CA_CERT_PATH=str(ca_certificate),
        )


@pytest.mark.parametrize(
    ("overrides", "message"),
    [
        ({"MQTT_USERNAME": "collector"}, "configured together"),
        ({"MQTT_PASSWORD_FILE": "collector.password"}, "configured together"),
        (
            {"MQTT_CA_CERT_PATH": "ca.crt"},
            "requires MQTT_TLS_ENABLED=true",
        ),
    ],
)
def test_incoherent_mqtt_security_settings_are_rejected(overrides, message):
    with pytest.raises(ValidationError, match=message):
        make_settings(**overrides)


def test_mqtt_client_rejects_empty_password_file(tmp_path):
    ca_certificate = tmp_path / "ca.crt"
    password_file = tmp_path / "collector.password"
    ca_certificate.write_text("test-ca", encoding="utf-8")
    password_file.write_text("\n", encoding="utf-8")

    with pytest.raises(ValidationError, match="must not be empty"):
        make_settings(
            MQTT_CLIENT_ENABLED=True,
            MQTT_TLS_ENABLED=True,
            MQTT_CA_CERT_PATH=str(ca_certificate),
            MQTT_USERNAME="collector",
            MQTT_PASSWORD_FILE=str(password_file),
        )


@pytest.mark.parametrize("missing_field", ["ca", "password"])
def test_mqtt_client_rejects_missing_security_file(tmp_path, missing_field):
    ca_certificate = tmp_path / "ca.crt"
    password_file = tmp_path / "collector.password"
    ca_certificate.write_text("test-ca", encoding="utf-8")
    password_file.write_text("test-password", encoding="utf-8")

    if missing_field == "ca":
        ca_certificate.unlink()
    else:
        password_file.unlink()

    with pytest.raises(ValidationError, match="readable file"):
        make_settings(
            MQTT_CLIENT_ENABLED=True,
            MQTT_TLS_ENABLED=True,
            MQTT_CA_CERT_PATH=str(ca_certificate),
            MQTT_USERNAME="collector",
            MQTT_PASSWORD_FILE=str(password_file),
        )


def test_production_accepts_authenticated_tls_mqtt_client(tmp_path):
    ca_certificate = tmp_path / "ca.crt"
    password_file = tmp_path / "collector.password"
    ca_certificate.write_text("test-ca", encoding="utf-8")
    password_file.write_text("test-password", encoding="utf-8")

    configured = make_settings(
        APP_ENV="production",
        DATABASE_PATH=str(tmp_path / "telemetry.db"),
        MQTT_HOST="mqtt",
        MQTT_PORT=8883,
        MQTT_CLIENT_ENABLED=True,
        MQTT_TLS_ENABLED=True,
        MQTT_CA_CERT_PATH=str(ca_certificate),
        MQTT_USERNAME="collector",
        MQTT_PASSWORD_FILE=str(password_file),
    )

    assert configured.MQTT_TLS_ENABLED is True
    assert configured.MQTT_USERNAME == "collector"


def test_mqtt_reconnect_range_must_be_ordered():
    with pytest.raises(ValidationError, match="MQTT_RECONNECT_MAX_SECONDS"):
        make_settings(
            MQTT_RECONNECT_MIN_SECONDS=10,
            MQTT_RECONNECT_MAX_SECONDS=5,
        )


def test_telemetry_history_uses_configured_default_limit():
    query_default = inspect.signature(read_telemetry).parameters["limit"].default

    assert query_default.default == settings.DEFAULT_HISTORY_LIMIT
