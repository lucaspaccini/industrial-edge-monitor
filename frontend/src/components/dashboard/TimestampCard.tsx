import { Clock } from "lucide-react";

import {
    Card,
    CardContent,
    CardHeader,
    CardTitle,
} from "@/components/ui/card";

interface TimestampCardProps {
    timestamp: string;
}

function formatTimestamp(timestamp: string): {
    date: string;
    time: string;
} {
    const date = new Date(timestamp);

    const formattedDate = date.toLocaleDateString("en-GB", {
        day: "2-digit",
        month: "short",
        year: "numeric",
    });

    const formattedTime = date.toLocaleTimeString("en-GB", {
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit",
        hour12: false,
    });

    const milliseconds = date
        .getMilliseconds()
        .toString()
        .padStart(3, "0");

    return {
        date: formattedDate,
        time: `${formattedTime}.${milliseconds}`,
    };
}

export default function TimestampCard({ timestamp }: TimestampCardProps) {
    const formatted = formatTimestamp(timestamp);

    return (
        <Card className="border-slate-800 bg-slate-900 text-slate-50">
            <CardHeader className="flex flex-row items-center justify-between">
                <CardTitle className="text-sm font-medium text-slate-400">
                    Last Update
                </CardTitle>

                <Clock className="h-5 w-5 text-slate-400" />
            </CardHeader>

            <CardContent className="space-y-1">
                <p className="text-2xl font-semibold">
                    {formatted.date}
                </p>

                <p className="text-4xl font-bold tracking-tight">
                    {formatted.time}
                </p>
            </CardContent>
        </Card>
    );
}