# σ-gate v5 (lab): multi-dataset mix + semantic labels + PRISM prompts

This directory is an **optional research harness** (not merge-gate). It wires:

- Multi-source JSON (`prepare_multi_dataset.py`) — diversity over raw volume (motivation from UTH-style work; not a reproduction of [arXiv:2407.08582](https://arxiv.org/abs/2407.08582)).
- Greedy GPT-2 generations with **PRISM-style** suffixes (motivation from PRISM; not a reproduction of [arXiv:2411.04847](https://arxiv.org/abs/2411.04847)).
- **MiniLM cosine** correctness labels (same encoder family as many truth probes).
- Leave-one-source-out LSD retrains via existing `benchmarks/sigma_gate_lsd/adapt_lsd.py`.

Upstream reference implementation (UTH): [hkust-nlp/Universal_Truthfulness_Hyperplane](https://github.com/hkust-nlp/Universal_Truthfulness_Hyperplane) — optional clone under `external/uth/` for human reading; this harness does not import it.

**SpikeScore-style training-free spikes** (arXiv:2601.19245) are **not** implemented here; only the three tracks above (multi-dataset LSD, semantic labels, PRISM prompts).

## Dependencies

From repo root, with `benchmarks/sigma_gate_lsd/.venv` activated:

```bash
pip install -q datasets sentence-transformers
```

## Run

```bash
bash run_v5.sh
# or
python3 benchmarks/sigma_gate_v5/run_v5_pipeline.py
```

See `docs/CLAIM_DISCIPLINE.md` before publishing any AUROC headline.
