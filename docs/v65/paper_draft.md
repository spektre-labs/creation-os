# v65 σ-Hypercortex — paper draft

## Abstract

We present **σ-Hypercortex (v65)**, a hyperdimensional neurosymbolic
kernel for local AI runtimes. Bipolar hypervectors at fixed
dimension `D = 16 384` bits are operated on by a small, closed set
of algebraic primitives — bind (XOR), threshold-majority bundle,
cyclic permute, and Q0.15 integer similarity — with a constant-time
cleanup memory, a closed-form analogy operator, and sequence memory
via position-permutation. A 9-opcode bytecode language (HVL)
provides a first-class ISA over these primitives with per-program
cost accounting measured in popcount-word units.

σ-Hypercortex composes with five upstream kernels — v60 σ-Shield
(capabilities), v61 Σ-Citadel (lattice), v62 Reasoning Fabric
(energy-based thoughts), v63 σ-Cipher (attested AEAD), and v64
σ-Intellect (agentic planner) — as a 6-bit branchless decision:
`allow = v60 & v61 & v62 & v63 & v64 & v65`. No reasoning emission
leaves the stack unless every kernel agrees.

The implementation is pure C11 with libc as its only runtime
dependency and zero floating-point on the hot path. On a single
M-series performance core, `bind` runs at **192 GB/s** (within a
factor of 2 of unified-memory peak), `Hamming` at **41 GB/s**, and
cleanup sweep over 1 024 prototypes at **10.5 M proto·cmps/s**. The
kernel is accompanied by **534 deterministic tests** that pass
under release, AddressSanitizer, UndefinedBehaviorSanitizer, and
OpenSSF 2026 hardened builds.

## 1. Problem

Large language models are stateless; their "memory" is prompt, their
"reasoning" is one-token-at-a-time autoregression, and the internal
representation they operate on is unverifiable. The 2026 frontier
has converged on the observation that **transformer self-attention
is formally a vector-symbolic (VSA) operation** — attention weights
perform soft unbinding, residual streams carry superpositions of
role-filler bindings, and the FFN blocks perform additive updates
that are functionally equivalent to bundling (AAAI 2026:
Attention-as-Binding). The substrate is already VSA; the question
is whether we *expose* it explicitly so it can be audited, budgeted,
and composed with a security kernel.

Existing HDC frameworks (TorchHD, NeuroHD, HDC-Library) run in
Python / PyTorch, use floating-point arithmetic, target the GPU as
an accelerator for what is fundamentally a popcount problem, and
do not compose with any capability / lattice / attestation kernel.
Persistent memory frameworks for LLM agents (OpenMem 2026, Mem0,
MemGPT, Zep) use float-embedding ANN retrieval — symbolic
structure is lost. The gap is a **systems-level integer HDC
substrate**, callable from C, composable with a security stack, and
fast enough to run per-token on commodity silicon.

## 2. Design

### 2.1 Representation

Hypervectors are **bipolar bit-packed** at `D = 16 384` bits:
`bit = 1 → +1`, `bit = 0 → −1`. This choice is deliberate:

- `D` is a power of two and a multiple of 64 (word size) and
  of 512 (cache line in bits), so no stride is wasted.
- `2 048 bytes = 32 × 64-byte cache lines` — each HV is its own
  natural prefetch block on M-series silicon.
- XOR is bipolar multiplication, which makes bind its own inverse
  and collapses unbind into bind.

### 2.2 Primitive operations

| op | formula | cost (one HV) |
|---|---|---|
| `bind`       | `d_i = a_i ⊗ b_i`            | 256 XOR words |
| `bundle`     | `d_i = thresh(Σ a^k_i, n/2)` | `n × D` tally inc + 256 threshold words |
| `permute`    | `d_i = a_{(i − k) mod D}`    | 256 word shifts |
| `similarity` | `(D − 2·H(a, b)) · (32768/D)` in Q0.15 | 256 popcount words |

Bundle uses an integer tally (int16 per bit position) and a
deterministic tie-breaker hypervector seeded from `n` so the
operation is fully reproducible and has no hidden RNG state.

### 2.3 Cleanup memory

A flat arena of labeled hypervectors. Query sweeps linearly with
branchless argmin update. The sweep **never exits early**, so
runtime is exactly `cap × (D / 64)` popcount words regardless of
match index — the timing channel is bounded by arena size, not by
secret state.

Insertion is append-only; capacity is caller-set at `_new` time
and overflow returns `-1` rather than reallocating. No allocation
on the hot path.

### 2.4 Record, analogy, sequence

- **Record**: `record = ⊕_i (role_i ⊗ filler_i)`; unbind is bind
  (XOR involution); capacity scales as `D / log |cleanup|` per
  HIS (arXiv:2603.13558).
- **Analogy**: `A : B :: C : D` closed-form as `D = A ⊗ B ⊗ C`,
  followed by cleanup lookup.
- **Sequence**: `seq = ⊕_i perm^i(item_i)`; decode at position `p`
  via `perm^{-p}` + cleanup.

### 2.5 HVL — HyperVector Language

A 9-opcode integer bytecode ISA:

```
HALT                 stop
LOAD  dst, arena_idx         regs[dst] ← arena[idx]
BIND  dst, a, b              regs[dst] ← regs[a] ⊗ regs[b]
BUNDLE dst, a, b; imm=c      regs[dst] ← majority(a, b, regs[c])
PERM  dst, a; imm=shift      regs[dst] ← perm(regs[a], shift)
LOOKUP _, a                  state.label ← cleanup(regs[a])
SIM   _, a, b                state.sim   ← sim(regs[a], regs[b])
CMPGE ; imm=threshold_q15    state.flag  ← state.sim ≥ imm
GATE                         state.v65_ok ← flag & (cost ≤ budget)
```

Each instruction is 8 bytes (op/dst/a/b + imm16 + pad). Cost is
tracked in popcount-word units (256 per HV-scale op). GATE writes
directly into the composed decision; if cost exceeds budget,
`v65_ok = 0` and the exec function returns `-2`.

This is the "new language surface" — every opcode maps 1:1 to an
integer vector primitive, every program has a hard integer cost
ceiling, and the gate result is the sixth lane of the 6-bit
composed decision.

### 2.6 Composed 6-bit decision

```c
cos_v65_decision_t
cos_v65_compose_decision(uint8_t v60, uint8_t v61, uint8_t v62,
                         uint8_t v63, uint8_t v64, uint8_t v65)
{
    cos_v65_decision_t d;
    d.v60_ok = v60 & 1u;  /* ... */  d.v65_ok = v65 & 1u;
    d.allow  = d.v60_ok & d.v61_ok & d.v62_ok &
               d.v63_ok & d.v64_ok & d.v65_ok;
    return d;
}
```

Six `uint8_t` lanes, branchless AND. Every failing lane is
inspectable for telemetry. No silent-degrade path.

## 3. Implementation discipline

- `D = 16 384` chosen for exact alignment to 32 × 64-byte cache
  lines.
- Hamming unrolled × 4 accumulators so M-series wide decode stays
  full.
- `__builtin_popcountll` lowers to AArch64 `cnt` + horizontal add.
- All arenas `aligned_alloc(64, ...)`; allocation happens only at
  `_new` calls.
- Ternary-select patterns (`better ? x : y`) lower to `csel` /
  `cmov` — branchless on the hot path.
- Zero FP anywhere in the library.

## 4. Tests

The self-test driver executes **534 deterministic assertions**:

- **Composition truth table** (64 rows × 7 checks = 448): every
  row of the 6-bit space verified end-to-end.
- **Primitives**: orthogonality variance of random HVs around D/2,
  bind self-inverse / commutative / associative, permute
  round-trip, permute preserving Hamming under joint shift, zero
  and copy.
- **Bundle + cleanup**: majority bundle closer to members than to
  unrelated HVs; cleanup exact-match sim = +32768; noisy recall
  above 0 with 512-bit flip; overflow correctly rejected.
- **Record / role-filler**: three-pair round-trip through cleanup
  memory; analogy closed-form identity
  `((A ⊗ B ⊗ C) ⊗ C ⊗ B) = A`; direct analogy helper.
- **Sequence**: 4-item sequence with `base_shift = 11`, decoding
  at each position and recovering via cleanup.
- **HVL**: end-to-end LOAD/BIND/SIM/CMPGE/GATE/LOOKUP/HALT flow;
  over-budget refusal returns `-2` with `v65_ok = 0`; malformed
  opcode returns `-1`; out-of-range register returns `-1`.

Tests pass under release, ASAN+UBSAN, UBSAN-no-recover, and the
OpenSSF 2026 hardened build.

## 5. Performance

Measured on a single M-series performance core at `-O2 -march=native`:

| op | throughput | memory bandwidth |
|---|---|---|
| Hamming (2 × HV) | 10.1 M ops/s | **41 GB/s** |
| Bind (3 × HV) | 31.2 M ops/s | **192 GB/s** |
| Cleanup (1024 protos) | 10 217 queries/s | **10.5 M proto·cmps/s** |
| HVL (7-op program) | 5.7 M programs/s | **40 M ops/s** |

`bind` at 192 GB/s is within a factor of 2 of unified-memory peak.
Integer HDC is a memory-bandwidth problem and v65 saturates the
bandwidth.

## 6. Composition with v57 Verified Agent

`make verify-agent` reports **14 PASS / 3 SKIP / 0 FAIL** with the
new `hyperdimensional_cortex` slot (owner `v65`, target
`check-v65`). `cos sigma` runs all six kernels sequentially and
reports the composed verdict; `cos decide 1 1 1 1 1 1` emits JSON
`{"allow":1,"reason":63,...}` in ~1 ms.

## 7. Limitations

- The integer tally in `bundle_majority` scales linearly with `n`;
  beyond `~2 000` superposed vectors the HIS recovery floor is
  violated. We document this as a capacity limit and reject
  `n > cap` at the caller.
- The HVL cost model treats all HV-scale ops as 256 popcount
  words; `BUNDLE` is 3× that for the 3-way majority. More precise
  per-op accounting is future work.
- Cross-modal integration (frozen-LLM embeddings → HV) is
  declared I-tier (synthetic input only) — the live adapter is
  not in v65 proper.
- SRAM/CAM acceleration paths per VaCoAl (arXiv:2604.11665) are
  P-tier (planned, compile-only).

## 8. Future work

- SME / AMX path for bundle tally (M4 matrix extension).
- Apple Neural Engine path for cleanup sweep (constant-time by
  construction on NE).
- HVL JIT compiler to native AArch64 (the bytecode is already
  straight-line and register-only; the JIT is table-driven).
- Hierarchical cleanup memory with two-tier index (LifeHD-style).
- Full public benchmark suite against TorchHD / OpenMem /
  Pinecone on analogy, multi-hop, and sequence-decode tasks.

## 9. References

- Kanerva, P. "Hyperdimensional computing: An introduction to
  computing in distributed representation with high-dimensional
  random vectors." *Cognitive Computation*, 2009.
- arXiv:2603.13558 — Holographic Invariant Storage.
- arXiv:2604.11665 — VaCoAl: deterministic HD reasoning with
  Galois-field algebra.
- arXiv:2511.08747 — Vector Symbolic Algebras for the Abstraction
  and Reasoning Corpus.
- arXiv:2509.25045 — Hyperdimensional Probe.
- arXiv:2403.04759 — LifeHD: Lifelong intelligence at the edge.
- OpenReview `wQDFkHOAZc` — Attention as Binding: a
  Vector-Symbolic Perspective on Transformer Reasoning (AAAI 2026).
- McMenemy, R. "OpenMem: Building a Persistent Neuro-Symbolic
  Memory Layer for LLM Agents with Hyperdimensional Computing."
  Medium, March 2026.
