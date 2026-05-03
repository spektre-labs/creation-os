import Link from "next/link";

export default function APIDoc() {
  return (
    <div className="mx-auto max-w-3xl px-6 py-20">
      <Link href="/docs" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
        ← Docs
      </Link>
      <h1 className="mt-6 text-3xl font-semibold">CLI reference (subset)</h1>
      <ul className="mt-6 flex list-disc flex-col gap-2 pl-6 text-[var(--spektre-muted)]">
        <li>
          <span className="font-mono text-[var(--spektre-fg)]">cos ttt</span> — TTT lab + Engram delta JSON
        </li>
        <li>
          <span className="font-mono text-[var(--spektre-fg)]">cos think</span> — JEPA planning / visualize
        </li>
        <li>
          <span className="font-mono text-[var(--spektre-fg)]">cos mcp</span> / <span className="font-mono">cos a2a</span>{" "}
          — registry + lab HTTP + Agent Card
        </li>
        <li>
          <span className="font-mono text-[var(--spektre-fg)]">cos deploy --docs</span> — runs <span className="font-mono">npm run build</span> here
        </li>
      </ul>
    </div>
  );
}
