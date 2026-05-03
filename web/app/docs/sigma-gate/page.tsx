import Link from "next/link";
import { SigmaCore } from "@/components/sigma-core";

export default function SigmaGateDoc() {
  return (
    <div>
      <div className="mx-auto max-w-3xl px-6 py-16">
        <Link href="/docs" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
          ← Docs
        </Link>
        <h1 className="mt-6 text-3xl font-semibold">σ-gate</h1>
        <p className="mt-4 text-[var(--spektre-muted)]">
          Fixed-point cognitive interrupt: embedded builds use <span className="font-mono">sigma_gate.h</span>;
          Python uses <span className="font-mono">sigma_gate_core</span>. Verdict vocabulary is ACCEPT / RETHINK /
          ABSTAIN.
        </p>
      </div>
      <SigmaCore />
    </div>
  );
}
