"use client";

import { motion } from "motion/react";

const steps = [
  { k: "Encode", d: "Signals feed fixed-point σ state (lab: Python core)." },
  { k: "Update", d: "σ and k_eff move under policy; no sigma_gate.h edits from this site." },
  { k: "Verdict", d: "ACCEPT / RETHINK / ABSTAIN drives downstream tools and memory." },
];

export function GateFlow() {
  return (
    <section className="border-t border-[var(--spektre-border)] px-6 py-24">
      <div className="mx-auto max-w-4xl">
        <h2 className="mb-10 text-2xl font-semibold">σ-gate flow</h2>
        <ol className="grid gap-6 md:grid-cols-3">
          {steps.map((s, i) => (
            <motion.li
              key={s.k}
              initial={{ opacity: 0, y: 12 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-40px" }}
              transition={{ delay: i * 0.08, duration: 0.45 }}
              className="rounded-xl border border-[var(--spektre-border)] bg-[var(--spektre-surface)] p-5"
            >
              <span className="text-xs font-mono text-[var(--spektre-accent)]">L{i + 1}</span>
              <h3 className="mt-2 font-medium">{s.k}</h3>
              <p className="mt-2 text-sm text-[var(--spektre-muted)]">{s.d}</p>
            </motion.li>
          ))}
        </ol>
      </div>
    </section>
  );
}
