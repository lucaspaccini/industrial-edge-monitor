import socket
import ssl
from collections.abc import Callable

import paho.mqtt.client as mqtt

from backend.core.config import Settings, settings


MqttClientFactory = Callable[..., mqtt.Client]


def _read_password(config: Settings) -> str:
    try:
        password = config.mqtt_password_path.read_text(encoding="utf-8").strip()
    except OSError as exc:
        raise RuntimeError("MQTT password file is not readable") from exc

    if not password:
        raise RuntimeError("MQTT password file is empty")
    return password


def create_mqtt_client(
    config: Settings = settings,
    *,
    client_factory: MqttClientFactory = mqtt.Client,
) -> mqtt.Client:
    if not config.MQTT_CLIENT_ENABLED:
        raise RuntimeError("MQTT client configuration is not enabled")

    client = client_factory(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=config.MQTT_CLIENT_ID,
    )

    if config.MQTT_USERNAME is not None:
        client.username_pw_set(config.MQTT_USERNAME, _read_password(config))

    if config.MQTT_TLS_ENABLED:
        context = ssl.create_default_context(
            ssl.Purpose.SERVER_AUTH,
            cafile=str(config.mqtt_ca_certificate_path),
        )
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.verify_mode = ssl.CERT_REQUIRED
        context.check_hostname = True
        client.tls_set_context(context)

    client.reconnect_delay_set(
        min_delay=config.MQTT_RECONNECT_MIN_SECONDS,
        max_delay=config.MQTT_RECONNECT_MAX_SECONDS,
    )
    return client


def connection_error_category(exc: BaseException) -> str:
    if isinstance(exc, ssl.SSLCertVerificationError):
        return "tls_certificate_verification"
    if isinstance(exc, ssl.SSLError):
        return "tls_handshake"
    if isinstance(exc, socket.gaierror):
        return "dns_resolution"
    if isinstance(exc, ConnectionRefusedError):
        return "connection_refused"
    return "transport"


def reason_code_category(reason_code: object) -> str:
    normalized = str(reason_code).lower()
    if "username" in normalized or "password" in normalized:
        return "authentication"
    if "authoriz" in normalized:
        return "authorization"
    return "broker_rejected"
