# Changelog

## v2.0 (2026-04-15)

- **Publish:** `make publish-github` runs `tools/publish_to_creation_os_github.sh` (`make check`, clone **spektre-labs/creation-os**, rsync, commit, push).
- **Docs / naming:** Creation OS–only messaging in README and ANALYSIS; [docs/publish_checklist_creation_os.md](docs/publish_checklist_creation_os.md), [docs/cursor_briefing_creation_os.md](docs/cursor_briefing_creation_os.md), [docs/cursor_integration_creation_os.md](docs/cursor_integration_creation_os.md); removed older multi-repository map and superseded publish doc names from this tree.
- Visuals: **`docs/assets/*.svg`** (architecture, BSC primitives, GEMM vs BSC bars, evidence ladder, Planes A–C) + **[docs/VISUAL_INDEX.md](docs/VISUAL_INDEX.md)**; README embeds figures + Mermaid evidence-flow.
- Docs: **[docs/HDC_VSA_ENGINEERING_SUPERIORITY.md](docs/HDC_VSA_ENGINEERING_SUPERIORITY.md)** — web-grounded literature map (Ma & Jiao 2022; Aygun et al. 2023; Springer AIR HDC review 2025; Yeung et al. Frontiers 2025; FAISS popcount PR) + safe vs unsafe claim table + deck paragraph; links from README / EXTERNAL / AGENTS.
- **Contributor / ops:** [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md), [AGENTS.md](AGENTS.md); [docs/DOC_INDEX.md](docs/DOC_INDEX.md); [docs/REPRO_BUNDLE_TEMPLATE.md](docs/REPRO_BUNDLE_TEMPLATE.md); `make help` / `make check`; GitHub Actions CI (`.github/workflows/ci.yml`).
- Docs: **[docs/EXTERNAL_EVIDENCE_AND_POSITIONING.md](docs/EXTERNAL_EVIDENCE_AND_POSITIONING.md)** — vetted external citations (Kanerva BSC/HDC, Schlegel–Neubert–Protzel VSA survey, EleutherAI `lm-evaluation-harness`); field consensus vs in-repo proofs.
- Docs: **[docs/CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md)** — evidence classes, forbidden baseline merges, falsifiers, reproducibility bundle; README adds reviewer-proof benchmark interpretation + **Publication-hard** section (cross-examination standard, not hype).
- 26 modules in single standalone file `creation_os_v2.c`
- Correlative encoding for Oracle (generalization)
- Iterative retraining (10 epochs)
- Cross-pattern correlation
- GEMM vs BSC benchmark: 32× memory, 192× fewer ops (proxy); wall-clock and trials/sec printed (`make bench` / §7)
- Noether conservation: σ = 0.000000 (genesis toy)
- Public tree: `core/`, `gda/`, `oracle/`, `physics/`, `tests/`, `Makefile`, AGPL `LICENSE`

## v1.1

- Added: Metacognition, Emotional Memory, Theory of Mind, Moral Geodesic, Consciousness Meter, Inner Speech, Attention Allocation, Epistemic Curiosity, Sleep/Wake, Causal Verification, Resilience, Meta Goals, Privacy, LSH Index, Quantum Decision, Arrow of Time, Distributed Consensus, Authentication

## v1.0

- Initial release: 8 modules (BSC Core, Mind, Oracle, Soul, Proconductor, JEPA, Benchmark, Genesis)
