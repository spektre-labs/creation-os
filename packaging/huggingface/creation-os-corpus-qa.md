---
license: cc-by-4.0
language:
- en
tags:
- sigma
- knowledge-distillation
- qa
- creation-os
- corpus
size_categories:
- 100<n<1K
task_categories:
- question-answering
- text-generation
---

# Creation OS Corpus QA (v152)

Baked QA dataset for σ-knowledge-distillation. v152.0 ships a
**16-paper Spektre Labs corpus fixture** (topic-coverage bitmask
across 8 foundational σ-topics: `K_kernel`, `sigma_gate`,
`one_equals_one`, `hypervec`, `corpus_policy`, `proconductor`,
`iron_combo`, `silicon_tile`) and synthesizes **200 QA pairs** —
160 covered + 40 non-covered — with a deterministic baseline σ
distribution that drops under a simulated SFT step by the
contract-bound amount:

| K-contract | Assertion |
|---|---|
| **K1** | mean σ drop on covered QA ≥ 15 % |
| **K2** | every covered QA's σ drops by ≥ 10 % |
| **K3** | non-covered QA σ drift ≤ 1 % |
| **K4** | σ-drop is monotone non-decreasing in topic coverage |

## v152.0 vs v152.1

- **v152.0** (this card): baked 16-paper fixture + deterministic
  QA + simulated σ-drop. No tokenizer. No network. No weights.
- **v152.1**: clone [`spektre-labs/corpus`](https://github.com/spektre-labs/corpus)
  (CC-BY-4.0, ~80 papers with Zenodo DOIs), parse `.tex`/`.md`/`.pdf`
  into structured claims + formal statements + K(t) formulas + σ
  definitions + empirical tallies, run the real MLX LoRA SFT on
  BitNet-b1.58-2B, and archive the resulting adapter to
  `models/v152/corpus_knowledge.safetensors` + upload here.

## Reproduction

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os
make check-v152
./creation_os_v152_distill --distill --seed 152
```

Expected fields in the report JSON: `mean_baseline_all`,
`mean_post_all`, `mean_baseline_covered`, `mean_post_covered`,
`mean_baseline_non_covered`, `mean_post_non_covered`,
`sigma_drop_pct_covered`, `sigma_delta_non_covered`.

## Citation

```bibtex
@misc{creation_os_corpus_qa_v152,
  title  = {Creation OS {{Corpus QA}} v152},
  author = {Rainio, Lauri Elias and {Spektre Labs}},
  year   = {2026},
  url    = {https://github.com/spektre-labs/creation-os}
}
```
