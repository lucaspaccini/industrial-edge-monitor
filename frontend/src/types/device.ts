export type Availability = "online" | "offline";
export type DeviceHealthStatus = "healthy" | "degraded";
export type ComponentHealthStatus = "healthy" | "degraded" | "fault" | "unknown";

export interface DeviceSummary {
    device_id: string;
    availability: Availability;
    reported_availability: Availability;
    last_seen: string | null;
}

export interface ComponentHealth {
    status: ComponentHealthStatus;
    error_code: string | null;
    updated_at: string | null;
}

export interface DeviceHealth {
    device_id: string;
    timestamp: string | null;
    received_at: string;
    status: DeviceHealthStatus;
    availability: Availability;
    reported_availability: Availability;
    last_seen: string | null;
    components: Record<string, ComponentHealth>;
    counters: Record<string, number>;
    metrics: Record<string, number | null>;
}
