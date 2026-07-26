import sqlite3
from datetime import datetime, timezone

from fastapi import HTTPException

from backend.api.alert_schemas import AlertRuleCreate, AlertRuleDefinition, AlertRulePatch
from backend.repositories import alert_repository


def _now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


class AlertService:
    def create_rule(self, request: AlertRuleCreate) -> dict:
        if not alert_repository.device_exists(request.device_id):
            raise HTTPException(status_code=404, detail="Device not found")
        try:
            return alert_repository.create_rule(request.model_dump(), _now())
        except sqlite3.IntegrityError as exc:
            raise HTTPException(
                status_code=409,
                detail="A rule with this name already exists for the device",
            ) from exc

    def get_rule(self, rule_id: int) -> dict:
        rule = alert_repository.fetch_rule(rule_id)
        if rule is None:
            raise HTTPException(status_code=404, detail="Alert rule not found")
        return rule

    def get_rules(
        self,
        device_id: str | None = None,
        enabled: bool | None = None,
        include_archived: bool = False,
    ) -> list[dict]:
        return alert_repository.fetch_rules(
            device_id=device_id,
            enabled=enabled,
            include_archived=include_archived,
        )

    def update_rule(self, rule_id: int, request: AlertRulePatch) -> dict:
        current = self.get_rule(rule_id)
        if current["archived_at"] is not None:
            raise HTTPException(status_code=404, detail="Alert rule not found")
        changes = request.model_dump(exclude_unset=True, exclude_none=True)
        if not changes:
            return current

        definition_fields = {
            "name",
            "device_id",
            "metric",
            "operator",
            "threshold",
            "duration_seconds",
            "hysteresis",
            "severity",
            "enabled",
        }
        merged = {
            field: changes.get(field, current[field])
            for field in definition_fields
        }
        AlertRuleDefinition.model_validate(merged)

        substantial_fields = {
            "metric",
            "operator",
            "threshold",
            "duration_seconds",
            "hysteresis",
        }
        substantial_change = any(
            field in changes and changes[field] != current[field]
            for field in substantial_fields
        )
        disabling = (
            changes.get("enabled") is False
            and current["enabled"] is True
        )
        reset_runtime = substantial_change or disabling
        resolution_reason = (
            "rule_disabled"
            if disabling
            else "rule_updated" if substantial_change else None
        )

        try:
            return alert_repository.update_rule_and_runtime(
                rule_id,
                changes,
                _now(),
                reset_runtime,
                resolution_reason,
            )
        except sqlite3.IntegrityError as exc:
            raise HTTPException(
                status_code=409,
                detail="Rule update conflicts with an existing rule",
            ) from exc

    def archive_rule(self, rule_id: int) -> None:
        current = self.get_rule(rule_id)
        if current["archived_at"] is not None:
            raise HTTPException(status_code=404, detail="Alert rule not found")
        alert_repository.archive_rule(rule_id, _now())

    def get_events(
        self,
        *,
        device_id: str | None = None,
        status: str | None = None,
        severity: str | None = None,
        rule_id: int | None = None,
        limit: int = 100,
    ) -> list[dict]:
        return alert_repository.fetch_events(
            device_id=device_id,
            status=status,
            severity=severity,
            rule_id=rule_id,
            limit=limit,
        )

    def get_event(self, event_id: int) -> dict:
        event = alert_repository.fetch_event(event_id)
        if event is None:
            raise HTTPException(status_code=404, detail="Alert event not found")
        return event


alert_service = AlertService()
