import logging

from backend.core.config import settings


LOG_FORMAT = "%(asctime)s | %(levelname)s | %(name)s | %(message)s"


def configure_logging() -> None:
    level = getattr(logging, settings.LOG_LEVEL)
    logging.basicConfig(
        level=level,
        format=LOG_FORMAT,
    )
    logging.getLogger().setLevel(level)


def get_logger(name: str) -> logging.Logger:
    return logging.getLogger(name)
