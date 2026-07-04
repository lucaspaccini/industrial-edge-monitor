from fastapi import APIRouter

from backend.services.health_service import health_service

router = APIRouter(tags=["health"])


@router.get("/health")
def read_health():
    return health_service.get_health_status()