from fastapi import HTTPException

from backend.api.repositories.telemetry_repository import (
    fetch_latest_telemetry,
    fetch_telemetry_history,
)


def get_telemetry_history(limit: int = 100):
    try:
        return fetch_telemetry_history(limit=limit)

    except Exception as exc:
        raise HTTPException(
            status_code=500,
            detail="Failed to read telemetry history",
        ) from exc


def get_latest_telemetry():
    try:
        telemetry = fetch_latest_telemetry()

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