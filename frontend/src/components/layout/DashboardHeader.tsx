import { Badge } from "@/components/ui/badge";

export default function DashboardHeader() {
    return (
        <header className="mb-8 flex items-center justify-between">
            <div>
                <h1 className="text-4xl font-bold tracking-tight">
                    Industrial Edge Monitor
                </h1>

                <p className="text-muted-foreground">
                    Real-time industrial telemetry dashboard
                </p>
            </div>

            <Badge variant="default">
                Connected
            </Badge>
        </header>
    );
}