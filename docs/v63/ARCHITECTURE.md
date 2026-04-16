# v63 σ-Cipher — architecture

## Wire map

```
              ┌───────────────────────────────────────────┐
              │                 cos  (CLI)                │
              │   cos sigma / cos seal / cos unseal       │
              └───────────────┬───────────────────────────┘
                              │
   ┌──────────────────────────┴──────────────────────────────┐
   │                                                         │
   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
   │  │ v60 σ-Shield │  │ v61 Σ-Citadel│  │ v62 Fabric   │   │
   │  │  capability  │  │ BLP + Biba + │  │ latent-CoT + │   │
   │  │  + σ-gate    │  │ attestation  │  │ EBT + HRM +  │   │
   │  │              │  │ quote256     │  │ NSA + MTP +  │   │
   │  │              │  │              │  │ ARKV         │   │
   │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘   │
   │         │                 │                 │           │
   │         ▼                 ▼                 ▼           │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │        v63 σ-Cipher  (end-to-end fabric)          │  │
   │  │                                                   │  │
   │  │  BLAKE2b-256 ─┬─► HKDF-BLAKE2b ─┬─► session_key   │  │
   │  │               │                 │                 │  │
   │  │  ChaCha20 ────┤   X25519 ────┐  │                 │  │
   │  │               │              │  │                 │  │
   │  │  Poly1305 ────┴──► AEAD ─────┴──┴──► seal/open    │  │
   │  │                                                   │  │
   │  │  attestation-bound envelope:                      │  │
   │  │    key = HKDF(v61_quote256, nonce, ctx)           │  │
   │  │                                                   │  │
   │  │  forward-secret symmetric ratchet                 │  │
   │  │  IK-like session handshake (BLAKE2b chaining key) │  │
   │  │  PQ-hybrid slot (ML-KEM-768, liboqs opt-in)       │  │
   │  └───────────────────────────────────────────────────┘  │
   │                       │                                 │
   │                       ▼                                 │
   │     composed 4-bit decision (branchless AND):           │
   │     allow = v60_ok & v61_ok & v62_ok & v63_ok           │
   └─────────────────────────────────────────────────────────┘
```

One emission from v62 → one attestation quote from v61 → one sealed
envelope from v63 → one 4-bit composed decision checked by
`cos_v63_compose_decision`.  No "OR" path.  No scalar fallback.

## File layout

```
src/v63/cipher.h              public ABI (≈ 300 lines)
src/v63/cipher.c              implementation (≈ 950 lines)
src/v63/creation_os_v63.c     self-test + microbench driver (≈ 550 lines)
scripts/v63/microbench.sh     AEAD / BLAKE2b / X25519 / seal throughput
docs/v63/THE_CIPHER.md        one-page articulation
docs/v63/ARCHITECTURE.md      this file
docs/v63/POSITIONING.md       comparison against Chutes / Tinfoil / Signal / Voidly
docs/v63/paper_draft.md       full write-up
```

## M4 hardware-discipline checklist

All nine Creation OS invariants apply:

| # | Invariant                                           | Where in v63                                   |
|---|-----------------------------------------------------|------------------------------------------------|
| 1 | mmap, never fread                                   | callers hand in already-mapped byte buffers    |
| 2 | 64-byte aligned allocations                         | no heap on hot path; state is stack-local and cache-aligned |
| 3 | Prefetch                                            | inherited from libc memcpy/memmove             |
| 4 | Branchless hot path                                 | tag verify: full-scan XOR-accumulate, no early exit |
| 5 | 4-way NEON accumulators                             | BLAKE2b G function maps to 64-bit ROR/ADD/XOR; ChaCha20 QR is ARX |
| 6 | Accelerate for per-layer repulsion                  | not applicable (cipher fabric)                 |
| 7 | Metal living weights                                | not applicable                                 |
| 8 | Heterogeneous dispatch                              | cipher runs on P-core where σ-gate lives       |
| 9 | `-march=armv9-a+sme`-ready (default build neutral)  | implementation compiles clean under both       |

Plus the 2026 crypto-specific rules:

- **Signed-shift UB eliminated** — `carry * ((int64_t)1 << N)` in place
  of `carry << N` across all ref10 field arithmetic, so UBSAN is clean.
- **Secure zeroize** — every intermediate key buffer and keystream
  block is wiped via a volatile-pointer loop that the compiler cannot
  optimise away.
- **Constant-time tag compare** — `cos_v63_ct_eq` walks `n` bytes
  unconditionally and OR-accumulates the XOR difference; no early
  exit.
- **X25519 constant-swap** — `v63_fe_cswap` masks with `-b`; every
  scalar bit is processed identically over 255 loop iterations.

## Microbench (reference run on M-series laptop)

```
ChaCha20-Poly1305 AEAD (4 KiB msgs):      ~564 MiB/s
BLAKE2b-256        (4 KiB msgs):         ~1267 MiB/s
X25519 scalar-mul  (base point):         ~12 k ops/s
seal(1 KiB, ctx="trace")                ~338 k ops/s
```

These numbers come from the **portable C path**, no NEON intrinsics,
no libsodium.  With `COS_V63_LIBSODIUM=1` (planned wire-up) the
numbers increase by roughly 3–5× on AEAD and 10× on X25519 because
libsodium uses the Apple-optimised AArch64 assembly paths.

## Threat-model tie-in

The threats v63 actually addresses:

| Threat                                       | Mitigation                                                          |
|----------------------------------------------|---------------------------------------------------------------------|
| Passive read of reasoning traces at rest     | sealed envelope (ChaCha20-Poly1305 AEAD, 128-bit authenticator)     |
| Passive read of in-flight handshakes         | IK-like X25519 handshake with BLAKE2b chaining key                  |
| Replay of a trace against a different host   | attestation-bound key (HKDF over v61 quote) — tag fails on wrong host|
| Forward secrecy of past keys after compromise| symmetric ratchet: KDF-chain, `secure_zero` on rotation             |
| Harvest-now-decrypt-later (quantum)          | PQ-hybrid slot: ML-KEM-768 + X25519 via `COS_V63_LIBOQS=1`          |
| Tampered ciphertext / AAD / tag              | AEAD rejects in constant time; `cos_v63_open` returns −3             |
| Timing side channel on tag verification      | `cos_v63_ct_eq` full-scan, no early exit                             |
| UB in field arithmetic (compiler freedom)    | UBSAN clean (`make ubsan-v63`)                                      |
| Use-after-free / OOB in cipher state         | ASAN clean (`make asan-v63`)                                        |

What v63 **does not** protect against:

- Runtime memory-dump attacks once plaintext is in-flight (mitigated by
  Secure Enclave / TDX / SEV-SNP, not by the cipher).
- Side channels from the CPU's speculative execution (mitigated by
  branch-protection via `HARDEN_CFLAGS`, not by the cipher).
- Adversarial σ-gate bypass (that is v60's job).
- Lattice violations (that is v61's job).
- EBT budget violations (that is v62's job).

v63 deals only with the **confidentiality and integrity of messages**
produced by the upstream three kernels.  The composed 4-bit decision
makes sure a message that any of the four kernels denies is never
emitted.

## Build matrix

```
make standalone-v63           default portable build (-O2 -march=native)
make standalone-v63-hardened  OpenSSF-2026 flags + ARM branch-protection
make test-v63                 144 self-tests
make check-v63                test-v63 + a PASS stamp for verify-agent
make asan-v63                 144 tests under AddressSanitizer
make ubsan-v63                144 tests under UndefinedBehaviorSanitizer
make microbench-v63           throughput dump for the four primitives
```

All targets are honest: if a test fails, the build fails; if a tool is
absent, the Makefile SKIPs; no silent downgrade.

## Where this fits in the 2026 taxonomy

The table in `docs/v63/POSITIONING.md` places v63 alongside Chutes,
Tinfoil, Signal, Voidly, reishi-handshake, and the QSC arXiv paper,
and shows which of the 2026 primitives each system ships and which it
leaves on the table.  The bottom line is that v63 is the only
open-source **local-AI-agent** row that ships every primitive as a
dependency-free C kernel, composes it with a capability kernel and a
reasoning fabric, and gates every emission on a composed 4-bit
branchless decision.
