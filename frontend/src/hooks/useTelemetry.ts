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

export function useTelemetry(deviceId: string | null, refreshIntervalMs = 2000): UseTelemetryResult {
    const [telemetry, setTelemetry] = useState<Telemetry[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        let isMounted = true;

        async function fetchTelemetry() {
            if (!deviceId) return;
            try {
                const data = await getTelemetry(deviceId);

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

        if (!deviceId) return;
        fetchTelemetry();

        const intervalId = window.setInterval(
            fetchTelemetry,
            refreshIntervalMs
        );

        return () => {
            isMounted = false;
            window.clearInterval(intervalId);
        };
    }, [deviceId, refreshIntervalMs]);

    const selectedTelemetry = deviceId
        ? telemetry.filter((sample) => sample.device_id === deviceId)
        : [];
    return {
        telemetry: selectedTelemetry,
        latestTelemetry: selectedTelemetry[0] ?? null,
        loading,
        error,
    };
}
