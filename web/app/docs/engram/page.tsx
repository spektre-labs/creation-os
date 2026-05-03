import Link from "next/link";
import { EngramMemory } from "@/components/engram-memory";

export default function EngramDoc() {
  return (
    <div>
      <div className="mx-auto max-w-3xl px-6 py-16">
        <Link href="/docs" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
          ← Docs
        </Link>
        <h1 className="mt-6 text-3xl font-semibold">Engram memory</h1>
        <p className="mt-4 text-[var(--spektre-muted)]">
          Hierarchy sketches and JSON sidecars in Python labs; separate from silicon memory claims.
        </p>
      </div>
      <EngramMemory />
    </div>
  );
}
