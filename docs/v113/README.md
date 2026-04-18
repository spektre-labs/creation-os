# v113 — σ-Sandbox: LLM-in-Sandbox with a σ-Gate

Bounded subprocess execution for LLM-generated code, with a σ-gate
that refuses to run code the model isn't confident about.

## Why

Cheng et al. 2025 (“LLM-in-Sandbox”) identified three meta-capabilities
that every agentic framework needs:

1. `execute_bash(cmd)` — run a command,
2. `str_replace_editor` — edit files,
3. `submit()` — return a structured result to the orchestrator.

Every framework that ships these capabilities today (Anthropic MCP,
OpenAI Assistants, Devin, SWE-agent, OpenHands) runs the code **blindly
trusting the model's next-token distribution**. If the model was
guessing, the sandbox still runs the guess.

v113 adds one ingredient: a σ-gate on the code-generation step itself.
If σ_product > τ_code on the tokens that produced the program, the
sandbox **refuses to execute** and returns a diagnostic:

```json
{
  "executed":  false,
  "gated_out": true,
  "gate_reason": "low_confidence_code (σ=0.78>τ=0.60)",
  "sigma_product": 0.78,
  "tau_code": 0.60
}
```

## Isolation

v113 uses the portable subset of POSIX resource limits:

- `RLIMIT_CPU`  — `timeout + 1` seconds (SIGXCPU if exceeded);
- `RLIMIT_AS`   — 512 MiB address space (Linux only; macOS ignores);
- `RLIMIT_FSIZE` — 64 MiB max single-file write;
- `RLIMIT_CORE` — 0 (no core dumps);
- a new **process group** so we can kill the whole child tree on
  deadline (`kill(-pgid, SIGKILL)`);
- stdin redirected to `/dev/null`;
- env reset to `PATH=/usr/bin:/bin:/usr/local/bin`, `HOME=/tmp`,
  `PYTHONPATH`/`LD_LIBRARY_PATH` unset.

This is **not a security boundary**. It's a "don't accidentally fill
your disk / spawn a fork-bomb / exfiltrate your env" fence. For a real
boundary, run v113 inside the v107 Docker image (or Firecracker /
gVisor). seccomp-bpf support is a compile-time option
(`COS_V113_SECCOMP=1`) planned for v115 on Linux.

## API surface

```
POST /v1/sandbox/execute
{
  "code":          "print('hello')",
  "language":      "python" | "bash" | "sh",
  "timeout":       30,         // 1..120, default 30
  "sigma_product": 0.42,        // optional; if set AND > tau_code, skip execution
  "tau_code":      0.60         // default 0.60
}
```

Receipt:

```json
{
  "executed":      true,
  "gated_out":     false,
  "timed_out":     false,
  "exit_code":     0,
  "wall_seconds":  0.031,
  "stdout":        "...",
  "stderr":        "",
  "sigma_product": 0.42,
  "tau_code":      0.60,
  "gate_reason":   ""
}
```

## Integration with v112

The `execute_code` tool is the canonical entry point. Register it in
the `tools` array of a chat completion; when the model emits

```
<tool_call>{"name":"execute_code","arguments":{"code":"…","language":"python"}}</tool_call>
```

the v112 σ-gate runs first on the *tool-selection* decision (does the
model know which tool to call?), and — if it allows the call — the v113
σ-gate runs on the *code generation* itself (is the code confident?).
Two independent measurements, two independent thresholds.

## Source

- [`src/v113/sandbox.h`](../../src/v113/sandbox.h) — API
- [`src/v113/sandbox.c`](../../src/v113/sandbox.c) — fork + rlimit +
  deadline loop + JSON receipt
- [`src/v113/main.c`](../../src/v113/main.c) — CLI
- [`benchmarks/v113/check_v113_sandbox_execute.sh`](../../benchmarks/v113/check_v113_sandbox_execute.sh)
  — self-test + σ-gate reject + live `echo` smoke

## Self-test

```bash
make check-v113
```

runs the pure-C self-test (which includes an actual `sleep 6` fork with
a 1 s deadline to exercise the SIGKILL path) plus two JSON-body
scenarios.

## Claim discipline

- v113 **is not** a hardened sandbox. We are explicit about this in
  every code comment that touches isolation. The merge-gate tests the
  deadline + resource-limit + σ-gate surfaces, not the security
  boundary.
- On a real Creation OS deployment, run v113 inside the v107 Docker
  image (which runs as a non-root `cos` user with no writable bind
  mounts), or inside Firecracker / gVisor.

## σ-stack layer

v113 sits at the **σ-governance** layer (σ-gate) + **Generation** layer
(subprocess execution). It consumes v101 σ-profiles and emits
execution receipts.

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_
