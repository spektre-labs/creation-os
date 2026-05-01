import { motion } from "framer-motion";
import { Bar, BarChart, ResponsiveContainer, XAxis } from "recharts";
import { Zap } from "lucide-react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./ui/Card";

const costs = [
  { name: "L1", mult: 1 },
  { name: "L2", mult: 5 },
  { name: "L3", mult: 20 },
  { name: "L4", mult: 100 },
  { name: "L5", mult: 1000 },
];

export function SignalCascade() {
  return (
    <Card data-testid="signal-cascade">
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Zap className="h-4 w-4 text-[var(--color-spektre-rethink)]" aria-hidden />
          Signal cascade
        </CardTitle>
        <CardDescription>Relative cost multipliers per level. Early exit keeps work in L1–L2 when σ is calm (illustrative).</CardDescription>
      </CardHeader>
      <CardContent className="grid gap-6 lg:grid-cols-2">
        <div className="space-y-3">
          {costs.map((c, i) => (
            <motion.div key={c.name} initial={{ opacity: 0 }} animate={{ opacity: 1 }} transition={{ delay: i * 0.05 }}>
              <div className="mb-1 flex justify-between text-xs text-[var(--color-spektre-muted)]">
                <span>{c.name}</span>
                <span className="font-mono text-[var(--color-spektre-text)]">{c.mult}×</span>
              </div>
              <div className="h-2.5 overflow-hidden rounded-full bg-[var(--color-spektre-border)]/50">
                <motion.div
                  className="h-full rounded-full bg-gradient-to-r from-[var(--color-spektre-blue)] to-[var(--color-spektre-blue-glow)]"
                  initial={{ scaleX: 0 }}
                  animate={{ scaleX: Math.min(1, c.mult / 1000) }}
                  transition={{ duration: 0.55, delay: i * 0.06, ease: [0.22, 1, 0.36, 1] }}
                  style={{ transformOrigin: "left" }}
                />
              </div>
            </motion.div>
          ))}
          <div className="rounded-xl border border-[var(--color-spektre-rethink)]/35 bg-[var(--color-spektre-rethink)]/8 p-3">
            <p className="text-[10px] font-semibold uppercase tracking-wider text-[var(--color-spektre-rethink)]">Early exit</p>
            <p className="mt-1 text-xs text-[var(--color-spektre-muted)]">
              When σ &lt; τ_hi at cheap probes, skip deep L4–L5 spend — intent only here, not a measured fraction.
            </p>
          </div>
        </div>
        <div className="flex flex-col gap-3">
          <div className="h-56 w-full min-h-[14rem] min-w-0">
            <ResponsiveContainer width="100%" height="100%" minHeight={224}>
              <BarChart data={costs} margin={{ top: 8, right: 8, left: 0, bottom: 0 }}>
                <XAxis dataKey="name" stroke="var(--color-spektre-muted)" fontSize={11} tickLine={false} axisLine={false} />
                <Bar dataKey="mult" fill="var(--color-spektre-blue)" radius={[5, 5, 0, 0]} opacity={0.9} />
              </BarChart>
            </ResponsiveContainer>
          </div>
          <div className="flex items-end justify-between gap-3 rounded-lg border border-[var(--color-spektre-border)]/80 bg-[var(--color-spektre-bg-raised)]/80 px-4 py-3">
            <div>
              <p className="text-[10px] font-medium uppercase tracking-wider text-[var(--color-spektre-faint)]">Resolves ≤ L2 · demo</p>
              <p className="mt-1 flex items-baseline gap-2">
                <motion.span
                  className="font-mono text-2xl font-semibold tabular-nums text-[var(--color-spektre-blue)]"
                  initial={{ opacity: 0, scale: 0.96 }}
                  animate={{ opacity: 1, scale: 1 }}
                  transition={{ delay: 0.35, type: "spring", stiffness: 200, damping: 20 }}
                >
                  87
                </motion.span>
                <span className="text-sm text-[var(--color-spektre-muted)]">% · placeholder</span>
              </p>
            </div>
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
