import Link from "next/link";
import { SignalCascade } from "@/components/signal-cascade";

export default function CascadeDoc() {
  return (
    <div>
      <div className="mx-auto max-w-3xl px-6 py-16">
        <Link href="/docs" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
          ← Docs
        </Link>
        <h1 className="mt-6 text-3xl font-semibold">Signal cascade</h1>
        <p className="mt-4 text-[var(--spektre-muted)]">
          Layered probes from cheap gates to heavier interpretability hooks; budgeted before full interrupt cost.
        </p>
      </div>
      <SignalCascade />
    </div>
  );
}
