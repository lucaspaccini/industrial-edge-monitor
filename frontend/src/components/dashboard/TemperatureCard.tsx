import { Thermometer } from "lucide-react";

import {
    Card,
    CardContent,
    CardHeader,
    CardTitle,
} from "@/components/ui/card";

interface TemperatureCardProps {
    temperature: number;
}

export default function TemperatureCard({
    temperature,
}: TemperatureCardProps) {
    return (
        <Card className="border-slate-800 bg-slate-900 text-slate-50">
            <CardHeader className="flex flex-row items-center justify-between">
                <CardTitle className="text-sm font-medium text-slate-400">
                    Temperature
                </CardTitle>

                <Thermometer className="h-5 w-5 text-slate-400" />
            </CardHeader>

            <CardContent>
                <p className="text-5xl font-semibold tracking-tight">
                    {temperature.toFixed(1)} °C
                </p>
            </CardContent>
        </Card>
    );
}