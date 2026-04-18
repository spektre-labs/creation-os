# v117 — σ-Long-Context: Paged KV-Cache with σ-Aware Eviction

BitNet 2B's native context window is 4096 tokens. That is too small
for sustained agent loops, multi-document reasoning, or long chats.
v117 makes a 4k-native model behave like a **32k-effective** model by
managing what we keep in the KV cache and what we offload to v115
σ-Memory — and it makes the eviction decisions explainable with
σ.

## The problem v117 solves

Every long-context system has to decide what to drop. Chroma 2025's
"context rot" paper documents this as a systemic failure mode:
models lose coherence when the caller silently truncates. Paged KV
caches (vLLM, Mooncake, llama.cpp's own) use LRU or FIFO — neither
of which tells you *why* a page was dropped.

v117 adds a third policy: **σ-aware LRU**. Each KV page carries the
σ_product observed during the decode step that wrote into it; the
evictor picks victims by

    score(page) = 100 · σ_product + 0.001 · (now_step − last_used_step)

under the default `COS_V117_EVICT_SIGMA_LRU` policy. Uncertain
context is evicted *before* confident context, and the offload hook
writes the evicted page's summary to v115 σ-Memory so the long-range
information is not lost — it is moved to persistent storage.

## Architecture

| Component            | Default | Purpose |
|----------------------|---------|---------|
| Page size            | 256 tok | Matches llama.cpp's internal page grid |
| Native context       | 4096    | Model's intrinsic window |
| Target context       | 32768   | Effective budget via eviction + offload |
| Sliding window       | 2048    | Recent tail kept with full attention, never evicted |
| Eviction policies    | LRU \| σ-LRU (default) \| σ-only | |
| Offload hook         | user-provided | Writes to v115 or a file |

## What ships in v117.0

- Page table + allocator (up to 512 pages)
- Ingest loop with automatic eviction when tokens > `native_ctx`
- Three eviction policies, each with self-test coverage
- Sliding-window protection: pages still inside the recent `N`
  tokens are never evicted
- Per-page σ update (`cos_v117_update_page_sigma`) so a later
  introspection pass can revise eviction ordering
- Offload hook invoked on every eviction with the page metadata and
  a user-data pointer
- JSON serialisation for observability (consumed by the upcoming
  `/v1/context` endpoint)

## What lands in v117.1

- Integration into `v101_bridge_generate` (compile-time flag; the
  hook is already in place in the kernel).
- Summarisation pass (a small text model) between "page evicted"
  and "written to v115" so the offload is useful as retrieved
  context.
- `GET /v1/context` HTTP endpoint on the v106 server.

## Source

- [`src/v117/long_context.h`](../../src/v117/long_context.h)
- [`src/v117/long_context.c`](../../src/v117/long_context.c)
- [`src/v117/main.c`](../../src/v117/main.c) — CLI (`--self-test`,
  `--simulate N SIGMA`)
- [`benchmarks/v117/check_v117_long_context_32k.sh`](../../benchmarks/v117/check_v117_long_context_32k.sh)

## Self-test

```bash
make check-v117
```

Runs:

1. `creation_os_v117_long_context --self-test` — defaults, σ-LRU
   eviction of the correct victim, SIGMA_ONLY policy picks highest
   σ regardless of age, plain LRU picks the oldest, and
   `update_page_sigma` changes the eviction choice.
2. `check_v117_long_context_32k.sh` — simulates 4096 tokens on a
   512-native configuration (8× over), asserts
   `evictions_total > 0`, `sliding_covered >= sliding_tokens`,
   `tokens_active ≤ native_ctx`, and that σ=0.9 "uncertain" pages
   appear in the evicted set under σ-LRU.

## Claim discipline

- v117.0 ships the **metadata layer + policies + tests**. The
  llama.cpp plumbing (KV tensors evicted on the C++ side) is scheduled
  for v117.1; `check-v117-long-context-32k` exercises the manager
  against a synthetic token stream, not a real transformer forward
  pass.
- We do **not** claim a measured win on LongBench / Needle-in-a-
  Haystack yet. The claim is that σ-aware eviction is a net-new
  eviction signal — "we explain which page we dropped and why".

## σ-stack layer

v117 sits between the **Generation** layer (v101 bridge) and the
**Training / Persistence** layer (v115 memory). It is the component
that converts a bounded-context model into an effectively unbounded
one while keeping σ auditable end-to-end.

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_
