from fastapi import FastAPI
from backend.api.routes.telemetry import router as telemetry_router

app = FastAPI(
    title="Industrial Edge Monitor API", 
    version="0.1.0", 
    description="REST API for Industrial Edge Monitor"
)

app.include_router(telemetry_router)

@app.get("/")

def root():
    return {"message": "Welcome to the Industrial Edge Monitor API!"}