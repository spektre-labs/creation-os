"use client";

import { motion } from "motion/react";

const layers = [
  { id: "L1", name: "Cheap heuristics", note: "Run first; bail early when safe." },
  { id: "L2", name: "Structured checks", note: "Formats, tool traces, channel splits." },
  { id: "L3", name: "Memory / Engram", note: "Recall vs novel; abstain on trojans." },
  { id: "L4", name: "Heavier probes", note: "SAE / interpretability lab hooks." },
  { id: "L5", name: "Full interrupt", note: "Receipts, proof hints, governance." },
];

export function SignalCascade() {
  return (
    <section className="scroll-mt-20 border-t border-[var(--spektre-border)] bg-[var(--spektre-surface)] px-6 py-24">
      <div className="mx-auto max-w-4xl">
        <h2 className="mb-4 text-2xl font-semibold">Signal cascade</h2>
        <p className="mb-10 max-w-2xl text-sm text-[var(--spektre-muted)]">
          Cheapest first: the cascade is a budget ladder, not a single scalar shortcut.
        </p>
        <div className="flex flex-col gap-3">
          {layers.map((l, i) => (
            <motion.div
              key={l.id}
              initial={{ opacity: 0, x: -12 }}
              whileInView={{ opacity: 1, x: 0 }}
              viewport={{ once: true }}
              transition={{ delay: i * 0.05 }}
              className="flex items-baseline gap-4 rounded-lg border border-[var(--spektre-border)] bg-black/20 px-4 py-3"
            >
              <span className="w-10 font-mono text-[var(--spektre-accent)]">{l.id}</span>
              <div>
                <div className="font-medium">{l.name}</div>
                <div className="text-sm text-[var(--spektre-muted)]">{l.note}</div>
              </div>
            </motion.div>
          ))}
        </div>
      </div>
    </section>
  );
}
