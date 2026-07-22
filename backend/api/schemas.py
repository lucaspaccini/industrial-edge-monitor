from pydantic import BaseModel


class TelemetryResponse(BaseModel):
    id: int
    device_id: str
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


class DeviceSummaryResponse(BaseModel):
    device_id: str
    availability: str
    reported_availability: str
    last_seen: str | None


class DeviceHealthResponse(BaseModel):
    device_id: str
    timestamp: str | None
    received_at: str
    status: str
    availability: str
    reported_availability: str
    last_seen: str | None
    components: dict
    counters: dict
    metrics: dict
