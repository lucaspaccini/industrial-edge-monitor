"use client";

import DashboardHeader from "@/components/layout/DashboardHeader";

import { useTelemetry } from "@/hooks/useTelemetry";
import { useTelemetryStatistics } from "@/hooks/useTelemetryStatistics";

import HumidityCard from "./HumidityCard";
import TimeStampCard from "./TimestampCard";
import StatusCard from "./StatusCard";
import TemperatureCard from "./TemperatureCard";
import TelemetryChart from "../charts/TelemetryChart";
import StatisticsCard from "./StatisticsCard";


export default function Dashboard() {
    const { telemetry, latestTelemetry, loading, error } = useTelemetry();
    const {
        statistics,
        loading: statisticsLoading,
        error: statisticsError,
    } = useTelemetryStatistics();

    if (loading) {
        return <p>Loading telemetry...</p>;
    }

    if (error) {
        return <p>Error: {error}</p>;
    }

    if (!latestTelemetry) {
        return <p>No telemetry available.</p>;
    }

    return (
    <main className="min-h-screen bg-slate-950 p-8 text-slate-50">
        <DashboardHeader />

        <div className="grid gap-6 md:grid-cols-2 xl:grid-cols-4">
            <TemperatureCard temperature={latestTelemetry.temperature} />
            <HumidityCard humidity={latestTelemetry.humidity} />
            <StatusCard status={latestTelemetry.machine_status} />
            <TimeStampCard timestamp={latestTelemetry.timestamp} />
        </div>

        <div className="mt-6">
            <TelemetryChart telemetry={telemetry} />
        </div>

        {statistics && (
            <div className="mt-6">
                <StatisticsCard statistics={statistics} />
            </div>
        )}
    </main>
);
}