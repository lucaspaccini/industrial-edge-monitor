from fastapi import APIRouter, Query, Response

from backend.api.alert_schemas import (
    AlertEventResponse,
    AlertRuleCreate,
    AlertRulePatch,
    AlertRuleResponse,
)
from backend.domain.alerts import AlertEventStatus, AlertSeverity
from backend.services.alert_service import alert_service


rules_router = APIRouter(prefix="/alert-rules", tags=["alert-rules"])
alerts_router = APIRouter(tags=["alerts"])


@rules_router.get("/", response_model=list[AlertRuleResponse])
def read_alert_rules(
    device_id: str | None = None,
    enabled: bool | None = None,
    include_archived: bool = False,
):
    return alert_service.get_rules(
        device_id=device_id,
        enabled=enabled,
        include_archived=include_archived,
    )


@rules_router.post("/", response_model=AlertRuleResponse, status_code=201)
def create_alert_rule(request: AlertRuleCreate):
    return alert_service.create_rule(request)


@rules_router.get("/{rule_id}", response_model=AlertRuleResponse)
def read_alert_rule(rule_id: int):
    return alert_service.get_rule(rule_id)


@rules_router.patch("/{rule_id}", response_model=AlertRuleResponse)
def update_alert_rule(rule_id: int, request: AlertRulePatch):
    return alert_service.update_rule(rule_id, request)


@rules_router.delete("/{rule_id}", status_code=204)
def archive_alert_rule(rule_id: int):
    alert_service.archive_rule(rule_id)
    return Response(status_code=204)


@alerts_router.get("/alerts/active", response_model=list[AlertEventResponse])
def read_active_alerts(
    device_id: str | None = None,
    severity: AlertSeverity | None = None,
    rule_id: int | None = None,
    limit: int = Query(default=100, ge=1, le=500),
):
    return alert_service.get_events(
        device_id=device_id,
        status="active",
        severity=severity,
        rule_id=rule_id,
        limit=limit,
    )


@alerts_router.get("/alert-events", response_model=list[AlertEventResponse])
def read_alert_events(
    device_id: str | None = None,
    status: AlertEventStatus | None = None,
    severity: AlertSeverity | None = None,
    rule_id: int | None = None,
    limit: int = Query(default=100, ge=1, le=500),
):
    return alert_service.get_events(
        device_id=device_id,
        status=status,
        severity=severity,
        rule_id=rule_id,
        limit=limit,
    )


@alerts_router.get("/alert-events/{event_id}", response_model=AlertEventResponse)
def read_alert_event(event_id: int):
    return alert_service.get_event(event_id)
