from fastapi import HTTPException

from backend.core.config import settings
from backend.repositories import telemetry_repository


class TelemetryService:
    def __init__(self, repository=telemetry_repository):
        self.repository = repository

    def get_telemetry_history(
        self,
        limit: int = settings.DEFAULT_HISTORY_LIMIT,
    ):
        try:
            return self.repository.fetch_telemetry_history(limit=limit)

        except Exception as exc:
            raise HTTPException(
                status_code=500,
                detail="Failed to read telemetry history",
            ) from exc

    def get_latest_telemetry(self):
        try:
            telemetry = self.repository.fetch_latest_telemetry()

            if telemetry is None:
                raise HTTPException(
                    status_code=404,
                    detail="No telemetry data available",
                )

            return telemetry

        except HTTPException:
            raise

        except Exception as exc:
            raise HTTPException(
                status_code=500,
                detail="Failed to read latest telemetry data",
            ) from exc

    def save_telemetry(self, payload: dict) -> None:
        self.repository.insert_telemetry(payload)
    
    def get_statistics(self):
        try:
            statistics = self.repository.fetch_telemetry_statistics()

            if statistics is None:
                return {
                    "samples": 0,
                    "temperature": {
                        "min": None,
                        "max": None,
                        "avg": None,
                    },
                    "humidity": {
                        "min": None,
                        "max": None,
                        "avg": None,
                    },
                    "last_update": None,
                }

            return {
                "samples": statistics["samples"],
                "temperature": {
                    "min": statistics["min_temperature"],
                    "max": statistics["max_temperature"],
                    "avg": statistics["avg_temperature"],
                },
                "humidity": {
                    "min": statistics["min_humidity"],
                    "max": statistics["max_humidity"],
                    "avg": statistics["avg_humidity"],
                },
                "last_update": statistics["last_update"],
            }

        except Exception as exc:
            raise HTTPException(
                status_code=500,
                detail="Failed to read telemetry statistics",
            ) from exc


telemetry_service = TelemetryService()