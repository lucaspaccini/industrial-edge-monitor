"use client";

import { useCallback, useEffect, useState } from "react";

import {
    getActiveAlerts,
    getAlertEvents,
    getAlertRules,
} from "@/services/alerts";
import { AlertEvent, AlertRule } from "@/types/alerts";
import { withoutArchivedRule } from "@/lib/alerts-state.mjs";


interface AlertSnapshot {
    deviceId: string;
    rules: AlertRule[];
    active: AlertEvent[];
    events: AlertEvent[];
}

export function useAlerts(deviceId: string, refreshIntervalMs = 5000) {
    const [snapshot, setSnapshot] = useState<AlertSnapshot | null>(null);
    const [failure, setFailure] = useState<{
        deviceId: string;
        message: string;
    } | null>(null);
    const [refreshRevision, setRefreshRevision] = useState(0);

    const refresh = useCallback(() => {
        setRefreshRevision((revision) => revision + 1);
    }, []);

    const removeRule = useCallback((ruleId: number) => {
        setSnapshot((current) => current === null ? null : {
            ...current,
            rules: withoutArchivedRule(current.rules, ruleId),
        });
    }, []);

    useEffect(() => {
        const selectedDeviceId = deviceId;
        let mounted = true;

        async function fetchAlerts() {
            try {
                const [rules, active, events] = await Promise.all([
                    getAlertRules(selectedDeviceId),
                    getActiveAlerts(selectedDeviceId),
                    getAlertEvents(selectedDeviceId),
                ]);
                if (mounted) {
                    setSnapshot({
                        deviceId: selectedDeviceId,
                        rules,
                        active,
                        events,
                    });
                    setFailure(null);
                }
            } catch (value) {
                if (mounted) {
                    setFailure({
                        deviceId: selectedDeviceId,
                        message: value instanceof Error
                            ? value.message
                            : "Failed to fetch alerts",
                    });
                }
            }
        }

        fetchAlerts();
        const intervalId = window.setInterval(fetchAlerts, refreshIntervalMs);
        return () => {
            mounted = false;
            window.clearInterval(intervalId);
        };
    }, [deviceId, refreshIntervalMs, refreshRevision]);

    const selected = snapshot?.deviceId === deviceId ? snapshot : null;
    return {
        rules: selected?.rules ?? [],
        activeAlerts: selected?.active ?? [],
        events: selected?.events ?? [],
        loading: selected === null && failure?.deviceId !== deviceId,
        error: failure?.deviceId === deviceId ? failure.message : null,
        refresh,
        removeRule,
    };
}
