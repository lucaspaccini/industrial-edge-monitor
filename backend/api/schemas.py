from pydantic import BaseModel


class TelemetryResponse(BaseModel):
    id: int
    timestamp: str
    temperature: float
    humidity: float
    machine_status: str

class MetricStatistics(BaseModel):
    min: float | None
    max: float | None
    avg: float | None


class TelemetryStatisticsResponse(BaseModel):
    samples: int
    temperature: MetricStatistics
    humidity: MetricStatistics
    last_update: str | None