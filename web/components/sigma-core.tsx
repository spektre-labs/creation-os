"use client";

import { motion } from "motion/react";

export function SigmaCore() {
  return (
    <section className="border-t border-[var(--spektre-border)] px-6 py-16">
      <div className="mx-auto max-w-4xl">
        <motion.div
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          className="rounded-2xl border border-[var(--spektre-border)] bg-gradient-to-br from-[var(--spektre-surface)] to-black/40 p-8"
        >
          <h2 className="mb-3 text-xl font-semibold">σ core</h2>
          <p className="text-sm text-[var(--spektre-muted)]">
            Fixed-point state, deterministic interrupts, and explicit verdict typing — the same vocabulary from
            firmware-shaped builds to Python lab CLIs.
          </p>
        </motion.div>
      </div>
    </section>
  );
}
