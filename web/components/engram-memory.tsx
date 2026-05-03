"use client";

import { motion } from "motion/react";

export function EngramMemory() {
  return (
    <section className="border-t border-[var(--spektre-border)] bg-[var(--spektre-surface)] px-6 py-16">
      <div className="mx-auto max-w-4xl">
        <h2 className="mb-4 text-xl font-semibold">Engram hierarchy</h2>
        <p className="text-sm text-[var(--spektre-muted)]">
          Living weights and JSON sidecars in Python are teaching sketches only. Production memory policy stays under
          repository docs — never confuse demo counters with kernel guarantees.
        </p>
        <motion.ul
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          className="mt-6 grid gap-3 md:grid-cols-3"
        >
          {["kernel", "firmware", "application"].map((tier) => (
            <li key={tier} className="rounded-lg border border-[var(--spektre-border)] p-4 text-sm capitalize">
              {tier}
            </li>
          ))}
        </motion.ul>
      </div>
    </section>
  );
}
