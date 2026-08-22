from datetime import datetime
from typing import Annotated, Literal

from pydantic import BeforeValidator, BaseModel, ConfigDict, Field, field_validator

from backend.core.validation import validate_device_id, validate_device_timestamp


DeviceId = str
MachineStatus = Literal["running", "stopped", "unknown"]
DiagnosticErrorCode = Literal[
    "none",
    "not_initialized",
    "communication_failed",
    "read_failed",
    "value_not_finite",
    "temperature_out_of_range",
    "humidity_out_of_range",
    "time_not_synchronized",
    "mqtt_disconnected",
    "publish_failed",
    "machine_status_unavailable",
]
StrictNonNegativeInt = Annotated[int, Field(strict=True, ge=0)]
StrictFiniteInt = Annotated[int, Field(strict=True)]
StrictFiniteFloat = Annotated[float, Field(strict=True, allow_inf_nan=False)]
MetricValue = StrictFiniteInt | StrictFiniteFloat | None
DeviceTimestamp = Annotated[datetime, BeforeValidator(validate_device_timestamp)]
Temperature = Annotated[
    float, Field(strict=True, ge=-40.0, le=85.0, allow_inf_nan=False)
]
Humidity = Annotated[
    float, Field(strict=True, ge=0.0, le=100.0, allow_inf_nan=False)
]


class TelemetryMessage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    device_id: DeviceId
    timestamp: DeviceTimestamp
    temperature: Temperature
    humidity: Humidity
    machine_status: MachineStatus

    @field_validator("device_id")
    @classmethod
    def valid_device_id(cls, value: str) -> str:
        return validate_device_id(value)


class ComponentHealthMessage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    status: Literal["healthy", "degraded", "fault", "unknown"]
    error_code: DiagnosticErrorCode
    updated_at: DeviceTimestamp | None


class HealthMessage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schema_version: Literal[1]
    device_id: DeviceId
    timestamp: DeviceTimestamp | None
    status: Literal["healthy", "degraded"]
    availability: Literal["online", "offline"]
    components: dict[str, ComponentHealthMessage]
    counters: dict[str, StrictNonNegativeInt]
    metrics: dict[str, MetricValue]

    _valid_device_id = field_validator("device_id")(TelemetryMessage.valid_device_id.__func__)


class AvailabilityMessage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schema_version: Literal[1]
    device_id: DeviceId
    status: Literal["online", "offline"]

    _valid_device_id = field_validator("device_id")(TelemetryMessage.valid_device_id.__func__)
