import Link from "next/link";

export default function MCPDoc() {
  return (
    <div className="mx-auto max-w-3xl px-6 py-20">
      <Link href="/docs" className="text-sm text-[var(--spektre-muted)] no-underline hover:underline">
        ← Docs
      </Link>
      <h1 className="mt-6 text-3xl font-semibold">MCP lab surface</h1>
      <p className="mt-4 text-[var(--spektre-muted)]">
        <span className="font-mono">cos mcp --serve-http</span> exposes a minimal JSON-RPC ping; production transport
        remains <span className="font-mono">creation_os_sigma_mcp</span>. <span className="font-mono">cos mcp --connect-url</span>{" "}
        probes remote health.
      </p>
    </div>
  );
}
