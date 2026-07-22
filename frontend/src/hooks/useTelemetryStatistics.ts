"use client";

import { useEffect, useState } from "react";

import { getTelemetryStatistics } from "@/services/statistics";
import { TelemetryStatistics } from "@/types/statistics";

interface UseTelemetryStatisticsResult {
    statistics: TelemetryStatistics | null;
    loading: boolean;
    error: string | null;
}

export function useTelemetryStatistics(
    deviceId: string | null,
    refreshIntervalMs = 5000
): UseTelemetryStatisticsResult {
    const [statistics, setStatistics] =
        useState<TelemetryStatistics | null>(null);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [statisticsDeviceId, setStatisticsDeviceId] = useState<string | null>(null);

    useEffect(() => {
        let isMounted = true;

        async function fetchStatistics() {
            if (!deviceId) return;
            try {
                const data = await getTelemetryStatistics(deviceId);

                if (!isMounted) {
                    return;
                }

                setStatistics(data);
                setStatisticsDeviceId(deviceId);
                setError(null);
            } catch (err) {
                if (!isMounted) {
                    return;
                }

                setError(
                    err instanceof Error
                        ? err.message
                        : "Failed to fetch telemetry statistics"
                );
            } finally {
                if (isMounted) {
                    setLoading(false);
                }
            }
        }

        if (!deviceId) return;
        fetchStatistics();

        const intervalId = window.setInterval(
            fetchStatistics,
            refreshIntervalMs
        );

        return () => {
            isMounted = false;
            window.clearInterval(intervalId);
        };
    }, [deviceId, refreshIntervalMs]);

    return {
        statistics: statisticsDeviceId === deviceId ? statistics : null,
        loading,
        error,
    };
}
