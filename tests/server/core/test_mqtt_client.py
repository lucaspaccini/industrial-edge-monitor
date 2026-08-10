import ssl

import pytest

from backend.core.config import Settings
from backend.mqtt.client import create_mqtt_client


class FakeClient:
    def __init__(self, callback_api_version, client_id):
        self.callback_api_version = callback_api_version
        self.client_id = client_id
        self.username = None
        self.password = None
        self.tls_context = None
        self.reconnect_delays = None

    def username_pw_set(self, username, password):
        self.username = username
        self.password = password

    def tls_set_context(self, context):
        self.tls_context = context

    def reconnect_delay_set(self, min_delay, max_delay):
        self.reconnect_delays = (min_delay, max_delay)


def secure_settings(tmp_path, **overrides) -> Settings:
    ca_certificate = tmp_path / "ca.crt"
    password_file = tmp_path / "collector.password"
    ca_certificate.write_text(
        "-----BEGIN CERTIFICATE-----\ninvalid-test-data\n-----END CERTIFICATE-----\n",
        encoding="utf-8",
    )
    password_file.write_text("super-secret-test-value\n", encoding="utf-8")
    values = {
        "APP_ENV": "test",
        "MQTT_HOST": "mqtt.test",
        "MQTT_PORT": 8883,
        "MQTT_CLIENT_ENABLED": True,
        "MQTT_TLS_ENABLED": True,
        "MQTT_CA_CERT_PATH": str(ca_certificate),
        "MQTT_USERNAME": "collector",
        "MQTT_PASSWORD_FILE": str(password_file),
        "MQTT_CLIENT_ID": "collector-test",
        "CORS_ORIGINS": "http://localhost:3000",
    }
    values.update(overrides)
    return Settings(_env_file=None, **values)


def test_client_configures_authentication_tls_and_reconnect(tmp_path, monkeypatch):
    config = secure_settings(tmp_path)
    expected_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)

    monkeypatch.setattr(
        "backend.mqtt.client.ssl.create_default_context",
        lambda purpose, cafile: expected_context,
    )

    client = create_mqtt_client(config, client_factory=FakeClient)

    assert client.client_id == "collector-test"
    assert client.username == "collector"
    assert client.password == "super-secret-test-value"
    assert client.tls_context is expected_context
    assert expected_context.verify_mode == ssl.CERT_REQUIRED
    assert expected_context.check_hostname is True
    assert expected_context.minimum_version == ssl.TLSVersion.TLSv1_2
    assert client.reconnect_delays == (1, 30)


def test_password_is_not_exposed_when_read_fails(tmp_path):
    config = secure_settings(tmp_path)
    config.mqtt_password_path.unlink()

    with pytest.raises(RuntimeError) as error:
        create_mqtt_client(config, client_factory=FakeClient)

    assert "super-secret-test-value" not in str(error.value)


def test_simulator_uses_its_dedicated_identity(tmp_path, monkeypatch):
    config = secure_settings(
        tmp_path,
        MQTT_USERNAME="simulator",
        MQTT_CLIENT_ID="industrial-edge-simulator",
    )
    expected_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    monkeypatch.setattr(
        "backend.mqtt.client.ssl.create_default_context",
        lambda purpose, cafile: expected_context,
    )

    client = create_mqtt_client(config, client_factory=FakeClient)

    assert client.username == "simulator"
    assert client.client_id == "industrial-edge-simulator"


def test_disabled_mqtt_client_is_rejected():
    config = Settings(
        _env_file=None,
        APP_ENV="test",
        MQTT_CLIENT_ENABLED=False,
        CORS_ORIGINS="http://localhost:3000",
    )

    with pytest.raises(RuntimeError, match="not enabled"):
        create_mqtt_client(config, client_factory=FakeClient)
