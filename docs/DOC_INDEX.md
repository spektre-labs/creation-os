# Documentation index — Creation OS (`creation-os`)

Single entry point for humans and automation. **All committed prose here is English.**

| Document | Audience | Purpose |
|----------|----------|---------|
| [README.md](../README.md) | Everyone | Problem + measured table, bounded LLM comparison, FIG 09 scan map, BSC primer, invariants, build, limitations, theory links, VISUAL_INDEX-aligned figures, Living Kernel v6 |
| [VOCAB_PIPELINE_V27.md](VOCAB_PIPELINE_V27.md) | Integrators, tokenizer / FPGA roadmap readers | v27 shipped vs directive roadmap; mmap COSB + benches + optional Rust/SBY hooks |
| [hallucination_reduction.md](../benchmarks/hallucination_reduction.md) | Benchmark authors | TruthfulQA / harness evidence note (external; not merge-gate) |
| [WHAT_IS_REAL.md](WHAT_IS_REAL.md) | v29 readers / agents | Tier-tagged claims for the v29 harness (measured vs not claimed) |
| [LOCAL_OPENAI_STUB.md](LOCAL_OPENAI_STUB.md) | Integrators wiring local tools | Optional loopback OpenAI-shaped stub (`creation_os_openai_stub`) — protocol smoke, not a product replacement |
| [WHICH_FILE_TO_READ.md](WHICH_FILE_TO_READ.md) | Reviewers, integrators | “Start here” map: v2 bootstrap vs v26 merge-gate harness; directory map; reviewer checklist |
| [SUITE_LAB.md](SUITE_LAB.md) | Integrators, browser demos | Optional mode metadata CLI + static page + launch script; honest scope (not merge-gate) |
| [v31_README.md](v31_README.md) | Integrators | v31 “purge lab”: optional upstream wrapper direction + commands (`make check-v31`) |
| [WHAT_IS_REAL_v31.md](WHAT_IS_REAL_v31.md) | Reviewers | Tier-tagged contract for v31 lab claims (not default product surface) |
| [FULL_LOCAL_SUITE.md](FULL_LOCAL_SUITE.md) | Integrators | What “full suite” scripts often promise vs what this repo ships; safe incremental path |
| [REPOS_AND_ROLES.md](REPOS_AND_ROLES.md) | Everyone, agents | **Consortium map:** creation-os vs protocol vs corpus vs Genesis; single push target; where insights land |
| [CANONICAL_GIT_REPOSITORY.md](CANONICAL_GIT_REPOSITORY.md) | Everyone, agents | Push **only** `spektre-labs/creation-os` for this kernel; forbidden remote mistakes |
| [RTL_SILICON_MIRROR.md](RTL_SILICON_MIRROR.md) | Integrators, FPGA/ASIC | SystemVerilog + optional **Chisel** (`hw/chisel`); `make formal-rtl-lint`, `make chisel-verilog` |
| [FULL_STACK_FORMAL_TO_SILICON.md](FULL_STACK_FORMAL_TO_SILICON.md) | Architects, thesis | Open stack map: papers → C → SV/Chisel → Yosys/OpenROAD/PDK; evidence-class caveats |
| [FEATURES_AND_STANDALONE_BUILDS.md](FEATURES_AND_STANDALONE_BUILDS.md) | Everyone | One-page map: `creation_os` vs v6/v7/v9/v10, `make check*`, module ranges, CI |
| [LIVING_KERNEL_V6.md](LIVING_KERNEL_V6.md) | Integrators, thesis writers | What `creation_os_v6.c` does, evidence class, non-claims, M01–M18 map, `make check-v6` |
| [HALLUCINATION_KILLER_V7.md](HALLUCINATION_KILLER_V7.md) | Integrators, thesis writers | v7 = v6 + M19–M23 hallucination-shaped σ toys; `make check-v7` |
| [PARAMETERS_IN_SILICON_V9.md](PARAMETERS_IN_SILICON_V9.md) | Integrators, thesis writers | v9 = v7 + M24–M29 stack/silicon σ schematics; `make check-v9` |
| [THE_REAL_MIND_V10.md](THE_REAL_MIND_V10.md) | Integrators, thesis writers | v10 = v9 + M30–M33 distillation / few-shot / swarm / abstention toys; `make check-v10` |
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

**Quick verify:** from repository root of this tree, `make check` (build + structural tests); `make check-v6` / `make check-v7` / `make check-v9` / `make check-v10` when editing those sources ([LIVING_KERNEL_V6.md](LIVING_KERNEL_V6.md), [HALLUCINATION_KILLER_V7.md](HALLUCINATION_KILLER_V7.md), [PARAMETERS_IN_SILICON_V9.md](PARAMETERS_IN_SILICON_V9.md), [THE_REAL_MIND_V10.md](THE_REAL_MIND_V10.md)). **What ships where:** [FEATURES_AND_STANDALONE_BUILDS.md](FEATURES_AND_STANDALONE_BUILDS.md).

---

*Spektre Labs · Creation OS · 2026*
