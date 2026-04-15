# Research program and thesis-grade architecture — Creation OS

**Audience:** dissertation committees, safety reviewers, systems researchers, and internal authors who must not confuse *pedagogy* with *production claims*.

**Companion artifacts:** portable kernel [`creation_os_v2.c`](../creation_os_v2.c); discipline [`CLAIM_DISCIPLINE.md`](CLAIM_DISCIPLINE.md); reproduction [`REPRO_BUNDLE_TEMPLATE.md`](REPRO_BUNDLE_TEMPLATE.md); full-stack map [`ANALYSIS.md`](ANALYSIS.md); external anchors [`EXTERNAL_EVIDENCE_AND_POSITIONING.md`](EXTERNAL_EVIDENCE_AND_POSITIONING.md); HDC engineering map [`HDC_VSA_ENGINEERING_SUPERIORITY.md`](HDC_VSA_ENGINEERING_SUPERIORITY.md).

---

## 0. How to read this repository as research

This tree is deliberately split into two epistemic roles:

| Role | Artifact | What it proves |
|------|-----------|----------------|
| **Portable proof** | `creation_os_v2.c` + `make test` / `make bench` | A single C11 translation unit can host a **coherent** BSC-style algebra (XOR, MAJ, σ), dozens of cognitive *demos*, and **checkable** invariants without a framework runtime. |
| **Systems hypothesis** | Planes A–C in **ANALYSIS** | A route to wrap **real** inference (GEMM-backed), Python orchestration, and native receipts **without** pretending the teaching file *is* the full stack. |

A thesis-level reader should never merge those two rows into one “we shipped AGI” sentence. The contribution is **geometry + measurement discipline + a composable map**, not a leaderboard trophy extracted from the teaching binary.

---

## 1. Research questions (explicit, bounded)

**RQ1 — Representation cost.** For a fixed dimensionality (here `D = 4096` in the documented harness), what is the **structural** RAM and op-count gap between float cosine-style similarity and XOR+POPCNT bit geometry for the *same shape* of task?

**RQ2 — Single-algebra composition.** Can a broad set of cognitive primitives (attention-like scoring, bundling, conservation checks, tamper sensitivity) be expressed **only** from XOR / MAJ / POPCOUNT-style operations while keeping σ as a **unified** coherence readout?

**RQ3 — Evaluation hygiene.** Can the repository enforce, by documentation and CI, a separation between **microbench**, **invariant**, **harness**, and **lab demo** evidence so that common ML paper failure modes (mixed baselines, appendix-thin reproduction) are *named* and *blocked* at prose level?

**RQ4 — Systems integration (hypothesis, not closed here).** Under the Planes A–C map, where should σ / Hamming receipts attach **before** expensive generation, and what receipts are required for long-horizon autonomy audits?

RQ1–RQ3 are anchored in this standalone repository. RQ4 is developed in **ANALYSIS** and optional paths that may not exist in every checkout; treat forward references as *hypothesis inventory*, not as “everything is built in this folder.”

---

## 2. Core contributions (what a dissertation could list)

Each item below is **scoped**. Falsifiers and evidence class live in **CLAIM_DISCIPLINE** and the README *Limitations* section.

**C1 — Structural ratios (arithmetic evidence class).**  
Memory and op-count ratios between float32 cosine proxy and 64×64-bit BSC words at `D=4096` follow from counting; any independent recount must agree or disclose a different problem definition.

**C2 — Measured throughput (microbench class).**  
`make bench` / §7 produces host-dependent trials/sec; must be archived with compiler flags and CPU identity when cited next to vendor numbers.

**C3 — Verified invariants (invariant class).**  
`make test` and printed σ identities are discrete pass/fail surfaces; a single counterexample is a bug or a broken port, not a “softer metric.”

**C4 — Pedagogical completeness (repository reality).**  
Twenty-six modules in one file demonstrate **composition under one geometry** for teaching and review — not benchmark parity with frontier chat models.

**C5 — Evidence ladder as an engineering artifact.**  
The evidence-ladder figure and **CLAIM_DISCIPLINE** encode an editorial policy stricter than many applied ML papers on baseline mixing.

**C6 — Systems map (hypothesis + inventory).**  
**ANALYSIS** documents Planes A–C, module groups, and evaluation modes for readers who need file-level navigation across optional trees.

---

## 3. Formal substrate (minimal, code-anchored)

**Binary spatter vocabulary.**  
Representations as `W` words of 64 bits (`creation_os_v2.c` with `core/cos_bsc.h` in split builds). **Bind** via XOR; **bundle** via MAJ; **coherence** via Hamming mass and σ as documented in README and tests.

**Why σ as a squared normalized Hamming fraction.**  
It is a deliberate choice for **structural** similarity readout in this program; any paper claiming different semantics must rename and re-derive — do not silently equate to cosine similarity of logits.

**Conservation and tamper stories.**  
Noether-style XOR-sum checks and crystal/identity sensitivity are **toy proofs of mechanism** in the reference file; they are not certificates of cryptographic strength unless separately analyzed.

---

## 4. Architecture thesis: Planes A–C (summary)

Full diagrams and file tables: **ANALYSIS** (*Layers and data flow*, *Plane A/B/C*).

**Plane A — Inference + superkernel.**  
GEMM-backed generation stays in the established stack; bit-kernel state and σ live in the superkernel path; GDA-style structures provide hypervector memory and masks where integrated.

**Plane B — Orchestration.**  
Python / MLX layers own receipts, servers, policy tooling, and harness orchestration. This plane must not “reinterpret” invariant failures as soft quality.

**Plane C — Native receipts.**  
NEON-friendly σ batches, optional Metal living-weight bias, dispatcher queues — **`cos_agi*` / `cos_mega*`** families as **audit surfaces** when present in extended checkouts.

**Dispatch principle (design invariant).**  
Prefer **retrieval and kernel gates before transformer spend** where the architecture allows — documented in project rules and README *AGI map*.

---

## 5. Methodology: evidence, threats, and what “harder than a paper” means here

**Evidence classes** are normative: see **CLAIM_DISCIPLINE**, section 1.  
**Forbidden merges** are explicit: same document, section 2.  
**Falsifiers** for the portable core: section 3.

**Threats to validity (thesis-style, not boilerplate).**

| Threat | Mitigation in-repo |
|--------|---------------------|
| Host-dependent throughput | Mandatory archive of `make bench` metadata per **REPRO_BUNDLE_TEMPLATE** |
| Toy oracle ≠ language model | README *Limitations* + forbidden merge #2 |
| JEPA toy energy ≠ generalization | README *Limitations* + forbidden merge #3 |
| Optional paths missing locally | ANALYSIS standalone note; forward references |
| Scope creep in prose | AGENTS + CLAIM_DISCIPLINE + this document |

**What “publication-hard” does *not* mean.**  
It does not grant peer-reviewed status to every demo. It means: claims are **typed**, **reproducible**, and **falsifiable** at the class that actually applies.

---

## 6. Related work and positioning

Canonical external anchors: **EXTERNAL_EVIDENCE_AND_POSITIONING** (Kanerva lineage, VSA survey, harness norms).  
Engineering superiority claims with literature rows: **HDC_VSA_ENGINEERING_SUPERIORITY**.

Positioning rule: **field consensus** (cited papers) is never swapped for **in-repo measurements** in a single sentence without a class label.

---

## 7. Artifacts and reproduction (minimum bar for a thesis appendix)

For any number that leaves this repository:

1. Git SHA or release tag.  
2. Exact compiler / `make` invocation.  
3. Machine identity.  
4. Raw stdout or JSON.  
5. Evidence class label.

Template: **REPRO_BUNDLE_TEMPLATE.md**.

CI in `.github/workflows/ci.yml` runs `make check` on Linux as a **structural** gate — not a substitute for archived harness logs.

---

## 8. Open problems (research backlog)

Aligned with **ROADMAP.md**: SME repulsion path, Metal living weights at scale, federated σ sync, persistent mmap ledgers, BBHash cold paths, GDA robotics closure, oracle self-play harnessing, physics/QHDC calibration, policy compilation from soul constraints.

Each item should enter the evidence ladder at the correct class when first measured; no premature “solved” language.

---

## 9. Responsible use and safety stance

Binary geometry does not remove misuse risk for generative hooks. Report security issues per **SECURITY.md**.  
Dual licensing (AGPL + commercial) is documented in **README** and **COMMERCIAL_LICENSE.md**; research users should keep license boundaries explicit in thesis appendices.

---

## 10. Suggested thesis chapter outline (using this repo)

1. **Introduction** — RQ1–RQ4, scope, non-goals.  
2. **Background** — HDC/BSC, related work (**EXTERNAL**, **HDC** doc).  
3. **Formal model** — σ, XOR, MAJ; pointer to `creation_os_v2.c` + tests.  
4. **Reference implementation** — module walk, invariants, limitations.  
5. **Microbenchmarks** — `make bench` protocol, arithmetic ratios.  
6. **Systems architecture** — Planes A–C from **ANALYSIS**; optional trees as future work.  
7. **Evaluation ethics** — evidence ladder, forbidden merges, harness vs native.  
8. **Reproducibility** — bundle template + CI.  
9. **Conclusion** — what is proven vs hypothesized; open problems from **ROADMAP**.  
10. **(Optional) Industry-scale challenges** — public narratives only: [COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md](COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md).

---

## 11. Pre-defense and submission gates

| Gate | Document |
|------|----------|
| Hostile review simulation (category errors, toys, physics, repro, license) | [ADVERSARIAL_REVIEW_CHECKLIST.md](ADVERSARIAL_REVIEW_CHECKLIST.md) |
| Per-section evidence class (§1–§26) | [MODULE_EVIDENCE_INDEX.md](MODULE_EVIDENCE_INDEX.md) |
| LaTeX bibliography | [CITATION.bib](CITATION.bib) (and [CITATION.cff](../CITATION.cff) for CFF-aware tools) |
| Industry / robotics alignment (public themes, bounded claims) | [COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md](COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md) |
| Shared vocabulary | [GLOSSARY.md](GLOSSARY.md) |
| §7 benchmark audit trail | [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md) |
| English-only policy | [LANGUAGE_POLICY.md](LANGUAGE_POLICY.md) |
| Viral misreadings FAQ | [COMMON_MISREADINGS.md](COMMON_MISREADINGS.md) |

---

*Spektre Labs · Creation OS · 2026 · English research prose — maintain with CLAIM_DISCIPLINE invariants.*
