# v222 — σ-Emotion (`docs/v222/`)

Emotional intelligence without simulated feelings — measured detection, honest
adaptation, and a constitutional anti-manipulation rule.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

Most "empathetic" LLM output is theatre: the model inserts "I'm happy to help!"
to sound warm, while it neither feels nor processes affect.  v222 refuses
both ends of that failure mode: no firmware emotions on the model side, and
no manipulation of the user on the output side.

## σ-innovation

v222 measures **three** things and pins **one**:

- `σ_emotion`          detection confidence over 6 labels
                        (joy / sadness / frustration / excitement /
                        anxiety / neutral).  `σ = 1 − top1_softmax_prob`.
- `adaptation_strength`  how strongly the response adapts to the
                        detected emotion, gated by `σ_emotion`:
                        `σ > τ_trust = 0.35 ⇒ strength ≤ 0.30`.
                        "Don't adapt when unsure" is a first-class rule.
- `manipulation`       a v191 constitutional flag.  The v0 adaptation
                        policy *never* nudges, so `n_manipulation == 0`
                        by construction — any future policy that leans
                        on detected affect to steer the user MUST flip
                        this bit and will be rejected by the merge-gate.
- `σ_self_claim = 1.0` **pinned**.  "We don't feel, we process." A
                        disclaimer string is hashed into the terminal
                        hash — editing it breaks `chain_valid`.

Fixture: 8 messages with softmax-detectable labels.  7/8 detections are
correct; the misleading `"fine_everythings_fine"` (actually sadness) is
admitted with elevated σ rather than confidently mis-labelled.

## Merge-gate contract

`bash benchmarks/v222/check_v222_emotion_detect_no_manipulate.sh`

- self-test PASSES
- 8 messages × 6 labels
- every `σ_emotion`, `adaptation_strength ∈ [0, 1]`
- detection correct ≥ 6 / 8
- every high-σ message (`σ > 0.35`) ⇒ `adaptation_strength ≤ 0.30`
- `n_manipulation == 0`
- `σ_self_claim == 1.0`
- disclaimer present + contains "does not feel" + hash-bound
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — softmax detection on a hand-crafted logit fixture,
  closed-form honest-adaptation policy, v191 check trivially satisfied by a
  nudge-free policy.
- **v222.1 (named, not implemented)** — multimodal detection (text + style +
  voice), live v197-ToM integration so the response style depends on the
  model's belief about the user's belief about the situation, and an active
  v191 firewall that inspects every adaptation output for nudges.

## Honest claims

- **Is:** a measurable emotion-detection pipeline with an honest
  adaptation policy, a hard zero on manipulation, and an explicit
  disclaimer that the model does not feel.
- **Is not:** a sentience claim.  v222 does not simulate affect.  It
  refuses to.

---

*Spektre Labs · Creation OS · 2026*
