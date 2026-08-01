from pathlib import Path
import sqlite3

from backend.core.config import settings


BASE_DIR = Path(__file__).resolve().parents[2]


def get_connection():
    configured_path = Path(settings.DATABASE_PATH).expanduser()

    if configured_path == Path(":memory:"):
        db_path: Path | str = ":memory:"
    else:
        db_path = (
            configured_path
            if configured_path.is_absolute()
            else BASE_DIR / configured_path
        ).resolve()
        db_path.parent.mkdir(parents=True, exist_ok=True)

    conn = sqlite3.connect(
        db_path,
        timeout=settings.DATABASE_TIMEOUT_SECONDS,
    )
    conn.row_factory = sqlite3.Row
    conn.execute(
        f"PRAGMA busy_timeout = {int(settings.DATABASE_TIMEOUT_SECONDS * 1000)}"
    )
    conn.execute("PRAGMA foreign_keys = ON")
    if db_path != ":memory:":
        conn.execute("PRAGMA journal_mode = WAL")
        conn.execute("PRAGMA synchronous = NORMAL")

    return conn
