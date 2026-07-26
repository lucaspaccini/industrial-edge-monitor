import pytest

from backend.core.config import settings
from backend.database.init_db import initialize_database


@pytest.fixture
def isolated_database(tmp_path, monkeypatch):
    database_path = tmp_path / "database" / "telemetry.db"
    monkeypatch.setattr(settings, "DATABASE_PATH", str(database_path))
    initialize_database()
    return database_path
