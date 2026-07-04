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
    refreshIntervalMs = 5000
): UseTelemetryStatisticsResult {
    const [statistics, setStatistics] =
        useState<TelemetryStatistics | null>(null);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        let isMounted = true;

        async function fetchStatistics() {
            try {
                const data = await getTelemetryStatistics();

                if (!isMounted) {
                    return;
                }

                setStatistics(data);
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

        fetchStatistics();

        const intervalId = window.setInterval(
            fetchStatistics,
            refreshIntervalMs
        );

        return () => {
            isMounted = false;
            window.clearInterval(intervalId);
        };
    }, [refreshIntervalMs]);

    return {
        statistics,
        loading,
        error,
    };
}