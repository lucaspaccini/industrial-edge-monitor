"use client";

import { useEffect, useState } from "react";
import { getDeviceHealth, getDevices } from "@/services/devices";
import { DeviceHealth, DeviceSummary } from "@/types/device";

export function useDevices(selectedDeviceId: string | null, refreshIntervalMs = 5000) {
    const [devices, setDevices] = useState<DeviceSummary[]>([]);
    const [health, setHealth] = useState<DeviceHealth | null>(null);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        let mounted = true;
        async function refresh() {
            try {
                const [deviceList, deviceHealth] = await Promise.all([
                    getDevices(),
                    selectedDeviceId ? getDeviceHealth(selectedDeviceId).catch(() => null) : null,
                ]);
                if (mounted) {
                    setDevices(deviceList);
                    setHealth(deviceHealth);
                    setError(null);
                }
            } catch (value) {
                if (mounted) setError(value instanceof Error ? value.message : "Failed to fetch devices");
            }
        }
        refresh();
        const id = window.setInterval(refresh, refreshIntervalMs);
        return () => { mounted = false; window.clearInterval(id); };
    }, [selectedDeviceId, refreshIntervalMs]);

    return { devices, health, error };
}
