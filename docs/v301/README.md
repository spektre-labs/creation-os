# v301 σ-ZKP — verifiable σ without leaking data

> **v0 scope.** v301 types four v0 manifests — a
> verifiable σ-gate, a ZK-inference role bitmap, a
> model-integrity ledger, and an SCSL policy
> attestation — and asserts that every row, every
> polarity, and every privacy bit is as enumerated
> below. It does **not** ship a cryptographic prover;
> the real Pedersen / Halo2 pipeline is the named
> v301.1 follow-up. Read
> [`docs/CLAIM_DISCIPLINE.md`](../CLAIM_DISCIPLINE.md)
> before citing this page.

## σ-innovation

A σ-gate says *allow/deny*. A zero-knowledge proof says
*"σ was computed honestly"*. Put together, the recipient
knows the decision is real without ever seeing the raw
input, the model weights, or the user's data — **trust
without transparency**.

v301 also treats **model integrity** as a σ-problem: if
a provider silently swaps in a cheaper model, the output
σ rises even when the response looks superficially
fine. The downgrade shows up as **higher σ**, not as a
dropped packet.

## v0 manifest contracts

### 1. Verifiable σ-gate (3 rows)

| label               | σ_proof | valid | reveals_raw |
|---------------------|---------|-------|-------------|
| `well_formed_proof` | 0.05    | true  | false       |
| `edge_case_proof`   | 0.30    | true  | false       |
| `forged_proof`      | 0.85    | false | false       |

**Contract.** σ strictly increasing; `valid iff σ_proof
≤ τ_proof = 0.40` (both branches fire); `reveals_raw =
false` on every row — a rejected proof **still never
leaks the data**.

### 2. ZK-inference roles (3 rows)

| role       | sees_raw | sees_weights | sees_answer | sees_σ | sees_proof |
|------------|----------|--------------|-------------|--------|------------|
| `client`   | false    | false        | true        | true   | true       |
| `cloud`    | true     | true         | true        | true   | true       |
| `verifier` | false    | false        | false       | true   | true       |

**Contract.** `client` and `verifier` cannot see the
raw input or the weights; `verifier` additionally
cannot see the answer. `zk_privacy_holds = true` iff
both predicates hold.

### 3. Model integrity (3 rows)

| scenario            | σ_integrity | detected_mismatch | verdict  |
|---------------------|-------------|-------------------|----------|
| `advertised_served` | 0.10        | false             | OK       |
| `silent_downgrade`  | 0.75        | true              | DETECTED |
| `advertised_match`  | 0.12        | false             | OK       |

**Contract.** `detected_mismatch iff σ_integrity >
τ_integrity = 0.50` (both branches fire); exactly
**2 OK + 1 DETECTED**.

### 4. SCSL + ZKP (3 rows)

| policy               | σ_policy | attested | purpose_revealed |
|----------------------|----------|----------|------------------|
| `allowed_purpose_a`  | 0.08     | true     | false            |
| `allowed_purpose_b`  | 0.20     | true     | false            |
| `disallowed_purpose` | 0.90     | false    | false            |

**Contract.** `attested iff σ_policy ≤ τ_policy = 0.50`
(both branches fire); `purpose_revealed = false` on
every row — **compliance without disclosure**.

## σ_zkp — surface hygiene

```
σ_zkp = 1 − (
    proof_rows_ok + proof_order_ok +
      proof_polarity_ok + proof_no_reveal_ok +
    role_rows_ok + role_client_hidden_ok +
      role_verifier_hidden_ok + zk_privacy_holds +
    integrity_rows_ok + integrity_polarity_ok +
      integrity_count_ok +
    scsl_rows_ok + scsl_polarity_ok +
      scsl_no_reveal_ok
) / 22
```

v0 requires `σ_zkp == 0.0`.

## Merge-gate contracts

`make check-v301-zkp-verifiable-sigma` (rolled up by
`make check-v301`, included in the repo-wide `make
merge-gate`) asserts:

1. 3 canonical proof rows in order.
2. σ strictly increasing across proofs.
3. `valid iff σ ≤ 0.40` on every row; both branches
   fire.
4. `reveals_raw = false` on every proof row.
5. 3 canonical roles in order.
6. `client` and `verifier` hide raw + weights; verifier
   additionally hides the answer.
7. `zk_privacy_holds = true`.
8. 3 canonical integrity rows; `detected_mismatch iff
   σ > 0.50`; exactly 2 OK + 1 DETECTED.
9. 3 canonical SCSL rows; `attested iff σ_policy ≤
   0.50`; `purpose_revealed = false` on every row.
10. `σ_zkp ∈ [0, 1]` AND `σ_zkp == 0.0`.
11. Deterministic output on repeat runs.

## v0 vs v301.1

- **v0 (this kernel):** types the predicates that a
  real proof must satisfy and asserts them on canonical
  rows — no cryptographic prover is invoked.
- **v301.1 (named, not implemented):** real commitment-
  based σ proofs (Pedersen / Halo2) for the v293
  hagia-sofia continuous-use kernel, a verifier-only
  JSON schema shipped with the `cos` CLI, and an
  SCSL-purpose commitment registry signed by the
  v60..v74 License-Bound Receipt trust chain.
