from fastapi import FastAPI

from backend.api.routes.telemetry import router as telemetry_router
from backend.api.config import settings

app = FastAPI(
    title=settings.APP_NAME,
    version=settings.APP_VERSION,
    description=settings.APP_DESCRIPTION,
)

app.include_router(telemetry_router)

@app.get("/")

def root():
    return {
        "service": settings.APP_NAME,
        "version": settings.APP_VERSION,
        "description": settings.APP_DESCRIPTION,
        "message": "Welcome to the Industrial Edge Monitor API!",
        "api_status": "running"
    }