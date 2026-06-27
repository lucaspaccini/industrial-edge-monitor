from pathlib import Path
from pydantic_settings import BaseSettings, SettingsConfigDict


BASE_DIR = Path(__file__).resolve().parents[2]


class Settings(BaseSettings):
    APP_NAME: str = "Industrial Edge Monitor API"
    APP_VERSION: str = "0.1.0"
    APP_DESCRIPTION: str = "REST API for Industrial Edge Monitor"

    DATABASE_PATH: str = "data/telemetry.db"

    MQTT_HOST: str = "localhost"
    MQTT_PORT: int = 1883
    MQTT_TOPIC: str = "industrial/telemetry"

    model_config = SettingsConfigDict(
        env_file=BASE_DIR / ".env",
        env_file_encoding="utf-8",
    )


settings = Settings()