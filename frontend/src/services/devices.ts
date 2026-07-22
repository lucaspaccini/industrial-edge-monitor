import { apiFetch } from "@/lib/api";
import { DeviceHealth, DeviceSummary } from "@/types/device";

export const getDevices = () => apiFetch<DeviceSummary[]>("/devices/");

export const getDeviceHealth = (deviceId: string) =>
    apiFetch<DeviceHealth>(`/devices/${encodeURIComponent(deviceId)}/health`);
