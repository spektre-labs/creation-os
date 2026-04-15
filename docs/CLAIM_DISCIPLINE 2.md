# Claim discipline — Creation OS (standalone kernel + full tree)

**Purpose:** make every sentence in READMEs, papers, and investor decks **survive cross-examination** — not by louder adjectives, but by **binding claims to evidence classes** and **forbidding known failure modes** (mixed baselines, silent precision drift, demo-as-harness).

**Repository language:** all committed prose is **English only** — [LANGUAGE_POLICY.md](LANGUAGE_POLICY.md).

---

## 1. Evidence classes (never merge without a wall)

| Class | What it is | Typical artifact |
|--------|------------|-------------------|
| **Arithmetic (structure)** | Ratios fixed by `D`, dtype, and chosen op counts | Derivation in README table footnotes; independent recount must agree |
| **Measured (microbench)** | Wall-clock / trials-per-sec on a named host | `make bench` stdout + compiler flags + CPU model |
| **Verified (invariant)** | Must hold on every run, every platform, under stated seeds | `./creation_os` invariant block; failing output = bug or broken port |
| **Harness (frontier tables)** | `lm-eval` / official task scripts, no casual `--limit` | Archived JSON + harness git SHA + model id |
| **Lab demo (C)** | Smoke / pedagogy binaries | Not LM quality; not interchangeable with harness rows |
| **Repository reality** | What CMake / Make actually link | Paths in ANALYSIS; “exists” ≠ “dominates production” |

**Rule:** one headline number → **one** class. If you need two numbers (e.g. native speed vs harness accuracy), you need **two rows** and explicit labels — see ANALYSIS *Evaluation modes*.

---

## 2. Forbidden merges (harder than typical ML write-ups)

1. **Microbench throughput × MMLU delta** in one sentence without a section break — *different questions, different uncertainty*.
2. **Oracle toy corpus fluency × chat model quality** — the Oracle proves σ-as-attention mechanics, not BLEU parity with GPT-class LMs.
3. **JEPA toy energy descent × generalization** — energy falls because the codebook stores pairs; generalization is a separate claim requiring held-out evaluation.
4. **“4096 bits” × “replaces QFT / SM dynamics”** — explicitly out of scope; Standard Model toys in-tree are labeled as **not lattice QFT** where documented.

---

## 3. Falsifiers (what would actually break the story)

For **`creation_os_v2.c`** as shipped in this standalone tree:

- `σ(x, x) ≠ 0` for identical bit patterns → core distance definition violated.
- Documented Noether XOR-sum conservation fails under **symmetric** interaction as described → algebraic invariant or demo wiring wrong.
- MAJ superposition claims fail under the documented construction with no change to definitions → bundle semantics mis-stated.

For **full Spektre tree** claims: bind each subsystem to its own test target (`make test`, harness script, native suite) — do not export a green checkmark from subsystem A to headline B.

---

## 4. Minimum reproducibility bundle (when you publish a number)

Archive together:

1. Git commit SHA (or release tag).
2. Exact build line (`cc …` / `make …`).
3. `uname -a` (or equivalent) and, on Apple Silicon, SoC generation if known.
4. Raw stdout or JSON for the command that produced the number.
5. One-line **evidence class** label above.

**Copy-paste form:** [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md).

That bundle is stricter than many appendix-free papers and matches how safety-critical systems document regressions.

---

## 5. Tone: “harder than a publication” without hype

**Hard** means: **bounded verbs** (*demonstrates*, *measures*, *implements*), **explicit limits** (*Limitations* section), **named falsifiers**, and **no category error** between bit geometry and float LM SOTA.

**Soft** means: superlatives without anchors, blended baselines, and “revolution” without a mechanistic sentence a reviewer can attack line-by-line.

This document is the line between them.

---

## 6. Peer-reviewed and community anchors (outside the diff)

For **bibliography-ready** links and a short “what the field already agrees on vs what this git tree proves” table, see **[EXTERNAL_EVIDENCE_AND_POSITIONING.md](EXTERNAL_EVIDENCE_AND_POSITIONING.md)**.

For **research questions, contribution list, threats to validity, and a suggested dissertation chapter outline**, see **[RESEARCH_AND_THESIS_ARCHITECTURE.md](RESEARCH_AND_THESIS_ARCHITECTURE.md)**.

---

*Spektre Labs · Creation OS · 2026*
