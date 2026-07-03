from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from backend.api.routes.telemetry import router as telemetry_router
from backend.api.routes.health import router as health_router

from backend.core.config import settings

from backend.core.logging import configure_logging, get_logger

from backend.database.init_db import initialize_database

configure_logging()
logger = get_logger(__name__)

app = FastAPI(
    title=settings.APP_NAME,
    version=settings.APP_VERSION,
    description=settings.APP_DESCRIPTION,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins_list,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(telemetry_router)
app.include_router(health_router)

initialize_database()

logger.info("Industrial Edge Monitor API started")

@app.get("/")

def root():
    return {
        "service": settings.APP_NAME,
        "version": settings.APP_VERSION,
        "description": settings.APP_DESCRIPTION,
        "message": "Welcome to the Industrial Edge Monitor API!",
        "api_status": "running"
    }