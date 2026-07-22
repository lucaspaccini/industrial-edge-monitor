from fastapi import APIRouter

from backend.api.schemas import DeviceHealthResponse, DeviceSummaryResponse
from backend.services.device_service import device_service


router = APIRouter(prefix="/devices", tags=["devices"])


@router.get("/", response_model=list[DeviceSummaryResponse])
def read_devices():
    return device_service.get_devices()


@router.get("/{device_id}/health", response_model=DeviceHealthResponse)
def read_device_health(device_id: str):
    return device_service.get_health(device_id)
