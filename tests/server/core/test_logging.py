import logging

from backend.core.config import settings
from backend.core.logging import configure_logging


def test_configure_logging_applies_configured_level(monkeypatch):
    root_logger = logging.getLogger()
    previous_level = root_logger.level
    monkeypatch.setattr(settings, "LOG_LEVEL", "DEBUG")

    try:
        configure_logging()
        assert root_logger.level == logging.DEBUG
    finally:
        root_logger.setLevel(previous_level)
