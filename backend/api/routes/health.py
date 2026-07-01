from fastapi import APIRouter

from backend.api.services.health_service import get_health_status

router = APIRouter(tags=["health"])


@router.get("/health")
def read_health():
    return get_health_status()