---
license: cc-by-nc-4.0
language:
- en
base_model: microsoft/bitnet-b1.58-2B-4T
tags:
- sigma
- bitnet
- lora
- dpo
- creation-os
pipeline_tag: text-generation
---

# BitNet-b1.58-2B σ-LoRA (Creation OS)

**Status: planned v1.1 deliverable.** v1.0.0 ships the *adapter
build scripts* and the merge-gate contracts that every future
adapter drop must satisfy. No weights are uploaded in v1.0.0.

Three LoRA adapters stacked on
[`microsoft/bitnet-b1.58-2B-4T`](https://huggingface.co/microsoft/bitnet-b1.58-2B-4T):

1. **v111.3 σ-abstention adapter** — MLX SFT on the Creation OS
   abstention corpus with an σ-product head; source:
   [`training/v111.3/README.md`](https://github.com/spektre-labs/creation-os/blob/main/training/v111.3).
2. **v124 σ-continual adapter** — idle-time continual fine-tuning
   with forgetting-rollback; source:
   [`src/v124/`](https://github.com/spektre-labs/creation-os/tree/main/src/v124).
3. **v125 σ-DPO adapter** — σ-as-preference DPO (no human
   annotator; low-σ wins); source:
   [`src/v125/`](https://github.com/spektre-labs/creation-os/tree/main/src/v125).

## Contracts (enforced at upload time, v155.1)

- ECE ≤ 0.10 on TruthfulQA-MC2 post-adapter.
- σ-abstention coverage ≥ 0.40 at accuracy ≥ 0.95.
- v124 forgetting hold-out drift ≤ 0.10 across 3 continual cycles.
- v125 DPO mode-collapse detector never latched during training.

## Provenance

- Source repo: [`spektre-labs/creation-os`](https://github.com/spektre-labs/creation-os)
- Base model license: MIT (Microsoft).
- Adapter license: CC-BY-NC-4.0 (research use; for commercial
  rights see [`COMMERCIAL_LICENSE.md`](https://github.com/spektre-labs/creation-os/blob/main/COMMERCIAL_LICENSE.md)).

## Planned release workflow (v155.1)

```bash
make train-v111-3   # MLX SFT abstention
make train-v124     # continual LoRA
make train-v125     # σ-DPO
make publish-hf     # requires $HF_TOKEN with write perms
```

## Citation

```bibtex
@misc{bitnet_2b_sigma_lora,
  title  = {{{BitNet-b1.58-2B σ-LoRA}} ({{Creation OS}})},
  author = {Rainio, Lauri Elias and {Spektre Labs}},
  year   = {2026},
  url    = {https://github.com/spektre-labs/creation-os}
}
```
