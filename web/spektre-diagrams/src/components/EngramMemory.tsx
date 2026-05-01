import { motion } from "framer-motion";
import { Archive, Flame, ThermometerSnowflake, Zap } from "lucide-react";
import { Badge } from "./ui/Badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./ui/Card";

const tiers = [
  { name: "Hot", speed: "ns", cap: "small", icon: Flame },
  { name: "Warm", speed: "µs", cap: "medium", icon: Zap },
  { name: "Cold", speed: "ms", cap: "large", icon: ThermometerSnowflake },
  { name: "Archive", speed: "s", cap: "huge", icon: Archive },
] as const;

export function EngramMemory() {
  return (
    <Card data-testid="engram-memory">
      <CardHeader>
        <CardTitle>Memory hierarchy</CardTitle>
        <CardDescription>Four tiers with latency/Capacity badges. Dual-process cards are schematic; no throughput claims.</CardDescription>
      </CardHeader>
      <CardContent className="grid gap-6 lg:grid-cols-2">
        <motion.div className="space-y-2" initial={{ opacity: 0, y: 6 }} animate={{ opacity: 1, y: 0 }}>
          {tiers.map((t, i) => {
            const Icon = t.icon;
            return (
              <motion.div
                key={t.name}
                initial={{ opacity: 0, x: -6 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: i * 0.07 }}
                className="flex items-center justify-between gap-3 rounded-xl border border-[var(--color-spektre-border)]/90 bg-[var(--color-spektre-bg-raised)]/85 px-3 py-2.5"
              >
                <span className="flex items-center gap-2.5 text-sm font-medium text-[var(--color-spektre-text)]">
                  <span className="flex h-8 w-8 items-center justify-center rounded-lg border border-[var(--color-spektre-border)] bg-[var(--color-spektre-surface)]">
                    <Icon className="h-4 w-4 text-[var(--color-spektre-blue)]" aria-hidden />
                  </span>
                  {t.name}
                </span>
                <span className="flex shrink-0 gap-1.5">
                  <Badge variant="muted">{t.speed}</Badge>
                  <Badge variant="blue">{t.cap}</Badge>
                </span>
              </motion.div>
            );
          })}
        </motion.div>
        <div className="grid gap-3">
          <Card className="border-[var(--color-spektre-blue)]/45 bg-[var(--color-spektre-blue)]/6">
            <CardContent className="py-4">
              <p className="text-xs font-semibold text-[var(--color-spektre-blue)]">Fast learner</p>
              <p className="mt-1 text-xs leading-relaxed text-[var(--color-spektre-muted)]">Low-latency plasticity in hot tiers — lab storyboard only.</p>
            </CardContent>
          </Card>
          <Card>
            <CardContent className="py-4">
              <p className="text-xs font-semibold text-[var(--color-spektre-text)]">Slow learner</p>
              <p className="mt-1 text-xs leading-relaxed text-[var(--color-spektre-muted)]">Consolidation into cold/archive paths with stronger receipts.</p>
            </CardContent>
          </Card>
        </div>
      </CardContent>
    </Card>
  );
}
