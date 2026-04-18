# v181 σ-Audit — compliance-grade σ-decision trail

## Problem

v160 telemetry records performance. v179 explains mechanism.
Neither alone is enough for a regulator, an insurer, or a
court. They want an **immutable**, **verifiable**,
**complete** log that lets an external auditor reconstruct
every decision without trusting the operator.

## σ-innovation

Every σ-decision (emit / abstain / revise) is appended as a
hash-chained entry carrying:

- `input_hash`  = SHA-256(prompt)
- `output_hash` = SHA-256(response)
- `sigma_product` + 8 σ-channels
- `decision` tag
- v179 mechanistic one-liner
- v180 steering vectors applied
- specialist id + adapter version
- `prev_hash` = canonical SHA-256 of previous entry
- `self_hash` = canonical SHA-256 of the current entry
- `sig`      = attestation tag (v181.0: keyed SHA-256 of
  `self_hash` under a session key; v181.1: ed25519)

The canonical hash covers every field except `self_hash` and
`sig`, so tampering **any** byte breaks either the chain or
the signature. The v181.0 fixture exercises a **1 000-entry
chain** end-to-end, plus two explicit tampering tests (middle
entry byte flip, signature byte flip).

Anomaly detection: if `mean σ` in the recent window exceeds
the prior window by ≥ 30 %, `anomaly_detected` fires. v181.1
auto-triggers v159 heal on such anomalies.

## Merge-gate

`make check-v181` runs
`benchmarks/v181/check_v181_audit_chain_verify.sh` and
verifies:

- self-test (chain verify on 1 000 entries, two tamper tests,
  anomaly detected with `rel_rise ≥ 30 %`, JSONL export count
  == chain count, summary determinism)
- summary `n_entries == 1000`, `anomaly_detected == true`,
  `anomaly_rel_rise_pct ≥ 30`
- `--export` JSONL contains exactly 1 000 lines with all
  required fields (`id, ts, sigma, decision, input_hash,
  output_hash, prev_hash, self_hash, sig`) all 64-char hex
- summary byte-deterministic across two runs

## v181.0 (shipped) vs v181.1 (named)

| | v181.0 | v181.1 |
|-|-|-|
| Attestation | keyed SHA-256 | ed25519 (libsodium) |
| Chain store | in-memory | append-only on disk |
| Report | JSONL export | `cos audit report --period YYYY-MM` (PDF) |
| Anomaly action | flag only | auto-trigger v159 heal |
| External verifier | `cos verify-forget` (v182) | standalone auditor CLI |

## Honest claims

- **Is:** a deterministic, offline hash-chained σ-decision log
  with SHA-256 canonicalization, tamper-evident entries,
  anomaly detection, and a JSONL export format suitable for
  external auditors.
- **Is not:** a production audit service. No on-disk append,
  no ed25519 signatures, no PDF report generation, no v159
  auto-trigger. Those ship in v181.1.
