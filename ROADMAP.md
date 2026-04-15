# ROADMAP

This file tracks **what is already in the reference C kernel** versus **platform-scale modules** that belong to the wider Spektre / Creation OS program. For **how to cite benchmarks and avoid mixed baselines**, read **[docs/CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md)**; for **research questions, thesis chapter outline, and threats to validity**, read **[docs/RESEARCH_AND_THESIS_ARCHITECTURE.md](docs/RESEARCH_AND_THESIS_ARCHITECTURE.md)**; for the **three-plane architecture and AGI primitive map**, read **[docs/ANALYSIS.md](docs/ANALYSIS.md)**.

## Shipped in `creation_os_v2.c` (v2.0 reference)

| § | Status | Item |
|---|--------|------|
| §1 | [x] | BSC core (XOR, MAJ, ROTL, σ) |
| §2 | [x] | Hypercube mind + SOC |
| §3 | [x] | Oracle (n-gram, correlative encoding) |
| §4 | [x] | Soul + Crystal Lock |
| §5 | [x] | Proconductor |
| §6 | [x] | JEPA world model loop |
| §7 | [x] | GEMM vs BSC benchmark |
| §8 | [x] | Genesis / Noether |
| §9–§26 | [x] | Metacognition … Authentication (stdout demos) |

## Shipped as `creation_os_v6.c` (Living Kernel — narrative σ scaffold)

| Item | Status | Notes |
|------|--------|------|
| Second standalone C11 binary | [x] | `creation_os_v6.c` — M01–M18 toys + 30 `self_test` checks |
| Doc + discipline | [x] | [docs/LIVING_KERNEL_V6.md](docs/LIVING_KERNEL_V6.md); README + COMMON_MISREADINGS + MODULE appendix |
| Build | [x] | `make check-v6` |

## Shipped as `creation_os_v7.c` (Hallucination Killer — v6 + M19–M23)

| Item | Status | Notes |
|------|--------|------|
| Third standalone C11 binary | [x] | `creation_os_v7.c` — M19–M23 σ toys + 35 `self_test` checks |
| Doc + discipline | [x] | [docs/HALLUCINATION_KILLER_V7.md](docs/HALLUCINATION_KILLER_V7.md); README + CHANGELOG + DOC_INDEX |
| Build | [x] | `make check-v7`; CI with v2 + v6 |

**Not** a replacement for §7 `make bench`, NEON coherence batch, or HV parliament bench — see LIVING_KERNEL / HALLUCINATION_KILLER doc *non-claims*.

## AGI platform — 22 forward modules (planned)

Format: checkbox, name, one line. These are **not** the §9–§26 demos above; they describe **product-scale** extensions beyond the reference file.

- [ ] **SME repulsion path** — native matrix-extension kernel where toolchain allows  
- [ ] **Metal living weights** — parallel POPCNT → logit bias dispatch  
- [ ] **Core ML / NE inference** — host-chosen model bundle, no weights in git  
- [ ] **Federated σ sync** — multi-node HV consensus protocol  
- [ ] **Persistent shadow ledger** — mmap-backed σ receipts  
- [ ] **BBHash O(1) lexicon** — cold path before transformer  
- [ ] **GDA perception deploy** — `gda/*` wired to live sensors (not demo-only)  
- [ ] **GDA robotics loop** — closed-loop σ control  
- [ ] **Oracle train scale** — streaming codebook + disk spill  
- [ ] **Self-play v3 harness** — proposer/solver/verifier roles as processes  
- [ ] **Oracle 10M CLI** — throughput gate with config surface  
- [ ] **Physics triple-universe** — extended genesis invariants  
- [ ] **QHDC calibration** — measured noise model for `qhdc_core`  
- [ ] **Soul policy compiler** — values → executable constraints  
- [ ] **JEPA video σ** — windowed HV over frames  
- [ ] **llama.cpp ghost boot** — real `gda_llama_integration` build  
- [ ] **Cluster HV registry** — B200/H100 slot tables as HV metadata  
- [ ] **CI matrix** — macOS + Linux, `make all`, artifact upload  
- [ ] **Fuzz σ codec** — libFuzzer on `encode_text`  
- [ ] **Bench regression gate** — store `trials_per_sec_*` baselines  
- [ ] **Docs Zenodo link** — Zenodo DOI in README when live  
- [ ] **Release signing** — tags + checksums on GitHub Releases  
