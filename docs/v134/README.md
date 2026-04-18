# v134 σ-Spike — Event-driven σ signalling

Source: `src/v134/spike.[ch]` · `src/v134/main.c`
Gate:   `benchmarks/v134/check_v134_spike_event_driven.sh`
Build:  `make creation_os_v134_spike` · `make check-v134`

## Problem

σ-channels today are float32 values recomputed **on every token**.
That's eight geometric-mean reductions per forward pass. Each
channel recomputation is tiny, but the aggregate is a steady-state
cost the σ-layer pays whether anything is happening or not.

The biological analogue is wrong on purpose. Cortical neurons
don't fire continuously — they spike only when something changes.
Intel's Loihi 2 embeds this principle in silicon: an idle neuron
consumes zero dynamic power.

## σ-innovation

v134 encodes the σ stream the same way:

| Primitive | What it is |
|-----------|------------|
| δ-spike encoder | `σ_spike ← |σ_now − σ_last| ≥ δ`; default δ = 0.10. A stable stream (σ drifts by < δ) emits no spikes and no downstream σ work. |
| Burst detector | O(1) ring buffer over the last `W` tokens (default 5). If it contains ≥ `K` spikes (default 3) the verdict is **BURST** → abstain. A single spike is **SPIKE** — emit but mark. |
| Lava-compatible JSON export | Frozen schema `{process, port, delta, burst_window, burst_count, events:[{t,v}…]}` — the shape Intel's Lava runtime consumes for a spike-port. v134.1 hands the stream to a Loihi 2 dev-kit; v134.0 simulates it natively. |

`burst-over-spike` is the LLM analogue of a **reactive spike train**:
one spike is information, a sustained burst is instability. v134
is the first σ-layer to make that distinction explicit in the gate.

## Contract

| Function | Role |
|----------|------|
| `cos_v134_feed(st, cfg, σ_now) → verdict` | feed one token; returns STABLE / SPIKE / BURST |
| `cos_v134_stable_ratio(st)` | fraction of tokens that did no σ-work |
| `cos_v134_to_json(st, …)` | status JSON for `X-COS-Sigma-Spikes` and `/v1/meta/spike` |
| `cos_v134_export_lava(st, cfg, spikes, n, …)` | Lava spike-stream serialisation |

## Merge-gate measurements

* `check-v134` asserts `stable_ratio ≥ 0.60` on a 70/30 synthetic
  stream — the "save 70% of σ-work on stable tokens" claim. The
  demo stream measures **0.7000**.
* A constructed 3-spike/5-token window is detected as **BURST**.
* The Lava export contains the frozen keys.

## v134.0 vs v134.1

| | Shipped (v134.0) | Follow-up (v134.1) |
|---|---|---|
| Spike encoder + burst | ✅ | — |
| Lava-format export | ✅ (text) | Hand off to a Loihi 2 dev-kit |
| v101 bridge integration | — | `σ_spike_check()` replaces `σ_aggregate()` when `COS_V134_SPIKE=1` |
| v106 HTTP header | — | `X-COS-Sigma-Spikes: 0 / 3` |
| v108 UI | — | Pulse-train visualisation instead of σ-bars |

## Honest claims

v134.0 is a **simulation** of neuromorphic σ on a conventional CPU.
It preserves the event-driven contract (no spike ⇒ no work) so the
Loihi 2 follow-up is a drop-in replacement, but the current binary
does not run on neuromorphic silicon. The "0 W on a flat stream"
claim refers to downstream σ work saved, not to host package power.
