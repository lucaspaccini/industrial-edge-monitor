import { BarChart3 } from "lucide-react";

import {
    Card,
    CardContent,
    CardHeader,
    CardTitle,
} from "@/components/ui/card";
import { TelemetryStatistics } from "@/types/statistics";

interface StatisticsCardProps {
    statistics: TelemetryStatistics;
}

function formatValue(value: number | null, unit: string): string {
    if (value === null) {
        return "N/A";
    }

    return `${value.toFixed(1)} ${unit}`;
}

export default function StatisticsCard({
    statistics,
}: StatisticsCardProps) {
    return (
        <Card className="border-slate-800 bg-slate-900 text-slate-50">
            <CardHeader className="flex flex-row items-center justify-between">
                <CardTitle className="text-sm font-medium text-slate-400">
                    Telemetry Statistics
                </CardTitle>

                <BarChart3 className="h-5 w-5 text-slate-400" />
            </CardHeader>

            <CardContent className="space-y-5">
                <div>
                    <p className="text-sm text-slate-400">Samples</p>
                    <p className="text-3xl font-semibold">
                        {statistics.samples}
                    </p>
                </div>

                <div className="grid gap-4 md:grid-cols-2">
                    <div>
                        <p className="mb-2 text-sm font-medium text-slate-400">
                            Temperature
                        </p>

                        <div className="space-y-1 text-sm">
                            <p>Min: {formatValue(statistics.temperature.min, "°C")}</p>
                            <p>Avg: {formatValue(statistics.temperature.avg, "°C")}</p>
                            <p>Max: {formatValue(statistics.temperature.max, "°C")}</p>
                        </div>
                    </div>

                    <div>
                        <p className="mb-2 text-sm font-medium text-slate-400">
                            Humidity
                        </p>

                        <div className="space-y-1 text-sm">
                            <p>Min: {formatValue(statistics.humidity.min, "%")}</p>
                            <p>Avg: {formatValue(statistics.humidity.avg, "%")}</p>
                            <p>Max: {formatValue(statistics.humidity.max, "%")}</p>
                        </div>
                    </div>
                </div>
            </CardContent>
        </Card>
    );
}