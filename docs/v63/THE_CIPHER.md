# σ-Cipher — the end-to-end encrypted fabric (v63)

> One page.  What v63 is, in the language a reviewer needs.

## The problem

By 2026 the local-AI-agent market has finally admitted that the
**reasoning layer is not securable** and the **execution layer must be
sandboxed** — so every serious runtime ships a WASM sandbox, a
seL4-style capability kernel, and a TEE attestation quote.  What
everyone still leaves on the table is:

> _"What about the **messages** between those layers?"_

Reasoning traces flow from the transformer to the verifier, from the
verifier to the KV cache, from the KV cache to disk.  If an attacker
compromises a single buffer, they read the plaintext of the thought
process.  A sandbox that authorises an action without a **sealed,
attested trace** that justifies the action is a sandbox that can be
replayed.

σ-Cipher is the fourth kernel in the Creation OS stack and closes that
gap: **every message emitted by v62 is sealed to the v61 attestation
quote before it leaves the kernel**, using a dependency-free,
constant-time implementation of the 2026 encryption frontier.

## What v63 ships

Ten primitives, all implemented from scratch in C (zero mandatory
dependencies beyond libc), all tested against the official RFC vectors
on every build, all clean under ASAN + UBSAN:

| # | Primitive                        | Upstream                 | Test vector                     |
|---|----------------------------------|--------------------------|---------------------------------|
| 1 | BLAKE2b-256                      | RFC 7693                 | "" + "abc" + 1 MiB chunked      |
| 2 | HKDF-BLAKE2b                     | RFC 5869 (with BLAKE2b)  | extract+expand + input-sensitivity |
| 3 | ChaCha20 stream                  | RFC 8439 §2.3 / A.1      | A.1 test #1 + stream/block parity |
| 4 | Poly1305 MAC                     | RFC 8439 §2.5 / A.3      | §2.5.2 "Cryptographic Forum"    |
| 5 | ChaCha20-Poly1305 AEAD           | RFC 8439 §2.8 / A.5      | §2.8.2 "sunscreen" vector + 4 tamper variants |
| 6 | X25519 scalar multiplication     | RFC 7748 §5.2 / §6.1     | §5.2 test #1 + §6.1 Alice/Bob ECDH |
| 7 | Constant-time equality + zeroize | NIST SP 800-90A guidance | differ-at-first, differ-at-last, identical |
| 8 | Attestation-bound sealed envelope| Chutes 2026 / Tinfoil 2026 / QSC arXiv:2603.15668 | round-trip + 4 tamper variants |
| 9 | Forward-secret symmetric ratchet | Signal 2025 Triple Ratchet | chain-key advance + determinism |
| 10| IK-like session handshake        | Noise IK + reishi hybrid | initiator ↔ responder + tamper  |

Composition bit with v60 + v61 + v62 is a **branchless 4-bit AND**
(`cos_v63_compose_decision`).  No "OR" path.  No scalar fallback.  One
sealed envelope per emission, bound to the attestation quote of the
runtime that produced it.

## The attestation-bound envelope

This is the idea the 2026 frontier converged on — Chutes, Tinfoil and
the "Quantum-Secure-By-Construction" arXiv paper all land here:

```
  session_key = HKDF(
      salt = v61_attestation_quote256,
      ikm  = per-envelope nonce,
      info = "cos/v63/seal/" || caller context
  )
```

**Consequence:** a sealed envelope only opens on a host whose live
v61 quote matches the one the sender sealed against.  If the runtime
is replayed, tampered with, or migrated to a different attested state,
the AEAD tag fails in constant time and the plaintext never leaves the
kernel.  This is _deterministic key binding_ — no TEE required — and
composes with a real TEE (Secure Enclave, Intel TDX, AMD SEV-SNP) when
one is present, following the 2026 Tinfoil Containers pattern.

## What v63 is not

- Not a full TLS / Noise policy engine.  It ships the primitives plus a
  fixed IK-style handshake, not a generic negotiation surface.
- Not a replacement for libsodium / OpenSSL in cloud-scale production.
  v63 is sized for an auditable local-agent runtime.  A libsodium
  opt-in (`COS_V63_LIBSODIUM=1`) swaps the primitives for the battle-
  hardened ones when the host has the library.
- Not a post-quantum scheme **on its own**.  It exposes a hybrid slot
  (`COS_V63_LIBOQS=1`) that augments each handshake token with ML-KEM-
  768 encapsulation and mixes both shared secrets into the chaining
  key (the reishi-handshake 2026 / Signal SPQR pattern); without the
  liboqs toolchain the hybrid slot reports `SKIP` honestly.

## Commands the user actually types

```
cos sigma                    # σ-Shield + Σ-Citadel + Reasoning Fabric + σ-Cipher
cos seal <file> [context]    # attestation-bound E2E seal
cos unseal <file> [context]  # verify + open sealed envelope
```

and under the hood:

```
make check-v63               # 144 tests against official RFC vectors
make asan-v63                # 144 tests clean under AddressSanitizer
make ubsan-v63               # 144 tests clean under UndefinedBehaviorSanitizer
make standalone-v63-hardened # OpenSSF-2026 build flags + ARM branch-protection
make microbench-v63          # AEAD / BLAKE2b / X25519 / seal throughput
```

## Hardware discipline (Creation OS invariants)

- No dynamic allocation on the AEAD hot path.
- Branchless tag verification (constant-time scan, no early exit).
- `secure_zero` before returning from any function that holds an
  intermediate key or keystream block.
- 64-byte aligned cipher state where allocated (ChaCha20 block,
  Poly1305 accumulator, HKDF PRK buffer).
- X25519 Montgomery ladder: 255 iterations, fixed-time, constant-swap,
  every scalar bit processed identically.
- Signed-shift UB eliminated (`carry * (1LL << N)` in place of
  `carry << N` across the entire ref10 field arithmetic).

## Sources (the 2026 frontier this kernel distils)

- **RFC 7693** — BLAKE2 hash function
- **RFC 5869** — HKDF key-derivation function
- **RFC 8439** — ChaCha20-Poly1305 AEAD for IETF protocols
- **RFC 7748** — Curve25519 / X25519 scalar multiplication
- **Chutes 2026** — _End-to-End Encrypted AI Inference with Post-
  Quantum Cryptography_ (ML-KEM-768 client-side + TEE decrypt)
- **Tinfoil Containers 2026** — confidential-VM model attestation
  composed with Sigstore transparency logs
- **QSC / arXiv:2603.15668** — _Quantum-Secure-By-Construction: A
  Paradigm Shift For Post-Quantum Agentic Intelligence_
- **arXiv:2603.11006** — _Layered performance analysis of TLS 1.3
  hybrid key exchange_ (X25519+ML-KEM-768 ≈ neutral)
- **draft-ietf-sshm-mlkem-hybrid-kex-09** — SSH PQ/T hybrid KEX
- **reishi-handshake 2026** — Noise_IKpq_25519+MLKEM768_ChaChaPoly_BLAKE2s
- **Signal SPQR / Triple Ratchet 2025** — PQ forward-secret ratchet
- **Voidly Agent SDK 2026** — Double Ratchet + X3DH + ML-KEM-768 for
  AI-agent messaging

## The sentence

> _Every thought v62 emits is sealed to the v61 attestation quote of
> the runtime that produced it, under a ChaCha20-Poly1305 AEAD keyed
> by a BLAKE2b-256 HKDF over that quote and a per-envelope nonce, with
> forward-secret symmetric ratchet and an optional ML-KEM-768 + X25519
> hybrid handshake — in 900 lines of branchless C on M4 silicon._
