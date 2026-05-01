import { motion } from "framer-motion";
import { Scale } from "lucide-react";
import { Badge } from "./ui/Badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./ui/Card";

const rungs: { title: string; hint: string; honest?: boolean }[] = [
  { title: "Theory", hint: "Definitions, lemmas, scope boundaries." },
  { title: "Simulation", hint: "Executable demos; not silicon or frontier LM proof." },
  { title: "Negative results", hint: "Explicit non-goals and falsifiers — publish the dead ends.", honest: true },
  { title: "Harness row", hint: "Archived json + harness SHA; see CLAIM_DISCIPLINE." },
  { title: "Microbench", hint: "Host-tied throughput; never merge with MMLU in one headline." },
  { title: "Invariant", hint: "Must hold every run (bit identities, gate laws)." },
  { title: "Receipt", hint: "Hash-chained audit for a σ decision path." },
];

export function EvidenceLadder() {
  return (
    <Card data-testid="evidence-ladder">
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Scale className="h-4 w-4 text-[var(--color-spektre-muted)]" aria-hidden />
          Evidence ladder
        </CardTitle>
        <CardDescription>Strongest claims sit on lower rungs only with matching evidence class. Expand rows for maintainer hints.</CardDescription>
      </CardHeader>
      <CardContent>
        <div className="relative pl-8">
          <div
            className="absolute bottom-2 left-[11px] top-2 w-px bg-gradient-to-b from-[var(--color-spektre-blue)]/55 via-[var(--color-spektre-border)] to-transparent"
            aria-hidden
          />
          <ul className="space-y-2">
            {rungs.map((r, i) => (
              <motion.li
                key={r.title}
                initial={{ opacity: 0, x: -6 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: i * 0.04 }}
                className="relative"
              >
                <span
                  className="absolute left-[-22px] top-[14px] h-2 w-2 rounded-full border-2 border-[var(--color-spektre-surface)] bg-[var(--color-spektre-blue)]"
                  aria-hidden
                />
                <details className="group rounded-xl border border-[var(--color-spektre-border)]/90 bg-[var(--color-spektre-bg-raised)]/70 px-3 py-2 transition-colors hover:border-[var(--color-spektre-border)]">
                  <summary className="flex cursor-pointer list-none items-center justify-between gap-2 text-sm text-[var(--color-spektre-text)] [&::-webkit-details-marker]:hidden">
                    <span>{r.title}</span>
                    {r.honest ? (
                      <Badge variant="abstain" className="shrink-0">
                        Honest boundaries
                      </Badge>
                    ) : null}
                  </summary>
                  <p className="mt-2 border-t border-[var(--color-spektre-border)]/50 pt-2 text-xs leading-relaxed text-[var(--color-spektre-muted)]">
                    {r.hint}
                  </p>
                </details>
              </motion.li>
            ))}
          </ul>
        </div>
      </CardContent>
    </Card>
  );
}
