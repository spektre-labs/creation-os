import Link from "next/link";
import { HeroOverview } from "@/components/hero-overview";
import { GateFlow } from "@/components/gate-flow";
import { SignalCascade } from "@/components/signal-cascade";
import { EvidenceLadder } from "@/components/evidence-ladder";
import { EvalDashboard } from "@/components/eval-dashboard";

export default function HomePage() {
  return (
    <main className="flex flex-col gap-0">
      <HeroOverview />
      <section className="border-t border-[var(--spektre-border)] bg-[var(--spektre-surface)] px-6 py-20">
        <p className="mx-auto max-w-3xl text-center text-xl font-medium tracking-tight md:text-2xl">
          Every AI answers.
          <span className="text-[var(--spektre-accent)]"> Creation OS measures first.</span>
        </p>
      </section>
      <GateFlow />
      <SignalCascade />
      <EvidenceLadder />
      <EvalDashboard />
      <section className="border-t border-[var(--spektre-border)] px-6 py-16 text-center">
        <p className="text-[var(--spektre-muted)] mb-6 text-sm">
          Python package and source: use the canonical repository; this site is static export–friendly.
        </p>
        <div className="flex flex-wrap justify-center gap-4">
          <code className="rounded border border-[var(--spektre-border)] bg-black/40 px-4 py-2 text-sm">
            pip install creation-os
          </code>
          <Link
            href="https://github.com/spektre-labs/creation-os"
            className="rounded border border-[var(--spektre-accent)] px-4 py-2 text-sm font-medium text-[var(--spektre-accent)] no-underline hover:bg-[var(--spektre-accent)]/10"
          >
            GitHub — spektre-labs/creation-os
          </Link>
        </div>
      </section>
      <footer className="border-t border-[var(--spektre-border)] px-6 py-8 text-center text-xs text-[var(--spektre-muted)]">
        Spektre Labs · lab UI only · see repository AGENTS.md for git policy
      </footer>
    </main>
  );
}
