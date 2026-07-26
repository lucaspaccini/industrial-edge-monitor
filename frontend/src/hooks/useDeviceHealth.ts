"use client";

import { useEffect, useState } from "react";

import { getDeviceHealth } from "@/services/devices";
import { DeviceHealth } from "@/types/device";


interface HealthResult {
    deviceId: string;
    health: DeviceHealth;
}

interface HealthError {
    deviceId: string;
    message: string;
}

export function useDeviceHealth(
    deviceId: string | null,
    refreshIntervalMs = 5000
) {
    const [result, setResult] = useState<HealthResult | null>(null);
    const [failure, setFailure] = useState<HealthError | null>(null);

    useEffect(() => {
        if (!deviceId) {
            return;
        }

        const selectedDeviceId = deviceId;
        let mounted = true;

        async function refresh() {
            try {
                const health = await getDeviceHealth(selectedDeviceId);

                if (mounted) {
                    setResult({ deviceId: selectedDeviceId, health });
                    setFailure(null);
                }
            } catch (value) {
                if (mounted) {
                    setFailure({
                        deviceId: selectedDeviceId,
                        message: value instanceof Error
                            ? value.message
                            : "Failed to fetch device health",
                    });
                }
            }
        }

        refresh();
        const intervalId = window.setInterval(refresh, refreshIntervalMs);

        return () => {
            mounted = false;
            window.clearInterval(intervalId);
        };
    }, [deviceId, refreshIntervalMs]);

    return {
        health: result?.deviceId === deviceId ? result.health : null,
        error: failure?.deviceId === deviceId ? failure.message : null,
    };
}
