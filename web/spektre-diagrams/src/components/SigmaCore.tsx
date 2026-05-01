import { motion } from "framer-motion";
import { useEffect, useState } from "react";
import { RadialBar, RadialBarChart, ResponsiveContainer } from "recharts";
import { Badge } from "./ui/Badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./ui/Card";

const gauge = [{ name: "σ", value: 72, fill: "var(--color-spektre-blue)" }];

const SAMPLE = "Epistemic strain ↓ when reconstruction holds…";

export function SigmaCore() {
  const [text, setText] = useState("");

  useEffect(() => {
    let i = 0;
    const n = SAMPLE.length + 14;
    const id = window.setInterval(() => {
      i = (i + 1) % n;
      setText(i <= SAMPLE.length ? SAMPLE.slice(0, i) : SAMPLE);
    }, 42);
    return () => clearInterval(id);
  }, []);

  return (
    <Card data-testid="sigma-core">
      <CardHeader>
        <CardTitle>σ core</CardTitle>
        <CardDescription>Input → sparse bottleneck read → radial gauge (illustrative). σ = Σ wᵢ · n(sᵢ) is not evaluated here.</CardDescription>
      </CardHeader>
      <CardContent className="grid gap-6 md:grid-cols-3">
        <motion.div
          className="flex min-h-[8.5rem] flex-col rounded-xl border border-[var(--color-spektre-border)] bg-[var(--color-spektre-bg-raised)]/90 p-4 font-mono text-xs leading-relaxed text-[var(--color-spektre-muted)]"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
        >
          <span className="mb-2 text-[10px] font-sans font-semibold uppercase tracking-wider text-[var(--color-spektre-faint)]">
            Hidden (lab)
          </span>
          <span className="min-h-[4.5rem] text-[var(--color-spektre-text)]">
            {text}
            <motion.span className="ml-0.5 inline-block h-3.5 w-px bg-[var(--color-spektre-blue)] align-middle" aria-hidden animate={{ opacity: [1, 0.15, 1] }} transition={{ duration: 1, repeat: Infinity }} />
          </span>
        </motion.div>
        <div className="relative min-h-52 w-full min-w-0 md:col-span-1">
          <div
            className="pointer-events-none absolute inset-0 rounded-xl bg-[radial-gradient(circle_at_50%_55%,var(--color-spektre-blue-glow)_0%,transparent_62%)] opacity-30"
            aria-hidden
          />
          <div className="relative h-full min-h-52 w-full min-w-0">
            <ResponsiveContainer width="100%" height="100%" minHeight={208}>
              <RadialBarChart innerRadius="58%" outerRadius="100%" data={gauge} startAngle={90} endAngle={-270}>
                <RadialBar dataKey="value" cornerRadius={6} background={{ fill: "var(--color-spektre-border)", opacity: 0.35 }} />
              </RadialBarChart>
            </ResponsiveContainer>
          </div>
          <p className="absolute bottom-1 left-0 right-0 text-center text-[10px] text-[var(--color-spektre-faint)]">readout · 0–100</p>
        </div>
        <div className="flex flex-col justify-center gap-3">
          <Badge variant="accept" className="w-fit px-3 py-1">
            ACCEPT
          </Badge>
          <p className="text-xs leading-relaxed text-[var(--color-spektre-muted)]">
            Weights <span className="font-mono text-[var(--color-spektre-text)]">wᵢ</span> are wiring knobs in product; this panel is visual
            only.
          </p>
        </div>
      </CardContent>
    </Card>
  );
}
