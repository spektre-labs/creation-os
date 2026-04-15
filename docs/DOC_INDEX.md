# Documentation index — Creation OS (`creation-os`)

Single entry point for humans and automation. **All committed prose here is English.**

| Document | Audience | Purpose |
|----------|----------|---------|
| [README.md](../README.md) | Everyone | Problem, measured table, BSC primer, invariants, build, limitations, theory links, Living Kernel v6 |
| [LIVING_KERNEL_V6.md](LIVING_KERNEL_V6.md) | Integrators, thesis writers | What `creation_os_v6.c` does, evidence class, non-claims, M01–M18 map, `make check-v6` |
| [PARADIGM_SNAPSHOT_FOR_DRIVE_BY_READERS.md](PARADIGM_SNAPSHOT_FOR_DRIVE_BY_READERS.md) | General reader | Plain-language paradigm contrast + explicit non-claims |
| [LANGUAGE_POLICY.md](LANGUAGE_POLICY.md) | Everyone | English-only rule for committed repo |
| [COMMON_MISREADINGS.md](COMMON_MISREADINGS.md) | Reviewers, integrators | Frequent misinterpretations → one-line correction + canonical link (figures, benchmarks, allocation guidance) |
| [ROADMAP.md](../ROADMAP.md) | Maintainers | Shipped vs planned; links to discipline docs |
| [CHANGELOG.md](../CHANGELOG.md) | Release readers | Versioned user-visible changes |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | Contributors | Build, PR hygiene, license headers |
| [SECURITY.md](../SECURITY.md) | Reporters | Vulnerability reporting |
| [SECURITY_DEVELOPER_NOTES.md](SECURITY_DEVELOPER_NOTES.md) | Maintainers, reviewers | Abuse patterns (epistemic, supply chain, future RPC) |
| [MAINTAINERS.md](MAINTAINERS.md) | Merge rights | Publish to GitHub, language/claims gate, CI pointer |
| [AGENTS.md](../AGENTS.md) | Coding agents / Copilot | Evidence-class rules, scope |
| [RESEARCH_AND_THESIS_ARCHITECTURE.md](RESEARCH_AND_THESIS_ARCHITECTURE.md) | Dissertation, safety, systems reviewers | RQs, contributions C1–C6, methodology, threats, thesis chapter outline |
| [ADVERSARIAL_REVIEW_CHECKLIST.md](ADVERSARIAL_REVIEW_CHECKLIST.md) | Authors before submit | Hostile reviewer attacks ↔ in-repo answers |
| [MODULE_EVIDENCE_INDEX.md](MODULE_EVIDENCE_INDEX.md) | Thesis writers | §1–§26 → evidence class + artifact |
| [CITATION.bib](CITATION.bib) | LaTeX users | BibTeX entries + CFF cross-reference |
| [COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md](COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md) | Strategy, robotics, safety | Public-theme industry challenges → receipt geometry; explicit non-claims |
| [GLOSSARY.md](GLOSSARY.md) | Cross-discipline readers | σ, BSC, HV, Planes, evidence class — quick definitions |
| [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md) | Anyone citing §7 / `make bench` | Fixed D, W, trials; op proxy; repro archive; falsifiers |
| [NATIVE_COHERENCE_NEON.md](NATIVE_COHERENCE_NEON.md) | Edge / robotics integrators | AArch64 NEON Hamming gate; `make bench-coherence` |
| [HYPERVECTOR_PARLIAMENT_AND_RETRIEVAL.md](HYPERVECTOR_PARLIAMENT_AND_RETRIEVAL.md) | Multi-agent / memory systems | K-agent bit parliament + NEON bank argmin; `make bench-agi-gate` |
| [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) | Authors, reviewers | Evidence classes, forbidden merges, falsifiers |
| [EXTERNAL_EVIDENCE_AND_POSITIONING.md](EXTERNAL_EVIDENCE_AND_POSITIONING.md) | Papers, investors | Kanerva / VSA survey / lm-eval anchors |
| [HDC_VSA_ENGINEERING_SUPERIORITY.md](HDC_VSA_ENGINEERING_SUPERIORITY.md) | Strategy, decks | Literature-backed HDC vs NN trade-offs + FAISS Hamming note + repo bridge |
| [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md) | Anyone publishing numbers | Archive checklist for microbench + harness rows |
| [ANALYSIS.md](ANALYSIS.md) | Full-stack readers | Planes A–C, parity program, frontier tables (optional paths as forward references) |
| [VISUAL_INDEX.md](VISUAL_INDEX.md) | Decks, teachers | SVG figures + Mermaid notes |
| [publish_checklist_creation_os.md](publish_checklist_creation_os.md) | Maintainers | Push **this** tree to **creation-os** `main` — checklist + `make publish-github` |
| [MLX_GUIDE.md](MLX_GUIDE.md) | MLX integrators | Python / MLX paths where applicable |
| [cursor_briefing_creation_os.md](cursor_briefing_creation_os.md) | Cursor users | Editor integration notes |
| [cursor_integration_creation_os.md](cursor_integration_creation_os.md) | Cursor users | Deeper integration |

**Quick verify:** from repository root of this tree, `make check` (build + structural tests) and `make check-v6` when the Living Kernel is in scope (see [LIVING_KERNEL_V6.md](LIVING_KERNEL_V6.md)).

---

*Spektre Labs · Creation OS · 2026*
