import Link from "next/link";
import { OmegaLoop } from "@/components/omega-loop";

export default function OmegaLoopDoc() {
  return (
    <div>
      <div className="mx-auto max-w-3xl px-6 py-16">
        <Link href="/docs" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
          ← Docs
        </Link>
        <h1 className="mt-6 text-3xl font-semibold">Ω-loop</h1>
        <p className="mt-4 text-[var(--spektre-muted)]">
          Observe → orient → predict → act → verify: phase naming for harness and lab CLIs.
        </p>
      </div>
      <OmegaLoop />
    </div>
  );
}
