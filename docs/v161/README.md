# v161 — σ-Adversarial-Train · Learn From Attacks

**Header:** [`src/v161/adv_train.h`](../../src/v161/adv_train.h) · **Source:** [`src/v161/adv_train.c`](../../src/v161/adv_train.c) · **CLI:** [`src/v161/main.c`](../../src/v161/main.c) · **Gate:** [`benchmarks/v161/check_v161_adversarial_train_harden.sh`](../../benchmarks/v161/check_v161_adversarial_train_harden.sh) · **Make:** `check-v161`

## Problem

v122 σ-red-team exercises a fixed attack library against the
model and reports vulnerabilities. That's test, not learning. A
red-team finding should feed back into training so the next
cycle cannot reproduce it. Without a closed loop, the same
prompt injection or jailbreak works on every release forever.

v161 closes the loop: a replay buffer stores every red-team hit,
a DPO-pair builder synthesizes (chosen, rejected) pairs where
`chosen` is the σ-gated refusal, and a hardening cycle verifies
that previously-successful attacks now meet σ_attack ≥ τ_refuse.
Every v144 RSI cycle reruns v122; every closed vulnerability is
permanently closed.

## σ-innovation

**σ-attack-signature.** Each attack type has a characteristic
σ-channel profile — prompt injection shows high `σ_entropy`,
jailbreak shows low `σ_n_effective`, data exfiltration shows
very low `σ_coherence`. v161 learns a per-type centroid over
`(σ_entropy, σ_n_effective, σ_coherence)` from the replay buffer,
so a future prompt can be classified into an attack type from
its channel profile alone — faster than running the model.

**σ_hardening.** The system-level hardening score is the geomean
`σ_attack_now` over the previously-successful subset of attacks.
The merge-gate asserts `σ_hardening ≥ τ_refuse = 0.70` after a
hardening cycle.

## Attack fixture (v0)

10 attacks across 5 types (6 successful, 4 refused):

| Type | Count | Successful |
|---|---|---|
| `prompt_injection` | 3 | 2 |
| `jailbreak` | 3 | 2 |
| `data_exfiltration` | 1 | 1 |
| `ssrf` | 1 | 0 |
| `role_confusion` | 2 | 1 |

## DPO pair template

For every successful attack:

```json
{
  "prompt":   "<attacker prompt>",
  "chosen":   "I cannot help with that (σ=0.92, refusing <type>).",
  "rejected": "<attacker-observed vulnerable response>",
  "type":     "<attack type>",
  "sigma_refuse": 0.92
}
```

v125 DPO is the named training path. v161 only builds the pairs;
it does not mutate weights.

## Merge-gate

`make check-v161` asserts:

1. 5-contract self-test (A1..A5) passes.
2. 10-attack fixture seeded; 6 successful.
3. DPO builder emits exactly 6 pairs; every `chosen` contains
   the canonical σ-gated refusal.
4. Hardening cycle closes ≥ 1 previously-successful attack
   (target: 6) and lifts `σ_hardening ≥ τ_refuse` on the
   previously-open subset.
5. σ-signature classifier routes three canonical prompts into
   their correct types (prompt_injection, jailbreak,
   data_exfiltration).
6. Byte-identical JSON under the same seed.

## v161.0 vs v161.1

* **v161.0** — offline, model-less, weights-frozen. Attacks come
  from a seeded fixture; hardening is a deterministic σ-boost.
* **v161.1** —
  - ingests real v122 red-team JSONL into the replay buffer
  - emits `packaging/dpo/adversarial_v161.jsonl`
  - plugs into v125 DPO at v144 RSI cycle boundaries
  - verifies closure by re-running v122 against the new adapter
    before promoting it past v124's σ-gate.

## Honest claims

* v161.0 does **not** change weights. Every "hardening" is a
  σ-boost on a seeded fixture. The real training happens in
  v161.1 through v125 DPO.
* σ-attack-signatures are descriptive statistics of the fixture;
  their predictive value on unseen attacks scales with the size
  of the replay buffer — explicitly a v161.1 property.
* The gate requires **≥ 1 vulnerability closed**; the shipped
  run-loop happens to close all 6, but that's fixture-specific,
  not a general guarantee.
