from pydantic import BaseModel


class TelemetryResponse(BaseModel):
    id: int
    timestamp: str
    temperature: float
    humidity: float
    machine_status: str