import { Activity } from "lucide-react";

import { Badge } from "@/components/ui/badge";
import {
    Card,
    CardContent,
    CardHeader,
    CardTitle,
} from "@/components/ui/card";

interface StatusCardProps {
    status: string;
}

function getStatusClassName(status: string): string {
    switch (status.toLowerCase()) {
        case "running":
            return "bg-emerald-500/15 border border-emerald-500/30 text-emerald-400";
        case "alarm":
            return "bg-red-500/15 text-red-400 border-red-500/30";
        case "stopped":
            return "bg-slate-500/15 text-slate-300 border-slate-500/30";
        default:
            return "bg-yellow-500/15 text-yellow-400 border-yellow-500/30";
    }
}

export default function StatusCard({ status }: StatusCardProps) {
    return (
        <Card className="border-slate-800 bg-slate-900 text-slate-50">
            <CardHeader className="flex flex-row items-center justify-between">
                <CardTitle className="text-sm font-medium text-slate-400">
                    Machine Status
                </CardTitle>

                <Activity className="h-5 w-5 text-slate-400" />
            </CardHeader>

            <CardContent>
                <Badge
                    variant="outline"
                    className={getStatusClassName(status)}
                >
                    {status.toUpperCase()}
                </Badge>
            </CardContent>
        </Card>
    );
}