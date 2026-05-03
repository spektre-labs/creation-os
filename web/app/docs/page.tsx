import Link from "next/link";

const links = [
  { href: "/docs/sigma-gate", title: "σ-gate deep dive" },
  { href: "/docs/cascade", title: "Signal cascade" },
  { href: "/docs/omega-loop", title: "Ω-loop" },
  { href: "/docs/engram", title: "Memory hierarchy" },
  { href: "/docs/ttt", title: "Test-time training (lab)" },
  { href: "/docs/jepa", title: "σ-JEPA world model (lab)" },
  { href: "/docs/mcp", title: "MCP integration (lab)" },
  { href: "/docs/api", title: "API reference (CLI)" },
];

export default function DocsIndex() {
  return (
    <div className="mx-auto max-w-3xl px-6 py-20">
      <Link href="/" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
        ← Home
      </Link>
      <h1 className="mt-6 text-3xl font-semibold">Documentation</h1>
      <p className="mt-4 text-[var(--spektre-muted)]">
        Repository markdown remains canonical; these routes are a navigable shell for the static export.
      </p>
      <ul className="mt-10 flex flex-col gap-3">
        {links.map((l) => (
          <li key={l.href}>
            <Link href={l.href} className="text-[var(--spektre-accent)] no-underline hover:underline">
              {l.title}
            </Link>
          </li>
        ))}
      </ul>
    </div>
  );
}
