from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from backend.api.routes.telemetry import router as telemetry_router
from backend.api.config import settings

app = FastAPI(
    title=settings.APP_NAME,
    version=settings.APP_VERSION,
    description=settings.APP_DESCRIPTION,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:3000",
        "http://127.0.0.1:3000",
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
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