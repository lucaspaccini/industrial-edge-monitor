export interface MetricStatistics {
    min: number | null;
    max: number | null;
    avg: number | null;
}

export interface TelemetryStatistics {
    samples: number;
    temperature: MetricStatistics;
    humidity: MetricStatistics;
    last_update: string | null;
}