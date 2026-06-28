"use client";

import { useEffect, useState } from "react";

import { getTelemetry } from "@/services/telemetry";
import { Telemetry } from "@/types/telemetry";

interface UseTelemetryResult {
    telemetry: Telemetry[];
    latestTelemetry: Telemetry | null;
    loading: boolean;
    error: string | null;
}

export function useTelemetry(refreshIntervalMs = 2000): UseTelemetryResult {
    const [telemetry, setTelemetry] = useState<Telemetry[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        let isMounted = true;

        async function fetchTelemetry() {
            try {
                const data = await getTelemetry();

                if (!isMounted) {
                    return;
                }

                setTelemetry(data);
                setError(null);
            } catch (err) {
                if (!isMounted) {
                    return;
                }

                setError(
                    err instanceof Error
                        ? err.message
                        : "Failed to fetch telemetry data"
                );
            } finally {
                if (isMounted) {
                    setLoading(false);
                }
            }
        }

        fetchTelemetry();

        const intervalId = window.setInterval(
            fetchTelemetry,
            refreshIntervalMs
        );

        return () => {
            isMounted = false;
            window.clearInterval(intervalId);
        };
    }, [refreshIntervalMs]);

    return {
        telemetry,
        latestTelemetry: telemetry[0] ?? null,
        loading,
        error,
    };
}