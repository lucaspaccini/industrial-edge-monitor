import inspect

import pytest
from pydantic import ValidationError

from backend.api.routes.telemetry import read_telemetry
from backend.core.config import Settings, settings


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


def test_telemetry_history_uses_configured_default_limit():
    query_default = inspect.signature(read_telemetry).parameters["limit"].default

    assert query_default.default == settings.DEFAULT_HISTORY_LIMIT
