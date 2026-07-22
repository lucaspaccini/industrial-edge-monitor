import { apiFetch } from "@/lib/api";
import { Telemetry } from "@/types/telemetry";

export async function getTelemetry(deviceId: string): Promise<Telemetry[]> {
    return apiFetch<Telemetry[]>(`/telemetry/?device_id=${encodeURIComponent(deviceId)}`);
}
