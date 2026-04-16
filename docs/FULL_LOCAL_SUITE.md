# Full local “suite” scope (what the repo ships vs what scripts often promise)

This document exists because it is easy to mix **three different classes** of work:

1. **Portable merge-gate software** (C11, `make merge-gate`, evidence classes in [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md)).
2. **Local integration shims** (OpenAI-shaped loopback servers, editor configs).
3. **A full product surface** (web UI, autonomous filesystem tools, persona-heavy chat modes, paper pipelines, automatic weight downloads).

This repository intentionally keeps **(1)** strict and **(2)** honest. **(3)** is not “one bash heredoc away” without also choosing **security**, **licensing**, **repro bundles**, and **non-deceptive marketing**.

## What already ships (real, merge-gate or optional)

| Artifact | Purpose | Command |
|:--|:--|:--|
| LM integration shell (v28) | GGUF introspection, mmap reads, sampling, HTTP chat shape, external engine hook | `make check-v28` |
| Collapse harness (v29) | mmap tensor views, σ channels, XNOR toy, BitNet forward stub | `make check-v29` |
| OpenAI-shaped loopback stub | `/v1/models`, `/v1/chat/completions`, `/v1/completions` (deterministic; no SSE). Loopback + **CORS** for local browser demos. | `make check-openai-stub` |
| Suite lab | Static mode metadata CLI + optional static page hitting the stub | [SUITE_LAB.md](SUITE_LAB.md) · `make test-suite-stub` · `./scripts/launch_suite.sh` |

## What a typical “full suite” script implied (not shipped as-is)

- **Autonomous coding agent** with unrestricted shell/filesystem access is a **security product**, not a drive-by C file.
- **Web UI + streaming** needs a coherent backend contract (SSE or WebSocket), auth, and abuse controls beyond a static HTML demo.
- **LaTeX / Zenodo automation** belongs in a separate release pipeline with credentials handling and audit logs.
- **“Replaces Cursor/Claude”** is a **go-to-market claim**, not a compiler output.

## A sane incremental path (if you want this direction)

1. Keep using **`creation_os_openai_stub`** to validate editor wiring (Continue/Aider) against `/v1`.
2. If you need a real model behind `/v1`, wire a **known external engine** behind the stub (or replace the stub) and archive harness rows per [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md).
3. Only then add **tooling** behind a separate binary with explicit sandboxing, config, and tests.

## Scripts

- `scripts/bootstrap_coding_agent.sh` — builds the stub and runs `--self-test`; does not download weights.
- `scripts/launch_suite.sh` — starts the stub on `:8080` (configurable via `STUB_PORT`) and, when `python3` is available, serves `web/static` on `:3000` (`HTTP_PORT`) so you can open `suite_lab.html`. **Ctrl+C** stops the foreground wait and kills the stub (and static server) via `trap`. No automatic weight download.
