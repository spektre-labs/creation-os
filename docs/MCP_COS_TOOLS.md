# `cos-mcp` — Creation OS as MCP infrastructure

*NEXT-4 deliverable.  Any MCP-capable agent can dial in over stdio
and ask Creation OS for a σ-gate before committing to an answer.*

The existing σ-MCP (`creation_os_sigma_mcp`, OMEGA-1) exposes the
three σ-**primitives** (`sigma_measure`, `sigma_gate`,
`sigma_diagnostic`) to external clients.  `cos-mcp` is the newer,
**product-level** MCP server: it exposes the six `cos.*` tools an
agent actually wants — one call to decide whether to answer, one
call to decide *how sure* the answer is, and one call to read
back what Creation OS just cached.

## Tools

| Tool                  | Arguments                 | Result shape                                                                    |
|-----------------------|---------------------------|---------------------------------------------------------------------------------|
| `cos.chat`            | `{ "prompt": string }`    | `response, sigma, action, cache, rethink_count, route, elapsed_ms, cost_eur`    |
| `cos.sigma`           | `{ "prompt": string }`    | `sigma_combined, sigma_logprob, sigma_entropy, sigma_perplexity, sigma_consistency, action` |
| `cos.calibrate`       | `{ "path"?: string }`     | `valid, tau, alpha, domain, path` (loads `~/.cos/calibration.json` by default)  |
| `cos.health`          | `{}`                      | `version, proofs_lean, proofs_frama_c, engram_count, session{…}, cost_eur`      |
| `cos.engram.lookup`   | `{ "prompt": string }`    | `hit, sigma?, prompt_hash, response?`                                           |
| `cos.introspect`      | `{ "prompt": string }`    | `sigma_perception, sigma_self, sigma_social, sigma_situational, word_count`     |

### `action` values

`ACCEPT`, `RETHINK`, or `ABSTAIN`.  Callers that want a hard
"do not answer" gate can treat any non-`ACCEPT` action as a block.

## Transport

Line-delimited JSON-RPC 2.0 over stdio, per the MCP 2026-03-26
spec:

```
{"jsonrpc":"2.0","id":1,"method":"initialize"}
{"jsonrpc":"2.0","id":2,"method":"tools/list"}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"cos.chat","arguments":{"prompt":"What is 2+2?"}}}
```

Modes:

- `./cos-mcp --stdio` (default) — server loop on stdin/stdout.
- `./cos-mcp --once` — answer one line and exit (handy for CI).
- `./cos-mcp --demo` — emit a canonical JSON envelope exercising
  every tool once (used by `check-cos-mcp`).

## Why this makes Creation OS **infrastructure**

A 2026-era agent runtime (Claude Code, Cursor, Windsurf, OpenClaw,
Hermes Agent, etc.) can register `cos-mcp` as an MCP server and:

1. **Filter its own hallucinations** — before committing to a
   generated answer, call `cos.sigma` and abort if
   `sigma_combined > τ`.
2. **Share cached answers** — call `cos.engram.lookup` first; if a
   prior session already answered the same prompt with low σ,
   reuse the reply (€0.00, 0 ms).
3. **Gate its own tool use** — pair `cos.introspect` with the
   existing σ-MCP gate to refuse ambiguous tool invocations.
4. **Measure its own drift** — poll `cos.health.session.coherence`
   to detect `DRIFTING` / `AT_RISK` states and trigger Ω-loop
   self-retraining.

σ-gate stops being a Creation-OS-only feature and becomes a
first-class service any agent can subscribe to — **the real promise
of open-standard MCP in 2026**.

## Evidence

```bash
make cos-mcp
make check-cos-mcp          # smoke test
./cos-mcp --demo            # canonical envelope
```

Harness lives at
[`benchmarks/sigma_pipeline/check_cos_mcp.sh`](../benchmarks/sigma_pipeline/check_cos_mcp.sh).
The σ-primitives MCP (`sigma_measure` / `sigma_gate` /
`sigma_diagnostic`) remains at
[`docs/MCP_SIGMA.md`](MCP_SIGMA.md) — `cos-mcp` is the product-level
complement, not a replacement.
