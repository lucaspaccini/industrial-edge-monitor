import { apiFetch } from "@/lib/api";
import { TelemetryStatistics } from "@/types/statistics";

export async function getTelemetryStatistics(): Promise<TelemetryStatistics> {
    return apiFetch<TelemetryStatistics>("/telemetry/statistics");
}