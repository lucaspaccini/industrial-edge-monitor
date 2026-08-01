from fastapi import APIRouter, Query

from backend.api.schemas import TelemetryResponse, TelemetryStatisticsResponse
from backend.core.config import settings
from backend.services.telemetry_service import telemetry_service

router = APIRouter(prefix="/telemetry", tags=["telemetry"])


@router.get("/", response_model=list[TelemetryResponse])
def read_telemetry(
    device_id: str | None = Query(default=None),
    limit: int = Query(default=settings.DEFAULT_HISTORY_LIMIT, ge=1, le=1000),
):
    return telemetry_service.get_telemetry_history(limit=limit, device_id=device_id)


@router.get("/latest", response_model=TelemetryResponse)
def read_latest_telemetry(device_id: str | None = Query(default=None)):
    return telemetry_service.get_latest_telemetry(device_id=device_id)


@router.get("/statistics", response_model=TelemetryStatisticsResponse)
def read_telemetry_statistics(device_id: str | None = Query(default=None)):
    return telemetry_service.get_statistics(device_id=device_id)
