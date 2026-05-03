"use client";

import { motion } from "motion/react";
import Link from "next/link";

export function HeroOverview() {
  return (
    <section className="relative flex min-h-[100svh] flex-col items-center justify-center overflow-hidden px-8 pt-24 pb-16 sm:px-12 md:px-16 lg:px-24 xl:px-28">
      <div className="pointer-events-none absolute inset-0 bg-[radial-gradient(ellipse_at_50%_-20%,rgba(92,225,197,0.12),transparent_55%)]" />
      <motion.div
        initial={{ opacity: 0, y: 24 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ duration: 0.7, ease: [0.22, 1, 0.36, 1] }}
        className="relative z-10 w-full max-w-4xl text-center"
      >
        <p className="mb-4 text-xs font-semibold uppercase tracking-[0.2em] text-[var(--spektre-accent)]">
          Creation OS
        </p>
        <h1 className="mb-6 text-4xl font-semibold tracking-tight md:text-6xl">
          Interrupt before harm.
          <br />
          <span className="text-[var(--spektre-muted)]">Steer with evidence.</span>
        </h1>
        <p className="mx-auto mb-10 max-w-2xl text-[var(--spektre-muted)]">
          The σ-gate turns uncertainty into an explicit verdict: ACCEPT, RETHINK, or ABSTAIN — with a
          cascade that prioritises the cheapest checks first.
        </p>
        <div className="flex flex-wrap justify-center gap-3">
          <Link
            href="/docs"
            className="rounded-lg bg-[var(--spektre-accent)] px-5 py-2.5 text-sm font-semibold text-black no-underline hover:opacity-90"
          >
            Documentation
          </Link>
          <Link
            href="/playground"
            className="rounded-lg border border-[var(--spektre-border)] px-5 py-2.5 text-sm font-medium text-[var(--spektre-fg)] no-underline hover:border-[var(--spektre-accent)]"
          >
            Playground
          </Link>
        </div>
      </motion.div>
    </section>
  );
}
