# Documentation index — Creation OS (`creation-os`)

Single entry point for humans and automation. **All committed prose here is English.**

| Document | Audience | Purpose |
|----------|----------|---------|
| [README.md](../README.md) | Everyone | Problem + measured table, bounded LLM comparison, FIG 09 scan map, BSC primer, invariants, build, limitations, theory links, VISUAL_INDEX-aligned figures, Living Kernel v6 |
| [VOCAB_PIPELINE_V27.md](VOCAB_PIPELINE_V27.md) | Integrators, tokenizer / FPGA roadmap readers | v27 shipped vs directive roadmap; mmap COSB + benches + optional Rust/SBY hooks |
| [hallucination_reduction.md](../benchmarks/hallucination_reduction.md) | Benchmark authors | TruthfulQA / harness evidence note (external; not merge-gate) |
| [WHAT_IS_REAL.md](WHAT_IS_REAL.md) | v29 readers / agents | Tier-tagged claims for the v29 harness (measured vs not claimed) |
| [v41_test_time_compute.md](v41_test_time_compute.md) | Integrators, test-time scaling readers | v41 lab: σ-guided budget forcing / adaptive N / toy reasoning tree + benchmark stub (`make check-v41`) |
| [v42_self_play.md](v42_self_play.md) | Integrators, self-play RL readers | v42 lab: σ-guided challenger/solver loop + replay sampling + benchmark stub (`make check-v42`) |
| [v43_sigma_distill.md](v43_sigma_distill.md) | Integrators, distillation readers | v43 lab: σ-weighted KD + progressive curriculum + multi-teacher ensemble + calibration (`make check-v43`) |
| [v44_inference_proxy.md](v44_inference_proxy.md) | Integrators, serving readers | v44 lab: σ-proxy (`creation_os_proxy`) + OpenAI-shaped loopback HTTP + demo SSE (`make check-v44`) |
| [v45_introspection.md](v45_introspection.md) | Integrators, calibration readers | v45 lab: σ-introspection (calibration gap, doubt reward, internal probe stub; `make check-v45`) |
| [v46_bitnet_sigma.md](v46_bitnet_sigma.md) | Integrators, BitNet / CPU inference readers | v46 lab: σ-from-logits fast path + SIMD + adaptive quant + SPEED_TABLE scaffold (`make check-v46`) |
| [v47/INVARIANT_CHAIN.md](v47/INVARIANT_CHAIN.md) | Verification / claims hygiene readers | v47 lab: invariant chain (M/T/P) + `make verify` stack notes + ZK honesty |
| [v48/RED_TEAM_REPORT.md](v48/RED_TEAM_REPORT.md) | Security / red-team readers | v48 lab: σ-armored attack surface table + honest limits + harness pointers |
| [v49/certification/README.md](v49/certification/README.md) | Certification / assurance readers | v49 lab: DO-178C-aligned artifact pack index + `make certify` entrypoint |
| [v50/FAQ_CRITICS.md](v50/FAQ_CRITICS.md) | Public critique / review readers | v50: pre-baked answers to common dismissal patterns (tier-honest) |
| [benchmarks/v50/FINAL_RESULTS.md](../benchmarks/v50/FINAL_RESULTS.md) | Benchmark readers | v50 rollup report (regenerate via `make v50-benchmark`) |
| [v51/ARCHITECTURE.md](v51/ARCHITECTURE.md) | Integration / reviewer readers | v51: six-phase cognitive loop + σ-gated agent + full-stack diagram (scaffold tier) |
| [v53/ARCHITECTURE.md](v53/ARCHITECTURE.md) | Harness / runtime readers | v53: σ-governed harness scaffold — σ-TAOR loop, σ-dispatch, σ-compression, `creation.md` loader |
| [v53/POSITIONING.md](v53/POSITIONING.md) | Reviewers comparing with Claude Code | v53: side-by-side positioning vs Claude Code (structural, not empirical) |
| [v53/paper_draft.md](v53/paper_draft.md) | Paper readers | v53: position paper "Harness Architecture: σ-Governance vs Time-Limits in Agentic Coding" (I-tier) |
| [v54/ARCHITECTURE.md](v54/ARCHITECTURE.md) | Multi-LLM / routing readers | v54: σ-proconductor scaffold — σ-profile routing, σ-weighted aggregation, disagreement abstain, EWMA learner |
| [v54/POSITIONING.md](v54/POSITIONING.md) | Reviewers comparing with MoA / RouteLLM / MoMA / Bayesian Orchestration | v54: side-by-side positioning vs 2024–2026 routing literature |
| [v54/paper_draft.md](v54/paper_draft.md) | Paper readers | v54: position paper "σ-Proconductor: σ as the Missing Routing Signal for Multi-LLM Ensembles" (I-tier) |
| [v55/ARCHITECTURE.md](v55/ARCHITECTURE.md) | Spec-decode / uncertainty readers | v55: σ₃-speculative scaffold — three-component σ + EARS + EASD with NEON hot path |
| [v55/POSITIONING.md](v55/POSITIONING.md) | Reviewers comparing with EARS / EASD / Taparia et al. | v55: composition argument — EARS + EASD + σ₃ are complementary |
| [v55/paper_draft.md](v55/paper_draft.md) | Paper readers | v55: position paper wiring Taparia 2603.24967, Sun 2512.13194, Su 2512.23765 into one C11 policy layer (I-tier) |
| [v56/ARCHITECTURE.md](v56/ARCHITECTURE.md) | Self-modification / hardware readers | v56: σ-Constitutional scaffold — rule-based VPRM verifier + σ-gated IP-TTT budget + grokking commutator σ-channel + ANE `matmul→1×1 conv` layout; one invariant (any self-modification must lower σ) |
| [v56/POSITIONING.md](v56/POSITIONING.md) | Reviewers comparing with VPRM / ThinkPRM / IP-TTT / TTSR / SLT grokking / ANE RE / SME2 | v56: Q1-2026 frontier map + composition tree + explicit-not-scope (no Core ML, no live TTT) |
| [v56/paper_draft.md](v56/paper_draft.md) | Paper readers | v56: position paper wiring VPRM 2601.17223, IP-TTT 2604.06169, SLT grokking 2603.01192 + 2603.13331, and 2026 ANE reverse-engineering into one C11 policy layer with 56/56 self-test (I-tier) |
| [v57/THE_VERIFIED_AGENT.md](v57/THE_VERIFIED_AGENT.md) | Anyone evaluating "is this agent runtime verified?" | v57: one-page articulation of The Verified Agent — convergence of v33–v56 with `make verify-agent` aggregate driver; explicit non-claims list (no FAA / EASA cert, no zero-CVE claim, no frontier-accuracy claim) |
| [v57/ARCHITECTURE.md](v57/ARCHITECTURE.md) | Reviewers / integrators | v57: composition wire map, tier semantics (M / F / I / P, no blending), per-slot responsibilities, registry + aggregator separation |
| [v57/POSITIONING.md](v57/POSITIONING.md) | Reviewers comparing with ad-hoc agent sandboxes (OSS / hardened / enterprise) | v57: side-by-side table of license / isolation / proofs / user-facing claim; the only column whose answer is "verify yourself" is v57 |
| [v57/paper_draft.md](v57/paper_draft.md) | Paper readers | v57: position paper "The Verified Agent: Tier-Honest Composition of Sandboxing, σ-Governed Reasoning, and Multi-LLM σ-Triangulation" — convergence story with `make verify-agent` reproducibility (I-tier) |
| [v58/THE_SIGMA_CACHE.md](v58/THE_SIGMA_CACHE.md) | Anyone asking "which KV-cache eviction policy does Creation OS use?" | v58: one-page articulation of σ-Cache — σ = (ε, α) decomposition as the retention signal + four-valued per-token precision tag (FULL / INT8 / INT4 / EVICT) + branchless NEON kernel; composed into v57 Verified Agent |
| [v58/ARCHITECTURE.md](v58/ARCHITECTURE.md) | Reviewers / integrators | v58: pipeline wire map (score → threshold → decide → compact), tier table per claim (M / F / I / P), hardware discipline checklist per `.cursorrules` |
| [v58/POSITIONING.md](v58/POSITIONING.md) | Reviewers comparing against StreamingLLM / H2O / SnapKV / KIVI / EntropyCache | v58: comparative table + explicit non-claims; the only row with a decomposed retention signal, branchless kernel, deterministic self-test, and published microbench |
| [v58/paper_draft.md](v58/paper_draft.md) | Paper readers | v58: "σ-Cache: σ-decomposed KV-cache eviction with per-token precision tiers and a branchless NEON kernel" — policy, kernel, correctness proof (68/68 deterministic self-test), 3-point microbench, positioning, limitations |
| [v59/THE_SIGMA_BUDGET.md](v59/THE_SIGMA_BUDGET.md) | Anyone asking "how does Creation OS decide when to stop thinking?" | v59: one-page articulation of σ-Budget — σ = (ε, α) decomposition as the adaptive compute budget controller + four-valued per-step tag (CONTINUE / EARLY_EXIT / EXPAND / ABSTAIN) + branchless NEON kernel; composed into v57 Verified Agent |
| [v59/ARCHITECTURE.md](v59/ARCHITECTURE.md) | Reviewers / integrators | v59: pipeline wire map (score → decide → summarize), tier table per claim (M / F / I / P), hardware-discipline checklist per `.cursorrules` |
| [v59/POSITIONING.md](v59/POSITIONING.md) | Reviewers comparing against TAB / CoDE-Stop / LYNX / DTSR / DiffAdapt / Coda / AdaCtrl / Risk-Control BF | v59: comparative table + explicit non-claims; the only row with a σ-decomposed decision surface and a faithful `ABSTAIN` tag |
| [v59/paper_draft.md](v59/paper_draft.md) | Paper readers | v59: "σ-Budget: σ-decomposed adaptive test-time compute budgeting with a branchless four-valued decision kernel" — policy, kernel, correctness proof (69/69 deterministic self-test), 3-point microbench (1.1–1.5 × 10⁸ decisions / s), positioning, limitations |
| [v60/THE_SIGMA_SHIELD.md](v60/THE_SIGMA_SHIELD.md) | Anyone asking "how does Creation OS refuse ambiguous-intent actions?" | v60: one-page articulation of σ-Shield — five-valued branchless capability authorise + σ-gated intent + TOCTOU-free args + code-page integrity; first cap-kernel to use σ=(ε,α) decomposition as the intent gate |
| [v60/ARCHITECTURE.md](v60/ARCHITECTURE.md) | Reviewers / integrators | v60: wire map (five orthogonal checks, priority cascade via mask AND-NOT), per-surface tier table (M / F / I / P), hardware-discipline checklist, 3-point microbench result |
| [v60/POSITIONING.md](v60/POSITIONING.md) | Reviewers comparing against seL4 / WASM / IronClaw / ShieldNet / Claude Code on the 2026 attack landscape (DDIPE, ClawWorm, Malicious Intermediary) | v60: comparative matrix + explicit non-claims; the only row with both a capability gate and an intent-σ gate; sibling of v59 σ-Budget |
| [v60/paper_draft.md](v60/paper_draft.md) | Paper readers | v60: "σ-Shield: Decomposed-Uncertainty Capability Kernels for LLM Agents" — abstract, policy, kernel, 81-test breakdown, microbench, positioning, limitations, artefacts |
| [v61/THE_CITADEL.md](v61/THE_CITADEL.md) | Anyone asking "how does Creation OS compose the full advanced-security menu?" | v61: one-page articulation of Σ-Citadel — BLP + Biba + MLS lattice + attestation + seL4/Wasmtime/eBPF/sandbox-exec/pledge/Nix/distroless/Sigstore/SLSA-v1.0 composed under one `make chace` aggregator |
| [v61/ARCHITECTURE.md](v61/ARCHITECTURE.md) | Reviewers / integrators | v61: wire map (v60 → v61 lattice → compose → tool exec → attest → verify), per-surface tier table, hardware-discipline checklist, 6.1 × 10⁸ decisions/s M4 microbench |
| [v61/POSITIONING.md](v61/POSITIONING.md) | Reviewers comparing against DARPA CHACE / seL4-only / Wasmtime-only / IronClaw / Claude Code | v61: per-layer best-in-class vs v61 stance; the only open-source row that ships every CHACE menu layer as one runnable gate |
| [v61/paper_draft.md](v61/paper_draft.md) | Paper readers | v61: "Σ-Citadel: Composed Defence in Depth for Local AI Agents" — abstract, design, 61-test breakdown, microbench, positioning, limitations, `make chace` composition |
| [v62/THE_FABRIC.md](v62/THE_FABRIC.md) | Anyone asking "where did the 2026 reasoning frontier land in this repo?" | v62: one-page articulation of the Reasoning Fabric — Coconut latent CoT + EBT verifier + HRM H/L loop + Native Sparse Attention + DeepSeek-V3 MTP + ARKV adaptive KV manager — six branchless C kernels, one ABI, composed with v60 + v61 as a 3-bit decision behind the Apple-tier `cos` CLI |
| [v62/ARCHITECTURE.md](v62/ARCHITECTURE.md) | Reviewers / integrators | v62: wire map (cos → verify-agent / chace → v60 / v61 / v62 → composed decision), M4 hardware-discipline checklist, microbench (~ 8 200 NSA calls/s, ~ 3.7 M EBT calls/s), threat-model tie-in |
| [v62/POSITIONING.md](v62/POSITIONING.md) | Reviewers comparing against Coconut / EBT / HRM / NSA / DeepSeek MTP / ARKV / Claude Code / Cursor CLI / Aider / llama.cpp / MLX-LM | v62: per-paper, per-tool comparison + comparison matrix; the only open-source row to ship the full 2026 reasoning frontier as one C ABI on Apple silicon, with composed σ-Shield + Σ-Citadel + EBT decision |
| [v62/paper_draft.md](v62/paper_draft.md) | Paper readers | v62: "The Reasoning Fabric: distilling the 2026 frontier into branchless C" — abstract, design, 68-test breakdown, microbench, composition with v60 + v61, positioning, limitations, references |
| [v63/THE_CIPHER.md](v63/THE_CIPHER.md) | Anyone asking "how are the messages between the Creation OS kernels protected?" | v63: one-page articulation of σ-Cipher — BLAKE2b-256 + HKDF + ChaCha20-Poly1305 AEAD + X25519 + attestation-bound sealed envelope + forward-secret ratchet + IK-like handshake + optional ML-KEM-768 hybrid; dependency-free C kernel composed with v60 + v61 + v62 as a 4-bit branchless decision |
| [v63/ARCHITECTURE.md](v63/ARCHITECTURE.md) | Reviewers / integrators | v63: wire map (v60 → v61 → v62 → v63 seal → 4-bit composed decision), M4 hardware-discipline checklist, build matrix, microbench (~564 MiB/s AEAD, ~1267 MiB/s BLAKE2b, ~12 k X25519 ops/s, ~338 k seal ops/s), threat-model tie-in |
| [v63/POSITIONING.md](v63/POSITIONING.md) | Reviewers comparing against Chutes / Tinfoil / reishi-handshake / Signal SPQR / Voidly / libsodium / TweetNaCl | v63: per-system write-up + comparison matrix; the only local-AI-agent row shipping every 2026 encryption primitive as one dependency-free C kernel with a 4-bit composed decision |
| [v63/paper_draft.md](v63/paper_draft.md) | Paper readers | v63: "σ-Cipher: distilling the 2026 end-to-end encryption frontier into branchless C" — abstract, design (4-bit composition, attestation-bound envelope, primitives, handshake, ratchet), 144-test breakdown, microbench, limitations, RFC references |
| [../SECURITY.md](../SECURITY.md) | Everyone (policy + reporting) | Supported-versions table, reporting process, tier semantics, active guarantees (M-tier minimum), explicit non-guarantees, local-dev quick-start (harden / sanitize / hardening-check / security-scan / sbom / reproducible-build) |
| [../THREAT_MODEL.md](../THREAT_MODEL.md) | Reviewers / adversarial testers | 7 assets, 7 adversary tiers, STRIDE × σ-Shield matrix, arXiv-per-row mapping (DDIPE / ClawWorm / Malicious Intermediary / ShieldNet), 7 invariants auto-checked by `make check-v60` |
| [creation.md](../creation.md) | Agent operators / contributors | Project **invariants** (not instructions) + σ-profile per task type |
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
