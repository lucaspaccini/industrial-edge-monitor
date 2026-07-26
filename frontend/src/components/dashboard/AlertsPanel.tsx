"use client";

import { FormEvent, useState } from "react";

import { Badge } from "@/components/ui/badge";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { useAlerts } from "@/hooks/useAlerts";
import {
    archiveAlertRule,
    createAlertRule,
    updateAlertRule,
} from "@/services/alerts";
import {
    AlertMetric,
    AlertOperator,
    AlertRule,
    AlertRuleInput,
    AlertSeverity,
} from "@/types/alerts";


const severityClasses: Record<AlertSeverity, string> = {
    info: "border-sky-500/30 bg-sky-500/15 text-sky-400",
    warning: "border-amber-500/30 bg-amber-500/15 text-amber-400",
    critical: "border-red-500/30 bg-red-500/15 text-red-400",
};

const runtimeStateClasses = {
    normal: "border-emerald-500/30 bg-emerald-500/15 text-emerald-400",
    pending: "border-amber-500/30 bg-amber-500/15 text-amber-400",
    active: "border-red-500/30 bg-red-500/15 text-red-400",
};

const emptyForm = {
    name: "",
    metric: "temperature" as AlertMetric,
    operator: "greater_than" as AlertOperator,
    threshold: "30",
    duration_seconds: "0",
    hysteresis: "1",
    severity: "warning" as AlertSeverity,
};

const label = (value: string) =>
    value.replaceAll("_", " ").replace(/^./, (character) => character.toUpperCase());

export default function AlertsPanel({
    deviceId,
    offline,
}: {
    deviceId: string;
    offline: boolean;
}) {
    const {
        rules,
        activeAlerts,
        events,
        loading,
        error,
        refresh,
        removeRule,
    } = useAlerts(deviceId);
    const [form, setForm] = useState(emptyForm);
    const [editingRuleId, setEditingRuleId] = useState<number | null>(null);
    const [mutationError, setMutationError] = useState<string | null>(null);

    const toRuleInput = (): AlertRuleInput => ({
        name: form.name,
        device_id: deviceId,
        metric: form.metric,
        operator: form.operator,
        threshold: Number(form.threshold),
        duration_seconds: Number(form.duration_seconds),
        hysteresis: Number(form.hysteresis),
        severity: form.severity,
        enabled: true,
    });

    async function submit(event: FormEvent) {
        event.preventDefault();
        try {
            const input = toRuleInput();
            if (editingRuleId === null) {
                await createAlertRule(input);
            } else {
                const {
                    device_id: _deviceId,
                    enabled: _enabled,
                    ...changes
                } = input;
                void _deviceId;
                void _enabled;
                await updateAlertRule(editingRuleId, changes);
            }
            setForm(emptyForm);
            setEditingRuleId(null);
            setMutationError(null);
            refresh();
        } catch (value) {
            setMutationError(
                value instanceof Error ? value.message : "Failed to save alert rule"
            );
        }
    }

    function edit(rule: AlertRule) {
        setEditingRuleId(rule.id);
        setForm({
            name: rule.name,
            metric: rule.metric,
            operator: rule.operator,
            threshold: String(rule.threshold),
            duration_seconds: String(rule.duration_seconds),
            hysteresis: String(rule.hysteresis),
            severity: rule.severity,
        });
    }

    async function toggle(rule: AlertRule) {
        try {
            await updateAlertRule(rule.id, { enabled: !rule.enabled });
            setMutationError(null);
            refresh();
        } catch (value) {
            setMutationError(
                value instanceof Error ? value.message : "Failed to update rule"
            );
        }
    }

    async function archive(rule: AlertRule) {
        if (!window.confirm(
            `Archive rule "${rule.name}"? Historical events will be preserved.`
        )) {
            return;
        }
        try {
            await archiveAlertRule(rule.id);
            removeRule(rule.id);
            if (editingRuleId === rule.id) {
                setEditingRuleId(null);
                setForm(emptyForm);
            }
            setMutationError(null);
            refresh();
        } catch (value) {
            setMutationError(
                value instanceof Error ? value.message : "Failed to archive rule"
            );
        }
    }

    return (
        <Card className="border-slate-800 bg-slate-900 text-slate-50">
            <CardHeader className="flex flex-row items-center justify-between">
                <CardTitle className="text-slate-300">Alerts and rules</CardTitle>
                <Badge className={activeAlerts.length > 0
                    ? severityClasses.critical
                    : "border-emerald-500/30 bg-emerald-500/15 text-emerald-400"}>
                    {activeAlerts.length} active
                </Badge>
            </CardHeader>
            <CardContent className="space-y-6">
                {offline &&
                    <p className="text-amber-400">Device offline; rules remain persisted but no new samples can be evaluated.</p>}
                {loading && <p>Loading alerts...</p>}
                {error && <p className="text-red-400">Alerts: {error}</p>}
                {mutationError && <p className="text-red-400">{mutationError}</p>}

                <section>
                    <h3 className="mb-2 font-medium">Active alerts</h3>
                    {activeAlerts.length === 0 ? <p className="text-slate-400">No active alerts.</p> :
                        <div className="space-y-2">{activeAlerts.map((alert) =>
                            <div key={alert.id} className="rounded border border-slate-700 p-3">
                                <Badge className={severityClasses[alert.severity]}>{alert.severity.toUpperCase()}</Badge>
                                <span className="ml-2 font-medium">{alert.rule_name}</span>
                                <p className="mt-1 text-slate-400">
                                    {label(alert.metric)} {label(alert.operator)} {alert.threshold}; current {alert.last_value}, extreme {alert.extreme_value}
                                </p>
                            </div>)}</div>}
                </section>

                <section>
                    <h3 className="mb-2 font-medium">Rules</h3>
                    {rules.length === 0 ? <p className="text-slate-400">No rules configured.</p> :
                        <div className="space-y-2">{rules.map((rule) =>
                            <div key={rule.id} className="flex flex-wrap items-center gap-2 rounded border border-slate-700 p-3">
                                <span className="font-medium">{rule.name}</span>
                                <Badge className={severityClasses[rule.severity]}>{rule.severity}</Badge>
                                <Badge
                                    variant="outline"
                                    className={runtimeStateClasses[rule.runtime_state]}
                                >
                                    {rule.runtime_state.toUpperCase()}
                                </Badge>
                                {!rule.enabled &&
                                    <Badge
                                        variant="outline"
                                        className="border-slate-500/30 bg-slate-500/15 text-slate-300"
                                    >
                                        DISABLED
                                    </Badge>}
                                <span className="text-slate-400">
                                    {label(rule.metric)} {label(rule.operator)} {rule.threshold}, {rule.duration_seconds}s, hysteresis {rule.hysteresis}
                                </span>
                                <button className="ml-auto text-sky-400" onClick={() => edit(rule)}>Edit</button>
                                <button className="text-amber-400" onClick={() => toggle(rule)}>
                                    {rule.enabled ? "Disable" : "Enable"}
                                </button>
                                <button className="text-red-400" onClick={() => archive(rule)}>
                                    Delete
                                </button>
                            </div>)}</div>}
                </section>

                <form onSubmit={submit} className="grid gap-3 rounded border border-slate-700 p-3 md:grid-cols-4">
                    <h3 className="md:col-span-4 font-medium">
                        {editingRuleId === null ? "Create rule" : "Edit rule"}
                    </h3>
                    <input required maxLength={100} placeholder="Rule name" value={form.name}
                        onChange={(event) => setForm({...form, name: event.target.value})}
                        className="rounded bg-slate-950 p-2" />
                    <select value={form.metric} onChange={(event) => setForm({...form, metric: event.target.value as AlertMetric})}
                        className="rounded bg-slate-950 p-2">
                        <option value="temperature">Temperature</option>
                        <option value="humidity">Humidity</option>
                    </select>
                    <select value={form.operator} onChange={(event) => setForm({...form, operator: event.target.value as AlertOperator})}
                        className="rounded bg-slate-950 p-2">
                        <option value="greater_than">Greater than</option>
                        <option value="less_than">Less than</option>
                    </select>
                    <input required type="number" step="any" aria-label="Threshold" value={form.threshold}
                        onChange={(event) => setForm({...form, threshold: event.target.value})}
                        className="rounded bg-slate-950 p-2" />
                    <input required type="number" min="0" step="1" aria-label="Duration seconds" value={form.duration_seconds}
                        onChange={(event) => setForm({...form, duration_seconds: event.target.value})}
                        className="rounded bg-slate-950 p-2" />
                    <input required type="number" min="0" step="any" aria-label="Hysteresis" value={form.hysteresis}
                        onChange={(event) => setForm({...form, hysteresis: event.target.value})}
                        className="rounded bg-slate-950 p-2" />
                    <select value={form.severity} onChange={(event) => setForm({...form, severity: event.target.value as AlertSeverity})}
                        className="rounded bg-slate-950 p-2">
                        <option value="info">Info</option>
                        <option value="warning">Warning</option>
                        <option value="critical">Critical</option>
                    </select>
                    <div className="flex gap-2">
                        <button type="submit" className="rounded bg-sky-700 px-3 py-2">
                            {editingRuleId === null ? "Create" : "Save"}
                        </button>
                        {editingRuleId !== null &&
                            <button type="button" onClick={() => { setEditingRuleId(null); setForm(emptyForm); }}
                                className="rounded bg-slate-700 px-3 py-2">Cancel</button>}
                    </div>
                </form>

                <section>
                    <h3 className="mb-2 font-medium">Recent event history</h3>
                    {events.length === 0 ? <p className="text-slate-400">No alert events.</p> :
                        <div className="space-y-1">{events.map((event) =>
                            <p key={event.id}>
                                <Badge className={severityClasses[event.severity]}>{event.severity}</Badge>
                                <span className="ml-2">{event.rule_name}: {event.status}</span>
                                <span className="ml-2 text-slate-400">{new Date(event.activated_at).toLocaleString()}</span>
                                {event.resolution_reason && <span className="ml-2 text-slate-400">({label(event.resolution_reason)})</span>}
                            </p>)}</div>}
                </section>
            </CardContent>
        </Card>
    );
}
