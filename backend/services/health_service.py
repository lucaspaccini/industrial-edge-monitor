from backend.core.config import settings
from backend.api.database import get_connection


class HealthService:
    def get_health_status(self):
        database_status = "disconnected"

        conn = None

        try:
            conn = get_connection()
            conn.execute("SELECT 1")
            database_status = "connected"

        finally:
            if conn is not None:
                conn.close()

        return {
            "status": "healthy" if database_status == "connected" else "degraded",
            "service": settings.APP_NAME,
            "version": settings.APP_VERSION,
            "database": database_status,
        }


health_service = HealthService()