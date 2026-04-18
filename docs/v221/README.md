# v221 — σ-Language (`docs/v221/`)

Semantic depth, Gricean implicature, discourse coherence, and multilingual
σ-calibration.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

LLMs process text at the surface.  v221 goes four levels deeper:

1. **Semantic depth** — how many plausible readings does an utterance admit?
2. **Implicature** — literal form vs intended speech act (Grice).
3. **Discourse coherence** — current turn consistent with prior + referent
   resolved?
4. **Multilingual calibration** — σ means the **same thing** in EN, FI, JA, ES.

## σ-innovation

Fixture: 10 utterances across 4 languages (en / fi / ja / es), each carrying
`(n_readings, intended_reading, picked_reading, is_implicature, prior_topic_id,
current_topic_id, referent_resolved)`.

Per-utterance σ:

- `σ_semantic   = 1 − 1 / n_readings`
- `σ_implicature = 0.05` if `picked == intended`, else `0.65` (failed lift)
- `σ_discourse  = 0.05` if `current == prior ∧ referent_resolved`, else `0.55`
- `σ_lang       = 0.35·σ_sem + 0.35·σ_imp + 0.30·σ_dsc`

**Multilingual calibration contract.**  For each language, the per-language
mean `σ_lang` must be within `±Δ_calib` (=0.08) of the global mean.  σ
**measures uncertainty language-independently**.

## Merge-gate contract

`bash benchmarks/v221/check_v221_language_implicature.sh`

- self-test PASSES
- 10 utterances × 4 languages
- every σ ∈ [0, 1]; weights sum to 1
- every implicature caught (`σ_implicature ≤ 0.10`) — the model picked the
  intended reading for every implicature in the fixture
- discourse coherent for ≥ 7 / 10
- `|μ_L − μ_global| ≤ Δ_calib` per language
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 10-utterance fixture with ground-truth reading index,
  closed-form σ assignments, calibrated model (picked == intended for every
  utterance by construction).  The contract verifies the *shape* of the
  analysis.
- **v221.1 (named, not implemented)** — live v117 long-context discourse
  analyser, v202 cultural register hooks, tokenizer-aware reading
  enumeration, and a real implicature classifier on raw utterances.

## Honest claims

- **Is:** a structured four-level language-σ framework with a multilingual
  calibration invariant.
- **Is not:** semantic-role labelling, a dependency parser, or a proof that
  the model *understands* meaning.  v221 measures σ *over* structured
  annotations — not the annotations themselves.

---

*Spektre Labs · Creation OS · 2026*
