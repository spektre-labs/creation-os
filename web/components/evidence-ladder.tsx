"use client";

import { motion } from "motion/react";

export function EvidenceLadder() {
  return (
    <section className="border-t border-[var(--spektre-border)] px-6 py-24">
      <div className="mx-auto max-w-4xl">
        <h2 className="mb-6 text-2xl font-semibold">Evidence ladder</h2>
        <motion.blockquote
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          className="border-l-4 border-[var(--spektre-accent)] pl-6 text-lg leading-relaxed text-[var(--spektre-muted)]"
        >
          Truth is built, not claimed: micro-bench throughput, harness scores, and silicon claims stay in
          separate sentences with archived repro metadata per <span className="font-mono">CLAIM_DISCIPLINE</span>.
        </motion.blockquote>
      </div>
    </section>
  );
}
