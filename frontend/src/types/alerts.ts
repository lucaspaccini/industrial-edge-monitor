export type AlertMetric = "temperature" | "humidity";
export type AlertOperator = "greater_than" | "less_than";
export type AlertSeverity = "info" | "warning" | "critical";
export type AlertRuntimeState = "normal" | "pending" | "active";
export type AlertEventStatus = "active" | "resolved";

export interface AlertRuleInput {
    name: string;
    device_id: string;
    metric: AlertMetric;
    operator: AlertOperator;
    threshold: number;
    duration_seconds: number;
    hysteresis: number;
    severity: AlertSeverity;
    enabled: boolean;
}

export interface AlertRule extends AlertRuleInput {
    id: number;
    created_at: string;
    updated_at: string;
    archived_at: string | null;
    runtime_state: AlertRuntimeState;
    pending_since: string | null;
    last_evaluated_at: string | null;
    last_value: number | null;
}

export interface AlertEvent {
    id: number;
    rule_id: number;
    device_id: string;
    rule_name: string;
    metric: AlertMetric;
    operator: AlertOperator;
    threshold: number;
    hysteresis: number;
    duration_seconds: number;
    severity: AlertSeverity;
    status: AlertEventStatus;
    started_at: string;
    activated_at: string;
    resolved_at: string | null;
    resolution_reason: string | null;
    activation_value: number;
    last_value: number;
    extreme_value: number;
}
