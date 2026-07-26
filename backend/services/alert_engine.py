from backend.api.database import get_connection
from backend.core.logging import get_logger
from backend.domain.alerts import (
    RuleCondition,
    is_recovery,
    is_violation,
    parse_utc_timestamp,
    telemetry_metric_value,
    update_extreme,
)
from backend.repositories import alert_repository


logger = get_logger(__name__)


class AlertEngine:
    def evaluate(self, sample: dict) -> None:
        event_time = parse_utc_timestamp(sample["timestamp"])
        event_time_text = event_time.isoformat().replace("+00:00", "Z")

        with get_connection() as conn:
            rules = alert_repository.fetch_enabled_rules_for_device(
                conn,
                sample["device_id"],
            )
            for rule in rules:
                self._evaluate_rule(
                    conn,
                    rule,
                    sample,
                    event_time,
                    event_time_text,
                )

    def _evaluate_rule(
        self,
        conn,
        rule: dict,
        sample: dict,
        event_time,
        event_time_text: str,
    ) -> None:
        value = telemetry_metric_value(sample, rule["metric"])
        if rule["last_telemetry_id"] == sample["id"]:
            logger.warning(
                "Alert evaluation telemetry_id=%s device_id=%s rule_id=%s "
                "metric=%s value=%s previous_state=%s resulting_state=%s "
                "outcome=duplicate_ignored",
                sample["id"],
                sample["device_id"],
                rule["id"],
                rule["metric"],
                value,
                rule["runtime_state"],
                rule["runtime_state"],
            )
            return

        if rule["last_evaluated_at"] is not None:
            last_evaluated = parse_utc_timestamp(rule["last_evaluated_at"])
            if event_time <= last_evaluated:
                logger.warning(
                    "Alert evaluation telemetry_id=%s device_id=%s rule_id=%s "
                    "metric=%s value=%s previous_state=%s resulting_state=%s "
                    "outcome=out_of_order_ignored timestamp=%s",
                    sample["id"],
                    sample["device_id"],
                    rule["id"],
                    rule["metric"],
                    value,
                    rule["runtime_state"],
                    rule["runtime_state"],
                    event_time_text,
                )
                return

        condition = RuleCondition(
            metric=rule["metric"],
            operator=rule["operator"],
            threshold=rule["threshold"],
            hysteresis=rule["hysteresis"],
        )
        violation = is_violation(condition, value)
        state = rule["runtime_state"]

        if state == "normal":
            if violation and rule["duration_seconds"] == 0:
                alert_repository.create_event(
                    conn,
                    rule,
                    started_at=event_time_text,
                    activated_at=event_time_text,
                    value=value,
                    extreme_value=value,
                )
                next_state = "active"
                pending_since = None
                pending_extreme = None
            elif violation:
                next_state = "pending"
                pending_since = event_time_text
                pending_extreme = value
            else:
                next_state = "normal"
                pending_since = None
                pending_extreme = None

        elif state == "pending":
            if not violation:
                next_state = "normal"
                pending_since = None
                pending_extreme = None
            else:
                pending_extreme = update_extreme(
                    rule["operator"],
                    rule["pending_extreme_value"]
                    if rule["pending_extreme_value"] is not None
                    else rule["last_value"],
                    value,
                )
                pending_time = parse_utc_timestamp(rule["pending_since"])
                elapsed = (event_time - pending_time).total_seconds()
                if elapsed >= rule["duration_seconds"]:
                    alert_repository.create_event(
                        conn,
                        rule,
                        started_at=rule["pending_since"],
                        activated_at=event_time_text,
                        value=value,
                        extreme_value=pending_extreme,
                    )
                    next_state = "active"
                    pending_since = None
                    pending_extreme = None
                else:
                    next_state = "pending"
                    pending_since = rule["pending_since"]

        else:
            active_event = alert_repository.fetch_active_event(conn, rule["id"])
            if active_event is None:
                raise RuntimeError(
                    f"rule {rule['id']} is active without an active event"
                )
            extreme = update_extreme(
                rule["operator"],
                active_event["extreme_value"],
                value,
            )
            if is_recovery(condition, value):
                alert_repository.resolve_active_event(
                    conn,
                    active_event["id"],
                    resolved_at=event_time_text,
                    last_value=value,
                    reason="condition_recovered",
                )
                next_state = "normal"
                pending_since = None
                pending_extreme = None
            else:
                alert_repository.update_active_event(
                    conn,
                    active_event["id"],
                    last_value=value,
                    extreme_value=extreme,
                )
                next_state = "active"
                pending_since = None
                pending_extreme = None

        alert_repository.update_runtime_state(
            conn,
            rule["id"],
            state=next_state,
            pending_since=pending_since,
            pending_extreme_value=pending_extreme,
            evaluated_at=event_time_text,
            value=value,
            telemetry_id=sample["id"],
        )
        outcome = (
            "transition"
            if state != next_state
            else "active_updated" if state == "active" else "observed"
        )
        log = logger.info if state != next_state else logger.debug
        log(
            "Alert evaluation telemetry_id=%s device_id=%s rule_id=%s "
            "metric=%s value=%s previous_state=%s resulting_state=%s outcome=%s",
            sample["id"],
            sample["device_id"],
            rule["id"],
            rule["metric"],
            value,
            state,
            next_state,
            outcome,
        )


alert_engine = AlertEngine()
