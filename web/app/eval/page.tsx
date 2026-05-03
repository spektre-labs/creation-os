import Link from "next/link";
import sample from "@/data/eval-sample.json";

type Metric = { name: string; value: string | null; note?: string };

export default function EvalPage() {
  const data = sample as { title?: string; metrics?: Metric[] };
  const metrics = data.metrics || [];
  return (
    <div className="mx-auto max-w-3xl px-6 py-20">
      <Link href="/" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
        ← Home
      </Link>
      <h1 className="mt-6 text-3xl font-semibold">Eval dashboard</h1>
      <p className="mt-4 text-[var(--spektre-muted)]">{data.title || "Static sample — wire your JSON export."}</p>
      <ul className="mt-8 flex flex-col gap-4">
        {metrics.map((m) => (
          <li key={m.name} className="rounded-lg border border-[var(--spektre-border)] bg-[var(--spektre-surface)] p-4">
            <div className="font-mono text-sm text-[var(--spektre-accent)]">{m.name}</div>
            <div className="mt-1 text-lg">{m.value === null ? "—" : String(m.value)}</div>
            {m.note ? <div className="mt-2 text-sm text-[var(--spektre-muted)]">{m.note}</div> : null}
          </li>
        ))}
      </ul>
    </div>
  );
}
