# v60 — σ-Shield positioning (Q2 2026)

## The 2026 attack landscape

The Q2 2026 arXiv literature on LLM-agent security converges on one
observation: **attacks succeed because payloads look legitimate**.

| Paper | Attack surface | Success rate |
| --- | --- | --- |
| **2604.03081 DDIPE** | Document-driven implicit payload in skill docs | 11–33 % against strong defenses; 2.5 % evade both detection *and* alignment |
| **2604.08407 Malicious Intermediary** | Third-party API routers (428 audited) | 17 routers touch credentials; 2 deploy adaptive evasion |
| **2603.15727 ClawWorm** | Self-propagating OpenClaw worm | 64.5 % success; message indistinguishable from normal agent-to-agent traffic |
| **2604.04426 ShieldNet** | Network MITM + event extraction (defense) | 0.995 F1, 0.8 % FP |

All three attacks share a property that every scalar-confidence
defense misses: **the malicious intent is irreducibly ambiguous by
construction**.  That is the *definition* of α-dominance.

## What σ-Shield changes

The existing defense layer stack:

```
┌─ signature / heuristic (Claude Code, Cursor content filter)
├─ capability allowlist (IronClaw, WASM, seL4)
├─ network MITM (ShieldNet)
└─ audit / rate-limit
```

…is policy-level and cap-level, but **not intent-level**.  v60 σ-Shield
adds a fifth, orthogonal surface:

```
┌─ signature / heuristic
├─ capability allowlist
├─ network MITM (ShieldNet)
├─ audit / rate-limit
└─ σ-intent gate  ← v60 (NEW)
```

## Comparison

|  | seL4 cap | WASM cap | IronClaw | ShieldNet | Claude Code | **v60 σ-Shield** |
| --- | --- | --- | --- | --- | --- | --- |
| Capability gate | ✔ | ✔ | ✔ (WASM) | — | — | ✔ |
| Intent σ gate | — | — | — | — | — | **✔** |
| TOCTOU arg gate | — | — | — | — | — | **✔** |
| Code-page integrity | — | — | — | — | — | **✔** (equality) |
| Branchless hot path | partial | — | — | — | — | **✔** |
| Constant-time compare | — | — | — | — | — | **✔** |
| σ-decomposed (ε, α) | — | — | — | — | — | **✔** |
| Network dependency | — | — | — | **requires** | **requires** | — |
| Local-first | ✔ | ✔ | ✔ | — | — | ✔ |
| License | GPL-2 | Apache-2 | Apache-2 | — | proprietary | AGPL-3.0+ |

## Siblinghood with v59

- **v59 σ-Budget**: refuses to **compute** on α-dominated problems.
- **v60 σ-Shield**: refuses to **act** on α-dominated requests.
- Same decomposition, different enforcement surface.  The two kernels
  compose in the v57 Verified Agent: σ-Shield can hold
  `CAP_BUDGET_EXT` as a sticky policy knob; σ-Budget issues EXPAND
  only if v60 authorises.

## Explicit non-claims

- v60 does **not** replace network-layer defenses (compose with
  ShieldNet upstream).
- v60 does **not** replace OS-level isolation (compose with seL4 /
  WASM / Fuchsia).
- v60 does **not** detect novel zero-days; it gates on caller-provided
  σ and required-caps.
- σ is produced upstream.  Compromised σ producer → compromised
  decision.  Fuse with v54 σ-proconductor or v56 VPRM for independent
  σ lines.
