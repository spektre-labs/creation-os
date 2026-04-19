# v279 — σ-JEPA (`docs/v279/`)

World model with σ = prediction error.  LeCun's JEPA
(2022) trains a predictor in a learned representation
space; the LeWorldModel paper (03/2026, "stable
end-to-end JEPA from pixels") stabilises the latent
with a two-term loss (prediction + regulariser).  The
prediction error is the σ of a world model: low σ
means the model knows what happens next and should
act; high σ means the model does not know and should
observe.

v279 does not run a JEPA trainer.  It types the σ-layer
on top of JEPA as four falsifiable manifests:
σ-gated ACT/OBSERVE, latent-entropy convergence with
σ, a two-term loss contract, and a citation manifest
pinning LeCun 2022 + LeWorldModel 2026/03 as
convergent evidence.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Prediction gate (exactly 4 fixtures, τ_predict = 0.30)

`decision = ACT iff σ_prediction ≤ τ_predict` else
`OBSERVE`.  Both branches fire.

| step | σ_prediction | decision |
|-----:|-------------:|:--------:|
|  0   | 0.07         | ACT      |
|  1   | 0.24         | ACT      |
|  2   | 0.46         | OBSERVE  |
|  3   | 0.82         | OBSERVE  |

### Latent convergence (exactly 3 checkpoints, canonical order)

`entropy_z` and `sigma_latent` both strictly decreasing
across checkpoints AND converge to each other per row
(|entropy_z − sigma_latent| ≤ 0.05): entropy
minimisation *is* σ minimisation.

| label   | entropy_z | sigma_latent |
|---------|----------:|-------------:|
| `early` | 0.72      | 0.70         |
| `mid`   | 0.41      | 0.44         |
| `late`  | 0.08      | 0.09         |

### Loss terms (exactly 2, canonical order)

LeWorldModel's two-term loss.  Both `enabled == true`,
weights in `(0, 1)`, and they sum to `1.0 ± 1e-3`.

| name          | enabled | weight |
|---------------|:-------:|-------:|
| `prediction`  | true    | 0.70   |
| `regularizer` | true    | 0.30   |

### Validation manifest (exactly 2 citations)

Citation contract, not an accuracy claim.  v279 does
not run a world model; v279.1 does.

| source                  | evidence                                 |
|-------------------------|------------------------------------------|
| `lecun_jepa_2022`       | Meta AI JEPA, representation-space pred. |
| `leworldmodel_2026_03`  | LeWorldModel 03/2026, stable e2e JEPA    |

### σ_jepa

```
σ_jepa = 1 − (predict_rows_ok + predict_both_ok +
              latent_rows_ok +
              latent_monotone_entropy_ok +
              latent_monotone_sigma_ok +
              latent_converge_ok +
              loss_rows_ok + loss_distinct_ok +
              loss_weights_ok +
              validation_rows_ok +
              validation_distinct_ok) /
             (4 + 1 + 3 + 1 + 1 + 1 + 2 + 1 + 1 + 2 + 1)
```

v0 requires `σ_jepa == 0.0`.

## Merge-gate contract

`bash benchmarks/v279/check_v279_jepa_world_model_sigma.sh`

- self-test PASSES
- 4 prediction rows; ACT iff σ_prediction ≤ 0.30; both
  branches
- 3 latent rows canonical; entropy_z and sigma_latent
  both strictly decreasing; per-row convergence
  |entropy_z − sigma_latent| ≤ 0.05
- 2 loss terms canonical (prediction, regularizer),
  distinct, weights sum to 1.0
- 2 validation citations present and distinct
- `σ_jepa ∈ [0, 1]` AND `σ_jepa == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed prediction / latent /
  loss / validation manifest with FNV-1a chain.
- **v279.1 (named, not implemented)** — a
  LeWorldModel JEPA trainer wired into v262 with
  σ-gated action/observe routing, measured latent
  entropy against σ on a pixel world model, and a
  live prediction-error gate.

## Honest claims

- **Is:** a typed, falsifiable σ-JEPA manifest where
  prediction-gate ACT/OBSERVE, latent-entropy
  convergence with σ, the two-term loss contract, and
  a two-source validation citation are merge-gate
  predicates.
- **Is not:** a running JEPA trainer, nor a
  measurement of LeWorldModel accuracy or stability.
  v279.1 is where the manifest meets real
  representation-space prediction.

---

*Spektre Labs · Creation OS · 2026*
