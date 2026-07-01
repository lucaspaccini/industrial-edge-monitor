from fastapi import APIRouter

from backend.api.schemas import TelemetryResponse
from backend.api.services.telemetry_service import (
    get_latest_telemetry,
    get_telemetry_history,
)

router = APIRouter(prefix="/telemetry", tags=["telemetry"])


@router.get("/", response_model=list[TelemetryResponse])
def read_telemetry():
    return get_telemetry_history(limit=100)


@router.get("/latest", response_model=TelemetryResponse)
def read_latest_telemetry():
    return get_latest_telemetry()