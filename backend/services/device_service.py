from datetime import datetime, timedelta, timezone

from fastapi import HTTPException

from backend.core.config import settings
from backend.repositories import device_repository


def _effective_availability(reported: str, last_seen: str | None) -> str:
    if reported != "online" or last_seen is None:
        return "offline"
    seen = datetime.fromisoformat(last_seen.replace("Z", "+00:00"))
    deadline = datetime.now(timezone.utc) - timedelta(
        seconds=settings.DEVICE_OFFLINE_TIMEOUT_SECONDS
    )
    return "online" if seen >= deadline else "offline"


class DeviceService:
    def get_devices(self) -> list[dict]:
        result = []
        for device in device_repository.fetch_devices():
            device["availability"] = _effective_availability(
                device["reported_availability"], device["last_seen"]
            )
            result.append(device)
        return result

    def get_health(self, device_id: str) -> dict:
        record = device_repository.fetch_health(device_id)
        if record is None:
            raise HTTPException(status_code=404, detail="No health data available")
        payload = record.pop("payload")
        payload.update(record)
        payload["availability"] = _effective_availability(
            payload["reported_availability"], payload["last_seen"]
        )
        return payload


device_service = DeviceService()
