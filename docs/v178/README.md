# v178 σ-Consensus — distributed truth without authority

## Problem

Mesh nodes (v128) and federated learners (v129) need a way to
agree on a claim — "is the reviewer bot trustworthy?" — when
some nodes may be faulty or adversarial.  No single node can
pick truth; but convergence of σ signals can.

## σ-innovation

One round of σ-Byzantine agreement:

1. **Per-claim σ.** Each node emits a σ for the claim.  Vote
   `TRUE ↔ σ < θ (0.30)`.
2. **Reputation-weighted quorum.** The vote requires
   `weight_true / total ≥ 2/3` to accept, symmetric to reject;
   otherwise the mesh **ABSTAINS**.
3. **Reputation update.**
   - Mature honest nodes start at 3.0, gain +0.25 per correct
     vote.
   - Young nodes start at 1.0 — they vote but cannot override
     mature majority.
   - Byzantine / suspect nodes start at 0.5 and lose 0.40 per
     incorrect vote (twice the penalty of honest nodes).
4. **σ-merkle tree.** Every resolved claim is leaf-hashed
   (SHA-256 of packed σ vector + decision) and folded into a
   root.  Any tampered leaf breaks `verify_merkle`.
5. **Resonance not consensus.** Truth is not picked — it is
   the convergence of σ above quorum.  The mesh chooses to
   abstain when the signal is split.

Under the baked fixture (5 nodes, 4 claims):

| decision | count |
|----------|------:|
| accept   | 2 |
| reject   | 1 |
| abstain  | 1 |

Reputations after the round: 3 mature → 3.75, young → 1.30,
byzantine → 0.00.  Sybil-resistance holds: the young node
voted 2/3 correctly but could not shift outcomes.

## Merge-gate

`make check-v178` runs
`benchmarks/v178/check_v178_consensus_byzantine.sh` and
verifies:

- self-test (including merkle tamper-detection)
- 5 nodes, 4 claims, decisions `accept=2 / reject=1 /
  abstain=1`
- byzantine reputation ≤ 0.1
- all mature honest nodes have reputation > 3.0
- young node has reputation > 1.0
- `merkle_root` is 64 hex chars, identical between JSON and
  `--root` CLI
- JSON + root byte-identical across runs

## v178.0 (shipped) vs v178.1 (named)

| | v178.0 | v178.1 |
|-|-|-|
| Transport | fixture | live v128 mesh socket |
| Signatures | none | per-node ed25519 signed messages |
| Merkle anchor | in-process | streaming anchor to v72 |
| Claim source | baked | real σ feed from kernels |

## Honest claims

- **Is:** a deterministic kernel proving Byzantine quorum,
  reputation weighting, sybil-resistance bounds and σ-
  merkle tamper-detection — all offline and verifiable.
- **Is not:** a live mesh.  No network I/O in v0; the
  contract is the decision rule, reputation math and
  hash-chain semantics.
