from datetime import datetime
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator


DeviceId = str
MachineStatus = Literal["running", "stopped", "unknown"]


class TelemetryMessage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    device_id: DeviceId
    timestamp: datetime
    temperature: float = Field(ge=-40.0, le=85.0)
    humidity: float = Field(ge=0.0, le=100.0)
    machine_status: MachineStatus

    @field_validator("device_id")
    @classmethod
    def valid_device_id(cls, value: str) -> str:
        if not value or len(value) > 63 or not all(
            character.isalnum() or character in "._-" for character in value
        ):
            raise ValueError("invalid device_id")
        return value


class ComponentHealthMessage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    status: Literal["healthy", "degraded", "fault", "unknown"]
    error_code: str | None
    updated_at: datetime | None


class HealthMessage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schema_version: Literal[1]
    device_id: DeviceId
    timestamp: datetime | None
    status: Literal["healthy", "degraded"]
    availability: Literal["online", "offline"]
    components: dict[str, ComponentHealthMessage]
    counters: dict[str, int]
    metrics: dict[str, float | int | None]

    _valid_device_id = field_validator("device_id")(TelemetryMessage.valid_device_id.__func__)


class AvailabilityMessage(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schema_version: Literal[1]
    device_id: DeviceId
    status: Literal["online", "offline"]

    _valid_device_id = field_validator("device_id")(TelemetryMessage.valid_device_id.__func__)
