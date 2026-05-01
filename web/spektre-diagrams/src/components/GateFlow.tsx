import { motion } from "framer-motion";
import { ChevronRight, GitBranch } from "lucide-react";
import { Badge } from "./ui/Badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./ui/Card";

const steps = [
  "Encode",
  "σ read",
  "Cascade L1–L5",
  "Verdict",
  "Regenerate?",
  "Post-check",
] as const;

export function GateFlow() {
  return (
    <Card data-testid="gate-flow">
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <GitBranch className="h-4 w-4 text-[var(--color-spektre-blue)]" aria-hidden />
          Pipeline
        </CardTitle>
        <CardDescription>Decision path with an optional regenerate loop (dashed). Post-check folds LSD, HIDE, ICR, entropy mini-signals.</CardDescription>
      </CardHeader>
      <CardContent>
        <div className="flex flex-col gap-3 lg:flex-row lg:flex-wrap lg:items-stretch">
          {steps.map((s, i) => (
            <motion.div
              key={s}
              initial={{ opacity: 0, x: -10 }}
              animate={{ opacity: 1, x: 0 }}
              transition={{ delay: i * 0.06, type: "spring", stiffness: 280, damping: 22 }}
              className="flex items-center gap-2 lg:flex-1 lg:min-w-0"
            >
              <div className="flex min-w-0 flex-1 flex-col gap-1.5 rounded-xl border border-dashed border-[var(--color-spektre-border)] bg-[var(--color-spektre-bg-raised)]/60 px-3 py-2.5">
                <div className="flex items-center gap-2">
                  <Badge
                    variant="blue"
                    className="flex !h-6 !w-6 shrink-0 items-center justify-center !px-0 !py-0 text-[10px] leading-none"
                  >
                    {i + 1}
                  </Badge>
                  <span className="truncate text-sm text-[var(--color-spektre-text)]">{s}</span>
                </div>
              </div>
              {i < steps.length - 1 ? (
                <ChevronRight className="hidden h-4 w-4 shrink-0 text-[var(--color-spektre-faint)] lg:block" aria-hidden />
              ) : null}
            </motion.div>
          ))}
        </div>
        <div className="mt-4 flex flex-wrap gap-2 border-t border-[var(--color-spektre-border)]/60 pt-4">
          {(["LSD", "HIDE", "ICR", "Entropy"] as const).map((x) => (
            <Badge key={x} variant="muted">
              {x}
            </Badge>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}
