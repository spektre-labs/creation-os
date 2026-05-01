import { motion } from "framer-motion";
import { Activity, Layers } from "lucide-react";
import { Badge } from "./ui/Badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./ui/Card";

const layers = [
  { id: "L1", label: "Entropy", note: "cheap prior" },
  { id: "L2", label: "HIDE", note: "state tension" },
  { id: "L3", label: "ICR", note: "cross-layer" },
  { id: "L4", label: "LSD", note: "trained probe" },
  { id: "L5", label: "SAE", note: "dictionary σ" },
  { id: "Ω", label: "Loop", note: "14 phases" },
  { id: "Eg", label: "Engram", note: "tiers" },
  { id: "Ev", label: "Evidence", note: "ladder" },
] as const;

export function HeroOverview() {
  return (
    <Card data-testid="hero-overview" className="overflow-hidden">
      <CardHeader className="relative">
        <div className="absolute -right-8 -top-16 h-40 w-40 rounded-full bg-[var(--color-spektre-blue)]/10 blur-3xl" aria-hidden />
        <CardTitle className="relative flex items-center gap-2 text-[var(--color-spektre-text)]">
          <span className="flex h-9 w-9 items-center justify-center rounded-lg border border-[var(--color-spektre-border)] bg-[var(--color-spektre-elevated)]">
            <Activity className="h-4 w-4 text-[var(--color-spektre-blue)]" strokeWidth={2} />
          </span>
          σ-gate cascade
        </CardTitle>
        <CardDescription>
          Verdict pulse → cascade depth. Dark lab chrome; numbers in charts are illustrative unless bound to a receipt JSON.
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-6">
        <div>
          <p className="mb-2 text-[10px] font-semibold uppercase tracking-wider text-[var(--color-spektre-faint)]">Verdict</p>
          <div className="flex flex-wrap gap-2">
            {(["ACCEPT", "RETHINK", "ABSTAIN"] as const).map((v, i) => (
              <motion.span
                key={v}
                initial={{ opacity: 0, y: 6 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: i * 0.07, type: "spring", stiffness: 320, damping: 24 }}
              >
                <Badge
                  variant={v === "ACCEPT" ? "accept" : v === "RETHINK" ? "rethink" : "abstain"}
                  className={v === "ACCEPT" ? "ring-2 ring-[var(--color-spektre-accept)]/35" : ""}
                >
                  {v}
                </Badge>
              </motion.span>
            ))}
          </div>
        </div>
        <div>
          <p className="mb-2 flex items-center gap-1.5 text-[10px] font-semibold uppercase tracking-wider text-[var(--color-spektre-faint)]">
            <Layers className="h-3 w-3" aria-hidden />
            Stack
          </p>
          <div className="grid grid-cols-2 gap-2 sm:grid-cols-4">
            {layers.map((l, i) => (
              <motion.div
                key={l.id}
                initial={{ opacity: 0, y: 8 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: 0.15 + i * 0.04 }}
                className="rounded-lg border border-[var(--color-spektre-border)]/80 bg-[var(--color-spektre-bg-raised)]/80 px-3 py-2"
              >
                <div className="flex items-center justify-between gap-2">
                  <span className="text-xs font-semibold text-[var(--color-spektre-blue)]">{l.id}</span>
                  <Badge variant="muted" className="font-normal">
                    {l.label}
                  </Badge>
                </div>
                <p className="mt-1 text-[11px] leading-snug text-[var(--color-spektre-muted)]">{l.note}</p>
              </motion.div>
            ))}
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
