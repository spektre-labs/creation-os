# Creation OS — Next steps (post σ-gate lab merge)

## Validated (ship-ready headline)

- [x] σ-gate v4 LSD probe: TruthfulQA holdout AUROC per `holdout_summary.json`
- [x] TriviaQA cross-domain smoke per `cross_domain_summary.json`
- [x] MCP stdio tools (`python/cos/mcp_sigma_server.py`, `scripts/cos_mcp_server.py`)
- [x] Makefile lab targets (`sigma-gate-smoke`, `sigma-mcp-smoke`, …)

## In progress

- [ ] HaluEval generative headline: freeze label protocol (`docs/CLAIM_DISCIPLINE.md`)
- [ ] Spectral σ: calibrate heuristic vs normalized Laplacian backend
- [ ] Unified σ: validate fusion vs LSD-only on holdout
- [ ] Cross-domain multi-benchmark script (`run_cross_domain_multi.py`) full 100× runs

## Planned

- [ ] Larger HF backbones for LSD (per-model probes; see scaling notes)
- [ ] Streaming σ (per-token hooks; scaffold in `sigma_streaming.py`)
- [ ] Pre-generation σ (HALT-style; `SigmaPrecheck` path)
- [ ] Paper / community post after repro bundle archived

## Known limitations

- Default probe targets GPT-2-scale hidden layout bundled in the pickle
- Short-form QA harnesses in-tree; no MMLU harness headline merge
- White-box trajectory access required for LSD scoring path
