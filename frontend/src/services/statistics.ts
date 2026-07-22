import { apiFetch } from "@/lib/api";
import { TelemetryStatistics } from "@/types/statistics";

export async function getTelemetryStatistics(deviceId: string): Promise<TelemetryStatistics> {
    return apiFetch<TelemetryStatistics>(`/telemetry/statistics?device_id=${encodeURIComponent(deviceId)}`);
}
