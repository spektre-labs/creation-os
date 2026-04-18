# v182 σ-Privacy — adaptive differential privacy by σ

## Problem

Stock DP picks a single ε and applies uniform noise. On
confident queries that overpays privacy (and destroys
utility); on uncertain queries it under-protects (because the
data is inherently fuzzy and deserves *more* cover). Memory
and audit rows also tend to leak plaintext unless a discipline
is enforced from the bottom up.

## σ-innovation

v182 layers four disciplines:

1. **Input hashing.** Every ingested prompt and response is
   immediately SHA-256 hashed. The row struct carries no
   plaintext field at all, so serialization *cannot* leak
   cleartext.
2. **Privacy tiers.** Every row tags itself `public`,
   `private`, or `ephemeral`. Ephemeral rows are purged at
   end-of-session; private rows never leave this node.
3. **σ-adaptive DP noise.** `noise_std(σ) = base + k·σ`, so
   low-σ rows receive *less* noise than the fixed-ε baseline
   (utility wins) and high-σ rows receive *more* (privacy
   wins). Formally:
   - `adaptive_err_low < fixed_err_low`  (utility)
   - `ε_effective_high < fixed_ε`        (privacy)
   σ-DP is Pareto-dominant over fixed-ε on the mixed σ bank.
4. **Right-to-erasure.** `--forget <session>` deletes memory
   rows for that session, marks the audit-style copy
   `forgotten`, and shrinks row count. The plaintext
   invariant still holds post-forget.

## Merge-gate

`make check-v182` runs
`benchmarks/v182/check_v182_privacy_dp_adaptive.sh` and
verifies:

- self-test (plaintext invariant, all three tiers present,
  σ-adaptive noise monotone, utility wins on low-σ, privacy
  wins on high-σ, forget reduces rows and marks ≥ 1,
  end-of-session drops ephemeral rows, deterministic JSON)
- `mean_noise_high > mean_noise_low`
- `mean_err_adaptive_low < mean_err_fixed_low`
- `epsilon_effective_high < fixed_epsilon`
- `--forget 2026-04-18`: row count drops, `marked ≥ 1`
- summary byte-deterministic

## v182.0 (shipped) vs v182.1 (named)

| | v182.0 | v182.1 |
|-|-|-|
| Memory hook | in-process fixture | v115 memory rows + AES-GCM at rest |
| Unlearn | in-process flag | v129 federated unlearn broadcast |
| DP mechanism | Gaussian with σ-std | per-query accountant + v129 accountant |
| Forget verifier | row count diff | zk-proof for external right-to-forget |

## Honest claims

- **Is:** a deterministic, offline demonstration that prompts
  and responses are hashed at ingest, tiers are enforced on
  every row, σ-adaptive Gaussian noise is Pareto-better than
  fixed-ε on the synthetic bank, and session-level forget
  shrinks the row set without breaking the plaintext
  invariant.
- **Is not:** a live privacy layer over v115 or v129. No
  AES-GCM, no federated unlearn broadcast, no zk-proof
  verifier. Those ship in v182.1.
