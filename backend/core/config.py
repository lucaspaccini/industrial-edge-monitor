from ipaddress import ip_address
from pathlib import Path
from typing import Annotated, Any, Literal
from urllib.parse import urlsplit

from pydantic import BeforeValidator, Field, field_validator, model_validator
from pydantic_settings import BaseSettings, SettingsConfigDict

from backend.core.validation import validate_device_id


BASE_DIR = Path(__file__).resolve().parents[2]

ApplicationEnvironment = Literal["development", "test", "production"]
LogLevel = Literal["CRITICAL", "ERROR", "WARNING", "INFO", "DEBUG"]


def reject_boolean_number(value: Any) -> Any:
    if isinstance(value, bool):
        raise ValueError("boolean values are not valid numeric configuration")
    return value


Port = Annotated[
    int,
    BeforeValidator(reject_boolean_number),
    Field(ge=1, le=65535),
]
PositiveInt = Annotated[int, BeforeValidator(reject_boolean_number), Field(gt=0)]
PositiveFloat = Annotated[
    float,
    BeforeValidator(reject_boolean_number),
    Field(gt=0, le=60),
]
HistoryLimit = Annotated[
    int,
    BeforeValidator(reject_boolean_number),
    Field(ge=1, le=1000),
]


class Settings(BaseSettings):
    # Application
    APP_ENV: ApplicationEnvironment = "development"
    LOG_LEVEL: LogLevel = "INFO"
    APP_NAME: str = "Industrial Edge Monitor API"
    APP_VERSION: str = "0.1.0"
    APP_DESCRIPTION: str = "REST API for Industrial Edge Monitor"

    # API
    DEFAULT_HISTORY_LIMIT: HistoryLimit = 100

    # Database
    DATABASE_PATH: str = "data/telemetry.db"
    DATABASE_TIMEOUT_SECONDS: PositiveFloat = 30.0

    # MQTT
    MQTT_HOST: str = "localhost"
    MQTT_PORT: Port = 1883
    MQTT_KEEPALIVE_SECONDS: PositiveInt = 60
    MQTT_TOPIC: str = "industrial/telemetry"
    MQTT_TOPIC_PREFIX: str = "industrial/devices"
    LEGACY_DEVICE_ID: str = "legacy-device"
    DEVICE_OFFLINE_TIMEOUT_SECONDS: PositiveInt = 150

    # CORS
    CORS_ORIGINS: str = "http://localhost:3000,http://127.0.0.1:3000"

    model_config = SettingsConfigDict(
        env_file=BASE_DIR / ".env",
        env_file_encoding="utf-8",
        extra="ignore",
    )

    @field_validator("DATABASE_PATH")
    @classmethod
    def validate_database_path(cls, value: str) -> str:
        normalized = value.strip()
        if not normalized:
            raise ValueError("DATABASE_PATH must not be empty")
        return normalized

    @field_validator("MQTT_HOST")
    @classmethod
    def validate_mqtt_host(cls, value: str) -> str:
        normalized = value.strip()
        if (
            not normalized
            or "://" in normalized
            or "/" in normalized
            or any(character.isspace() for character in normalized)
        ):
            raise ValueError(
                "MQTT_HOST must be a hostname or IP address without a URL scheme"
            )
        return normalized

    @field_validator("MQTT_TOPIC", "MQTT_TOPIC_PREFIX")
    @classmethod
    def validate_mqtt_topic(cls, value: str) -> str:
        normalized = value.strip()
        if (
            not normalized
            or normalized.startswith("/")
            or normalized.endswith("/")
            or "+" in normalized
            or "#" in normalized
            or "\x00" in normalized
        ):
            raise ValueError(
                "MQTT topics must be non-empty concrete topic paths without wildcards"
            )
        return normalized

    @field_validator("LEGACY_DEVICE_ID")
    @classmethod
    def validate_legacy_device_id(cls, value: str) -> str:
        return validate_device_id(value)

    @field_validator("CORS_ORIGINS")
    @classmethod
    def validate_cors_origins(cls, value: str) -> str:
        origins = [origin.strip().rstrip("/") for origin in value.split(",")]
        if not origins or any(not origin for origin in origins):
            raise ValueError("CORS_ORIGINS must contain at least one origin")

        for origin in origins:
            try:
                parsed = urlsplit(origin)
                port = parsed.port
            except ValueError as exc:
                raise ValueError(f"invalid CORS origin: {origin}") from exc
            if (
                parsed.scheme not in {"http", "https"}
                or parsed.hostname is None
                or parsed.username is not None
                or parsed.password is not None
                or parsed.path
                or parsed.query
                or parsed.fragment
                or (port is not None and not 1 <= port <= 65535)
            ):
                raise ValueError(f"invalid CORS origin: {origin}")

        return ",".join(dict.fromkeys(origins))

    @model_validator(mode="after")
    def validate_environment_configuration(self):
        if self.APP_ENV == "production":
            configured_path = Path(self.DATABASE_PATH).expanduser()
            database_path = (
                configured_path
                if configured_path.is_absolute()
                else BASE_DIR / configured_path
            ).resolve()
            if (
                self.DATABASE_PATH == ":memory:"
                or database_path.is_relative_to(BASE_DIR)
            ):
                raise ValueError(
                    "DATABASE_PATH must be explicitly configured outside the source tree "
                    "in production"
                )
            mqtt_host = self.MQTT_HOST.lower().rstrip(".").strip("[]")
            try:
                mqtt_host_is_loopback = ip_address(mqtt_host).is_loopback
            except ValueError:
                mqtt_host_is_loopback = mqtt_host == "localhost"
            if mqtt_host_is_loopback:
                raise ValueError(
                    "MQTT_HOST must be explicitly configured for production"
                )
        return self

    @property
    def cors_origins_list(self) -> list[str]:
        return self.CORS_ORIGINS.split(",")


settings = Settings()
