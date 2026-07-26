"use client";

import { useEffect, useState } from "react";

import { getDevices } from "@/services/devices";
import { DeviceSummary } from "@/types/device";


export function useDeviceList(refreshIntervalMs = 5000) {
    const [devices, setDevices] = useState<DeviceSummary[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        let mounted = true;

        async function refresh() {
            try {
                const deviceList = await getDevices();

                if (mounted) {
                    setDevices(deviceList);
                    setError(null);
                }
            } catch (value) {
                if (mounted) {
                    setError(
                        value instanceof Error
                            ? value.message
                            : "Failed to fetch device list"
                    );
                }
            } finally {
                if (mounted) {
                    setLoading(false);
                }
            }
        }

        refresh();
        const intervalId = window.setInterval(refresh, refreshIntervalMs);

        return () => {
            mounted = false;
            window.clearInterval(intervalId);
        };
    }, [refreshIntervalMs]);

    return { devices, loading, error };
}
