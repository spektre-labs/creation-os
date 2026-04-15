# Adversarial review checklist — Creation OS

**Purpose:** anticipate the first attacks a **hostile but competent** reviewer (dissertation opponent, safety panel, or systems PC) will make — and point to **where this repository already answers**, where it **explicitly refuses the claim**, and where **more measurement is required**.

Use this as a **pre-submission** gate for papers, theses, and public posts that cite this tree.

---

## A. Category errors (mixing evidence classes)

| Attack | Safe response (in-repo) |
|--------|-------------------------|
| “You report 192× and also cite MMLU — so the model is 192× better.” | **Forbidden merge #1** in [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md); README *Measured results* footnotes; evidence ladder in README *Publication-hard*. |
| “`make bench` throughput equals chat quality.” | Different classes: microbench vs LM quality; README *Limitations*; [RESEARCH_AND_THESIS_ARCHITECTURE.md](RESEARCH_AND_THESIS_ARCHITECTURE.md) RQ3. |
| “Invariant tests imply harness parity.” | `make test` covers **algebraic** σ / MAJ / Noether / crystal demos — not ARC/MMLU; ANALYSIS *Evaluation modes*. |

---

## B. Oracle, JEPA, and toy scale

| Attack | Safe response |
|--------|----------------|
| “The Oracle proves GPT-class language modeling.” | **Forbidden merge #2**; README *Limitations* (15-sentence corpus, n-gram codebook). |
| “JEPA energy drop proves generalization.” | **Forbidden merge #3**; README *Limitations* (memorization story). |
| “Demo output is cherry-picked.” | Archive full stdout + commit SHA per [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md); CI runs `make check` (structural), not selective demo lines. |

---

## C. Physics, bits, and cosmology rhetoric

| Attack | Safe response |
|--------|----------------|
| “4096 bits replace QFT / lattice QCD.” | **Forbidden merge #4**; README and CLAIM_DISCIPLINE; genesis toys are **pedagogical** unless separately validated. |
| “σ is a consciousness measure in the philosophical sense.” | §13 is a **defined composite** in C demos — not a clinical instrument; README *Limitations* on cognitive modules. |

---

## D. Reproducibility and portability

| Attack | Safe response |
|--------|----------------|
| “Results are not reproducible without GPUs.” | `creation_os_v2.c` is **stdlib + libm**; `cc -O2 -I.` line in README; `make check` on Linux CI. |
| “Throughput numbers are not portable.” | True — **Measured (microbench)** requires host log; arithmetic ratios do not. |

---

## E. Architecture and AGI wording

| Attack | Safe response |
|--------|----------------|
| “This repo is the entire production AGI stack.” | README *AGI map*: teaching file vs Planes A–C; [ANALYSIS.md](ANALYSIS.md) optional paths; RESEARCH doc **portable vs systems hypothesis** table. |
| “`cos_agi*` proves safe deployment.” | Native symbols are **receipt / audit surfaces** when present — not certificates without threat modeling; extend with your deployment’s safety case. |

---

## F. Legal, license, and dual-use

| Attack | Safe response |
|--------|----------------|
| “AGPL does not apply / commercial use is unclear.” | [LICENSE](../LICENSE), [COMMERCIAL_LICENSE.md](../COMMERCIAL_LICENSE.md), README *License*. |
| “No security contact.” | [SECURITY.md](../SECURITY.md). |

---

## G. “We solve the same problems as [frontier lab]” without evidence

| Attack | Safe response |
|--------|----------------|
| “This repo replaces [named org]’s safety stack.” | It does not. See [COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md](COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md) scope boundary; claims stay on **this tree’s** artifacts. |
| “Named companies validate our roadmap.” | **Illustrative public themes only** — no implied partnership; cite their papers separately if needed. |

---

## H. What still legitimately needs *your* extra work

- Held-out evaluation for any module you want to claim **generalizes** beyond the toy.  
- Harness runs with archived configs for any **frontier comparison**.  
- Independent reimplementation of σ / MAJ / XOR counts for **third-party audit** of arithmetic ratios.  
- Threat modeling for any **networked** or **tool-using** deployment (not in scope for the standalone C file).

---

*Spektre Labs · Creation OS · 2026 · Pair with [MODULE_EVIDENCE_INDEX.md](MODULE_EVIDENCE_INDEX.md) and [COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md](COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md).*
