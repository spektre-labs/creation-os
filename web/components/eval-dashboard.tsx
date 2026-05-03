"use client";

import { motion } from "motion/react";
import Link from "next/link";

export function EvalDashboard() {
  return (
    <section className="border-t border-[var(--spektre-border)] bg-[var(--spektre-surface)] px-6 py-24">
      <div className="mx-auto max-w-4xl text-center">
        <h2 className="mb-4 text-2xl font-semibold">Eval dashboard</h2>
        <p className="mx-auto mb-8 max-w-xl text-sm text-[var(--spektre-muted)]">
          Live numbers ship from your repro bundle JSON — not hard-coded marketing figures in this static export.
        </p>
        <motion.div
          initial={{ opacity: 0, scale: 0.98 }}
          whileInView={{ opacity: 1, scale: 1 }}
          viewport={{ once: true }}
        >
          <Link
            href="/eval"
            className="inline-block rounded-lg border border-[var(--spektre-border)] px-6 py-3 text-sm font-medium no-underline hover:border-[var(--spektre-accent)]"
          >
            Open eval page →
          </Link>
        </motion.div>
      </div>
    </section>
  );
}
