import Link from "next/link";

export default function JEPADoc() {
  return (
    <div className="mx-auto max-w-3xl px-6 py-20">
      <Link href="/docs" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
        ← Docs
      </Link>
      <h1 className="mt-6 text-3xl font-semibold">σ-JEPA (v123 lab)</h1>
      <p className="mt-4 text-[var(--spektre-muted)]">
        Latent prediction with <span className="font-mono">SigmaJEPA.plan_argmin_sigma</span> and{" "}
        <span className="font-mono">cos think --prompt</span> / <span className="font-mono">--visualize</span>. Toy
        SIGReg-shaped penalty only — see module docstrings.
      </p>
    </div>
  );
}
