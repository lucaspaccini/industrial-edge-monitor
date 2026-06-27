from pathlib import Path
import sqlite3

from backend.api.config import settings


BASE_DIR = Path(__file__).resolve().parents[2]


def get_connection():
    db_path = BASE_DIR / settings.DATABASE_PATH

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    return conn