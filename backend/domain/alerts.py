from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Literal


AlertMetric = Literal["temperature", "humidity"]
AlertOperator = Literal["greater_than", "less_than"]
AlertSeverity = Literal["info", "warning", "critical"]
RuleRuntimeState = Literal["normal", "pending", "active"]
AlertEventStatus = Literal["active", "resolved"]
ResolutionReason = Literal[
    "condition_recovered",
    "rule_disabled",
    "rule_updated",
    "rule_archived",
]


@dataclass(frozen=True)
class RuleCondition:
    metric: AlertMetric
    operator: AlertOperator
    threshold: float
    hysteresis: float


def is_violation(condition: RuleCondition, value: float) -> bool:
    if condition.operator == "greater_than":
        return value > condition.threshold
    return value < condition.threshold


def is_recovery(condition: RuleCondition, value: float) -> bool:
    if condition.operator == "greater_than":
        return value <= condition.threshold - condition.hysteresis
    return value >= condition.threshold + condition.hysteresis


def update_extreme(operator: AlertOperator, current: float, value: float) -> float:
    return max(current, value) if operator == "greater_than" else min(current, value)


def telemetry_metric_value(sample: dict, metric: AlertMetric) -> float:
    if metric == "temperature":
        return float(sample["temperature"])
    if metric == "humidity":
        return float(sample["humidity"])
    raise ValueError(f"unsupported alert metric: {metric}")


def parse_utc_timestamp(value: str) -> datetime:
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise ValueError("timestamp must be timezone-aware")
    return parsed.astimezone(timezone.utc)


def utc_isoformat(value: datetime) -> str:
    if value.tzinfo is None or value.utcoffset() is None:
        raise ValueError("timestamp must be timezone-aware")
    return value.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")
