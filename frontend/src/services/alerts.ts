import { apiFetch } from "@/lib/api";
import { AlertEvent, AlertRule, AlertRuleInput } from "@/types/alerts";


export const getAlertRules = (deviceId: string) =>
    apiFetch<AlertRule[]>(
        `/alert-rules/?device_id=${encodeURIComponent(deviceId)}`
    );

export const getActiveAlerts = (deviceId: string) =>
    apiFetch<AlertEvent[]>(
        `/alerts/active?device_id=${encodeURIComponent(deviceId)}`
    );

export const getAlertEvents = (deviceId: string, limit = 20) =>
    apiFetch<AlertEvent[]>(
        `/alert-events?device_id=${encodeURIComponent(deviceId)}&limit=${limit}`
    );

export const createAlertRule = (rule: AlertRuleInput) =>
    apiFetch<AlertRule>("/alert-rules/", {
        method: "POST",
        body: JSON.stringify(rule),
    });

export const updateAlertRule = (
    ruleId: number,
    changes: Partial<Omit<AlertRuleInput, "device_id">>
) =>
    apiFetch<AlertRule>(`/alert-rules/${ruleId}`, {
        method: "PATCH",
        body: JSON.stringify(changes),
    });

export const archiveAlertRule = (ruleId: number) =>
    apiFetch<void>(`/alert-rules/${ruleId}`, {
        method: "DELETE",
    });
