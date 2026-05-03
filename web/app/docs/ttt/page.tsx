import Link from "next/link";

export default function TTTDoc() {
  return (
    <div className="mx-auto max-w-3xl px-6 py-20">
      <Link href="/docs" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
        ← Docs
      </Link>
      <h1 className="mt-6 text-3xl font-semibold">Test-time training (v123 lab)</h1>
      <p className="mt-4 text-[var(--spektre-muted)]">
        σ-gated in-place projection updates with chunk-wise steps: <span className="font-mono">cos ttt --learn</span>,{" "}
        <span className="font-mono">cos ttt --eval --before-after</span>. Not a reproduction of vendor TTT stacks.
      </p>
    </div>
  );
}
