import { DeviceHealth } from "@/types/device";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";

function getHealthStatusClassName(status: string): string {
    switch (status.toLowerCase()) {
        case "healthy":
        case "online":
            return "border-emerald-500/30 bg-emerald-500/15 text-emerald-400";
        case "degraded":
        case "unknown":
            return "border-amber-500/30 bg-amber-500/15 text-amber-400";
        case "fault":
        case "offline":
            return "border-red-500/30 bg-red-500/15 text-red-400";
        default:
            return "border-slate-500/30 bg-slate-500/15 text-slate-300";
    }
}

export default function DeviceHealthCard({ health }: { health: DeviceHealth | null }) {
    const label = (key: string) => key.replaceAll("_", " ").replace(/^./, (value) => value.toUpperCase());
    return (
        <Card className="border-slate-800 bg-slate-900 text-slate-50">
            <CardHeader><CardTitle className="text-slate-300">Device health</CardTitle></CardHeader>
            <CardContent className="space-y-2 text-sm">
                {!health ? <p>No health snapshot available.</p> : <>
                    <p>Device: {health.device_id}</p>
                    <p>Overall: <Badge variant="outline" className={getHealthStatusClassName(health.status)}>{health.status.toUpperCase()}</Badge></p>
                    <p>Availability: <Badge variant="outline" className={getHealthStatusClassName(health.availability)}>{health.availability.toUpperCase()}</Badge></p>
                    <p>Last seen: {health.last_seen ? new Date(health.last_seen).toLocaleString() : "never"}</p>
                    <div>{Object.entries(health.components).map(([name, component]) =>
                        <p key={name}>{label(name)}: <Badge variant="outline" className={getHealthStatusClassName(component.status)}>{component.status.toUpperCase()}</Badge>{component.error_code && component.error_code !== "none" ? ` (${label(component.error_code)})` : ""}</p>
                    )}</div>
                    <div className="grid gap-2 sm:grid-cols-2">
                        {Object.entries(health.counters).map(([name, value]) =>
                            <p key={name}>{label(name)}: {value}</p>)}
                        {Object.entries(health.metrics).map(([name, value]) =>
                            <p key={name}>{label(name)}: {value ?? "unavailable"}</p>)}
                    </div>
                </>}
            </CardContent>
        </Card>
    );
}
