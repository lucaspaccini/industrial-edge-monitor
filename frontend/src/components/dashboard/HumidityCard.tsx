import { Droplets } from "lucide-react";

import {
    Card,
    CardContent,
    CardHeader,
    CardTitle,
} from "@/components/ui/card";

interface HumidityCardProps {
    humidity: number;
}

export default function HumidityCard({ humidity }: HumidityCardProps) {
    return (
        <Card className="border-slate-800 bg-slate-900 text-slate-50">
            <CardHeader className="flex flex-row items-center justify-between">
                <CardTitle className="text-sm font-medium text-slate-400">
                    Humidity
                </CardTitle>

                <Droplets className="h-5 w-5 text-slate-400" />
            </CardHeader>

            <CardContent>
                <p className="text-5xl font-semibold tracking-tight">
                    {humidity.toFixed(1)} %
                </p>
            </CardContent>
        </Card>
    );
}