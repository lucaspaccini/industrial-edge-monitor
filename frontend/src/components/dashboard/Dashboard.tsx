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
import DeviceHealthCard from "./DeviceHealthCard";
import { useDevices } from "@/hooks/useDevices";
import { useState } from "react";


export default function Dashboard() {
    const [selectedDeviceId, setSelectedDeviceId] = useState<string | null>(null);
    const { devices } = useDevices(null);
    const effectiveDeviceId = devices.some((device) => device.device_id === selectedDeviceId)
        ? selectedDeviceId : devices[0]?.device_id ?? null;
    const { health, error: devicesError } = useDevices(effectiveDeviceId);
    const { telemetry, latestTelemetry, loading, error } = useTelemetry(effectiveDeviceId);
    const {
        statistics,
        error: statisticsError,
    } = useTelemetryStatistics(effectiveDeviceId);

    if (!effectiveDeviceId && devices.length === 0) {
        return <p>No devices available.</p>;
    }

    if (loading) {
        return <p>Loading telemetry...</p>;
    }

    return (
    <main className="min-h-screen bg-slate-950 p-8 text-slate-50">
        <DashboardHeader />

        <div className="mb-6 flex items-center gap-3">
            <label htmlFor="device-select">Device</label>
            <select id="device-select" value={effectiveDeviceId ?? ""}
                onChange={(event) => setSelectedDeviceId(event.target.value)}
                className="rounded border border-slate-700 bg-slate-900 px-3 py-2">
                {devices.map((device) => <option key={device.device_id} value={device.device_id}>
                    {device.device_id} · {device.availability}
                </option>)}
            </select>
        </div>

        {(devicesError || statisticsError || error) &&
            <p className="mb-4 text-amber-400">{devicesError ?? statisticsError ?? error}</p>}

        {latestTelemetry ? <><div className="grid gap-6 md:grid-cols-2 xl:grid-cols-4">
            <TemperatureCard temperature={latestTelemetry.temperature} />
            <HumidityCard humidity={latestTelemetry.humidity} />
            <StatusCard status={latestTelemetry.machine_status} />
            <TimeStampCard timestamp={latestTelemetry.timestamp} />
        </div>

        <div className="mt-6">
            <TelemetryChart telemetry={telemetry} />
        </div>

        </> : <p>No telemetry available for this device.</p>}

        {statistics && (
            <div className="mt-6">
                <StatisticsCard statistics={statistics} />
            </div>
        )}

        <div className="mt-6"><DeviceHealthCard health={health} /></div>
    </main>
);
}
