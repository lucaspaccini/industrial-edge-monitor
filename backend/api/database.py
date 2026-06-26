from pathlib import Path
import sqlite3


BASE_DIR = Path(__file__).resolve().parents[2]
DB_PATH = BASE_DIR / "data" / "telemetry.db"


def get_connection():

    conn = sqlite3.connect(DB_PATH);
    conn.row_factory = sqlite3.Row;

    return conn
