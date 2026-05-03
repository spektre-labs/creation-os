"use client";

import { motion } from "motion/react";

export function OmegaLoop() {
  return (
    <section className="border-t border-[var(--spektre-border)] px-6 py-16">
      <div className="mx-auto max-w-4xl">
        <h2 className="mb-6 text-xl font-semibold">Ω-loop</h2>
        <div className="flex flex-wrap gap-2">
          {["OBSERVE", "ORIENT", "PREDICT", "ACT", "VERIFY"].map((ph, i) => (
            <motion.span
              key={ph}
              initial={{ opacity: 0, y: 6 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true }}
              transition={{ delay: i * 0.06 }}
              className="rounded-full border border-[var(--spektre-border)] px-3 py-1 font-mono text-xs text-[var(--spektre-accent)]"
            >
              {ph}
            </motion.span>
          ))}
        </div>
        <p className="mt-6 text-sm text-[var(--spektre-muted)]">
          Harness phases map to lab commands (`cos predict`, `cos think`, …); naming stays aligned with Ω-loop docs.
        </p>
      </div>
    </section>
  );
}
