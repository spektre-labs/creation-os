import { motion } from "framer-motion";
import { Orbit } from "lucide-react";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./ui/Card";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "./ui/Tabs";

const phaseCount = 14;

export function OmegaLoop() {
  const ticks = Array.from({ length: phaseCount }, (_, i) => {
    const a = (i / phaseCount) * 2 * Math.PI - Math.PI / 2;
    const r = 48;
    const cx = 60 + r * Math.cos(a);
    const cy = 60 + r * Math.sin(a);
    return { cx, cy, i };
  });

  return (
    <Card data-testid="omega-loop">
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Orbit className="h-4 w-4 text-[var(--color-spektre-blue)]" aria-hidden />
          Ω loop
        </CardTitle>
        <CardDescription>Fourteen phases on a ring; tabs mirror “observe · decide · verify · memorize” groupings (pedagogy only).</CardDescription>
      </CardHeader>
      <CardContent className="space-y-6">
        <div className="relative flex min-h-[14rem] items-center justify-center">
          <svg viewBox="0 0 120 120" className="h-44 w-44 drop-shadow-[0_0_28px_oklch(0.45_0.15_250/0.12)]">
            <defs>
              <linearGradient id="omega-ring" x1="0%" y1="0%" x2="100%" y2="100%">
                <stop offset="0%" stopColor="var(--color-spektre-border)" stopOpacity="0.9" />
                <stop offset="100%" stopColor="var(--color-spektre-blue)" stopOpacity="0.35" />
              </linearGradient>
            </defs>
            <circle cx="60" cy="60" r="48" fill="none" stroke="url(#omega-ring)" strokeWidth="2" />
            {ticks.map(({ cx, cy, i }) => (
              <circle
                key={i}
                cx={cx}
                cy={cy}
                r={i % 3 === 0 ? 2.2 : 1.2}
                fill={i % 3 === 0 ? "var(--color-spektre-blue)" : "var(--color-spektre-faint)"}
                opacity={0.9}
              />
            ))}
            <motion.g
              animate={{ rotate: 360 }}
              transition={{ repeat: Infinity, duration: 22, ease: "linear" }}
              style={{ transformOrigin: "60px 60px" }}
            >
              <circle cx="60" cy="12" r="5" fill="var(--color-spektre-blue)" opacity="0.95" />
            </motion.g>
            <text x="60" y="64" textAnchor="middle" fill="var(--color-spektre-muted)" style={{ fontSize: "9px" }}>
              ∫σ
            </text>
          </svg>
          <span className="pointer-events-none absolute bottom-2 text-[10px] text-[var(--color-spektre-faint)]">
            {phaseCount} phases · argmin ∫σ dt (visual)
          </span>
        </div>

        <Tabs defaultValue="observe">
          <TabsList className="w-full justify-start sm:w-auto">
            <TabsTrigger value="observe">Observe</TabsTrigger>
            <TabsTrigger value="decide">Decide</TabsTrigger>
            <TabsTrigger value="verify">Verify</TabsTrigger>
            <TabsTrigger value="mem">Memorize</TabsTrigger>
          </TabsList>
          <TabsContent value="observe">
            Encode context, surface uncertainty, and map probes — Ω phases that feed cheap σ reads.
          </TabsContent>
          <TabsContent value="decide">
            Commit verdict bands (ACCEPT / RETHINK / ABSTAIN) and route spend to deeper cascade levels only when needed.
          </TabsContent>
          <TabsContent value="verify">
            Post-checks, receipts, and distortion probes — align with harness rows when a run JSON exists.
          </TabsContent>
          <TabsContent value="mem">
            Consolidation into slower tiers — dual-process “fast vs slow” is a separate card below.
          </TabsContent>
        </Tabs>
      </CardContent>
    </Card>
  );
}
