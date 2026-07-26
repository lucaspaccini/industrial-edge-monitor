from datetime import datetime
from typing import Annotated

from pydantic import BaseModel, ConfigDict, Field, StrictBool, field_validator, model_validator

from backend.core.validation import validate_device_id
from backend.domain.alerts import (
    AlertEventStatus,
    AlertMetric,
    AlertOperator,
    AlertSeverity,
    ResolutionReason,
    RuleRuntimeState,
)


StrictFiniteInt = Annotated[int, Field(strict=True)]
StrictFiniteFloat = Annotated[float, Field(strict=True, allow_inf_nan=False)]
StrictFiniteNumber = StrictFiniteInt | StrictFiniteFloat
StrictNonNegativeIntNumber = Annotated[int, Field(strict=True, ge=0)]
StrictNonNegativeFloat = Annotated[
    float,
    Field(strict=True, ge=0, allow_inf_nan=False),
]
StrictNonNegativeNumber = StrictNonNegativeIntNumber | StrictNonNegativeFloat
StrictNonNegativeInt = Annotated[int, Field(strict=True, ge=0)]


class AlertRuleDefinition(BaseModel):
    model_config = ConfigDict(extra="forbid")

    name: str = Field(min_length=1, max_length=100)
    device_id: str
    metric: AlertMetric
    operator: AlertOperator
    threshold: StrictFiniteNumber
    duration_seconds: StrictNonNegativeInt = 0
    hysteresis: StrictNonNegativeNumber = 0.0
    severity: AlertSeverity
    enabled: StrictBool = True

    @field_validator("name")
    @classmethod
    def validate_name(cls, value: str) -> str:
        normalized = value.strip()
        if not normalized:
            raise ValueError("name must not be blank")
        return normalized

    @field_validator("device_id")
    @classmethod
    def validate_rule_device_id(cls, value: str) -> str:
        return validate_device_id(value)

    @model_validator(mode="after")
    def validate_threshold_range(self):
        limits = {
            "temperature": (-40.0, 85.0),
            "humidity": (0.0, 100.0),
        }
        minimum, maximum = limits[self.metric]
        if not minimum <= self.threshold <= maximum:
            raise ValueError(
                f"threshold for {self.metric} must be between {minimum} and {maximum}"
            )
        return self


class AlertRuleCreate(AlertRuleDefinition):
    pass


class AlertRulePatch(BaseModel):
    model_config = ConfigDict(extra="forbid")

    name: str | None = Field(default=None, min_length=1, max_length=100)
    metric: AlertMetric | None = None
    operator: AlertOperator | None = None
    threshold: StrictFiniteNumber | None = None
    duration_seconds: StrictNonNegativeInt | None = None
    hysteresis: StrictNonNegativeNumber | None = None
    severity: AlertSeverity | None = None
    enabled: StrictBool | None = None

    @field_validator("name")
    @classmethod
    def validate_name(cls, value: str | None) -> str | None:
        if value is None:
            return None
        normalized = value.strip()
        if not normalized:
            raise ValueError("name must not be blank")
        return normalized


class AlertRuleResponse(AlertRuleDefinition):
    model_config = ConfigDict(extra="ignore")

    id: int
    created_at: datetime
    updated_at: datetime
    archived_at: datetime | None
    runtime_state: RuleRuntimeState
    pending_since: datetime | None
    last_evaluated_at: datetime | None
    last_value: float | None


class AlertEventResponse(BaseModel):
    id: int
    rule_id: int
    device_id: str
    rule_name: str
    metric: AlertMetric
    operator: AlertOperator
    threshold: float
    hysteresis: float
    duration_seconds: int
    severity: AlertSeverity
    status: AlertEventStatus
    started_at: datetime
    activated_at: datetime
    resolved_at: datetime | None
    resolution_reason: ResolutionReason | None
    activation_value: float
    last_value: float
    extreme_value: float
