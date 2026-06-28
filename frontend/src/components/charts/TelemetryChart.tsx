"use client";

import {
    CartesianGrid,
    Line,
    LineChart,
    XAxis,
    YAxis,
} from "recharts";

import {
    Card,
    CardContent,
    CardDescription,
    CardHeader,
    CardTitle,
} from "@/components/ui/card";
import {
    ChartConfig,
    ChartContainer,
    ChartTooltip,
    ChartTooltipContent,
} from "@/components/ui/chart";
import { Telemetry } from "@/types/telemetry";

interface TelemetryChartProps {
    telemetry: Telemetry[];
}

const chartConfig = {
    temperature: {
        label: "Temperature",
    },
} satisfies ChartConfig;

function formatTime(timestamp: string): string {
    return new Date(timestamp).toLocaleTimeString("en-GB", {
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit",
        hour12: false,
    });
}

export default function TelemetryChart({
    telemetry,
}: TelemetryChartProps) {
    const chartData = telemetry
        .slice()
        .reverse()
        .map((item) => ({
            time: formatTime(item.timestamp),
            temperature: item.temperature,
        }));

    return (
        <Card className="border-slate-800 bg-slate-900 text-slate-50">
            <CardHeader>
                <CardTitle>Temperature History</CardTitle>
                <CardDescription className="text-slate-400">
                    Latest telemetry samples
                </CardDescription>
            </CardHeader>

            <CardContent>
                <ChartContainer
                    config={chartConfig}
                    className="h-[300px] w-full"
                >
                    <LineChart
                        accessibilityLayer
                        data={chartData}
                        margin={{
                            left: 12,
                            right: 12,
                        }}
                    >
                        <CartesianGrid vertical={false} />

                        <XAxis
                            dataKey="time"
                            tickLine={false}
                            axisLine={false}
                            tickMargin={8}
                            minTickGap={32}
                        />

                        <YAxis
                            tickLine={false}
                            axisLine={false}
                            tickMargin={8}
                            width={40}
                        />

                        <ChartTooltip
                            cursor={false}
                            content={<ChartTooltipContent />}
                        />

                        <Line
                            dataKey="temperature"
                            type="monotone"
                            stroke="#38bdf8"
                            strokeWidth={3}
                            dot={false}
                            activeDot={{ r: 5 }}
                        />
                    </LineChart>
                </ChartContainer>
            </CardContent>
        </Card>
    );
}