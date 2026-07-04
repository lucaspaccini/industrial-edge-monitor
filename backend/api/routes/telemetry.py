from fastapi import APIRouter

from backend.api.schemas import TelemetryResponse, TelemetryStatisticsResponse
from backend.services.telemetry_service import telemetry_service

router = APIRouter(prefix="/telemetry", tags=["telemetry"])


@router.get("/", response_model=list[TelemetryResponse])
def read_telemetry():
    return telemetry_service.get_telemetry_history()


@router.get("/latest", response_model=TelemetryResponse)
def read_latest_telemetry():
    return telemetry_service.get_latest_telemetry()

@router.get("/statistics", response_model=TelemetryStatisticsResponse)
def read_telemetry_statistics():
    return telemetry_service.get_statistics()