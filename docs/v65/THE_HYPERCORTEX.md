# v65 σ-Hypercortex — the hyperdimensional neurosymbolic kernel

**One page. AGPL-3.0. Pure C11. Zero runtime dependencies. Zero floating-point on the hot path. 500+ deterministic tests.**

## The problem v65 solves

LLMs are stateless, non-compositional, and operate on a representation no
one can verify. Their "memory" is prompt. Their "reasoning" is
one-token-at-a-time autoregression. Their "planning" is the same. The
2026 frontier identified the fix: **put a neurosymbolic substrate under
the LLM** — a representation that is

- **compositional** (bind, bundle, permute are algebraic operations),
- **robust** (graceful degradation at 10–30 % bit noise),
- **persistent** (cleanup memory = long-term cortex the LLM never had),
- **verifiable** (every similarity is an integer comparison),
- **fast** (popcount-native, 40 GB/s per M-series core).

That substrate is **hyperdimensional computing / vector symbolic
architectures** (HDC/VSA). v65 ships it.

## What v65 ships

**Seven capabilities, one kernel, one header.**

1. **Bipolar hypervectors** at `D = 16 384 bits` (2 048 B = 32 × 64-byte
   cache lines). Bit-packed, naturally SIMD-aligned on M-series.
2. **VSA primitives** — `bind` (XOR, self-inverse), `bundle`
   (threshold-majority with integer tally + tie-breaker), `permute`
   (cyclic bit rotation), `similarity` in Q0.15 = `(D − 2·H)·(32768/D)`.
3. **Cleanup memory** — constant-time nearest-neighbour over labeled
   prototypes. Branchless argmin. Sweep is O(cap) regardless of which
   label matches, so similarity leakage is bounded by arena size, not
   by secret state.
4. **Record / role-filler** — `record = ⊕_i (role_i ⊗ filler_i)`;
   unbind = bind (XOR involution); round-trip recovery through
   cleanup.
5. **Analogy** — `A:B :: C:?` solved in closed form as `A ⊗ B ⊗ C`
   followed by cleanup. Zero gradient steps.
6. **Sequence memory** — `seq = ⊕_i perm^i(item_i)`; decode at
   position `p` via `perm^{-p}` + cleanup.
7. **HVL — HyperVector Language** — a 9-opcode integer bytecode ISA
   (`LOAD / BIND / BUNDLE / PERM / LOOKUP / SIM / CMPGE / GATE / HALT`)
   with per-instruction cost accounting in popcount-word units and an
   integrated GATE opcode that writes `v65_ok` directly into the
   composed 6-bit decision.

## The 6-bit composed decision

```
allow = v60_ok & v61_ok & v62_ok & v63_ok & v64_ok & v65_ok
```

- **v60 σ-Shield** — was the action allowed?
- **v61 Σ-Citadel** — does the data flow honour BLP+Biba+MLS?
- **v62 Reasoning Fabric** — is the EBT energy ≤ budget?
- **v63 σ-Cipher** — is the AEAD tag authentic and quote-bound?
- **v64 σ-Intellect** — does the agentic kernel authorise emit?
- **v65 σ-Hypercortex** — is the thought on-manifold in cleanup memory
  AND under the HVL cost budget?

Pure branchless AND of six `uint8_t` lanes. No policy engine. No JIT.
No silent-degrade path. Any failing lane is inspectable for telemetry.

## Hardware discipline (M4, AGENTS.md, .cursorrules)

- D chosen so **one HV = exactly 32 × 64-byte cache lines** — natural
  NEON alignment, no padding, no stride waste.
- `__builtin_popcountll` lowers to AArch64 **NEON `cnt` + horizontal
  add** — the hot loop is silicon-native.
- All arenas `aligned_alloc(64, …)`; never `malloc` on the read path.
- **Zero FP** anywhere: similarity, tallies, costs — all integer.
- Constant-time cleanup sweep: O(cap) regardless of match index.
- Branchless inner loops (`cmov` / `csel` on AArch64).

## Measured on an M-series performance core

| Op | Throughput | Notes |
|---|---|---|
| Hamming (2 × HV) | **10.1 M ops/s** ≈ **41 GB/s** | popcount-bound |
| Bind (XOR, 3 × HV) | **31.2 M ops/s** ≈ **192 GB/s** | unified-memory-bound |
| Cleanup (1024 protos) | **10.5 M proto·cmps/s** | constant-time sweep |
| HVL (7-op program) | **5.7 M programs/s** ≈ **40 M ops/s** | end-to-end bytecode |

> 192 GB/s bind throughput is within a factor of 2 of M-series peak
> unified-memory bandwidth. Integer popcount HDC is *not* a compute
> problem — it is a memory-bandwidth problem, and v65 saturates it.

## What v65 is not

- **Not** a language model. Zero gradient steps, zero weights. It sits
  under or beside an LLM and *verifies* its thoughts on an explicit
  manifold.
- **Not** a vector DB. Fixed-dimension bipolar HVs, popcount-native —
  not float-embedding ANN.
- **Not** tied to a transformer. Attention-as-Binding (AAAI 2026)
  says every transformer *already* runs a VSA internally — v65
  exposes the substrate so any model fits on top with zero retraining.

## Commands

```bash
make check-v65                          # 500+ deterministic tests
make asan-v65 ubsan-v65                 # same under ASAN + UBSAN
make standalone-v65-hardened            # OpenSSF 2026 + PIE + branch-protect
make microbench-v65                     # print the table above

cos hv                                  # self-test + microbench demo
cos sigma                               # six-kernel composed verdict
cos decide 1 1 1 1 1 1                  # {"allow":1,"reason":63,...}
```

## Sources (2026 frontier)

- **OpenMem** (McMenemy, 2026) — persistent neuro-symbolic memory layer
  for LLM agents.
- **VaCoAl** — arXiv:2604.11665 — deterministic HD reasoning with
  Galois-field algebra; reversible composition; 57-generation
  multi-hop over 470 k Wikidata edges; SRAM/CAM target.
- **Attention-as-Binding** — AAAI 2026 (OpenReview `wQDFkHOAZc`) —
  transformer self-attention ≡ VSA unbind + superposition.
- **VSA for ARC-AGI** — arXiv:2511.08747 — 94.5 % Sort-of-ARC via VSA
  program synthesis.
- **Holographic Invariant Storage (HIS)** — arXiv:2603.13558 —
  closed-form recovery-fidelity contracts for bipolar VSA.
- **Hyperdimensional Probe** — arXiv:2509.25045 — HDC to decode LLM
  representations across models.
- **HDFLIM** — HDC over frozen LLM + vision; symbolic
  bind/bundle/similarity; no model modification.
- **ConformalHDC** — HDC + conformal prediction for uncertainty
  quantification.
- **LifeHD** (arXiv:2403.04759) — on-device lifelong learning, 74.8 %
  clustering, 34.3× energy vs. NN baselines.

## The sentence

> v65 σ-Hypercortex gives the local AI runtime a permanent
> neurosymbolic cortex with a first-class bytecode ISA — integer
> popcount, 32 cache-lines per thought, 192 GB/s bind, 40 GB/s
> Hamming, cleanup memory with constant-time sweep, and a 6-bit
> branchless gate that refuses to let a thought escape unless every
> kernel above it agrees.

1 = 1.
