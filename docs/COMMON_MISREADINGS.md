# Common misreadings — quick corrections (English-only repo)

**Use:** cite the **Misreading** column and the **Where to read** link in reviews, audits, and external summaries. Pair with [PARADIGM_SNAPSHOT_FOR_DRIVE_BY_READERS.md](PARADIGM_SNAPSHOT_FOR_DRIVE_BY_READERS.md) for a short orientation.

**Language:** this file is **English** — the [language policy](LANGUAGE_POLICY.md) applies to all committed docs here.

**Figures:** SVG labels live in repository text — verify the committed `<text>` nodes (see [VISUAL_INDEX.md](VISUAL_INDEX.md)); do not treat lossy raster exports as authoritative strings.

---

| Misreading | Correction (one sentence) | Where to read |
|------------|-----------------------------|----------------|
| “The diagram literally says mollock / malloc / no heap except stack.” | That was **never** repo policy; some SVGs previously held **corrupt bytes** in `<text>` nodes (control characters / invalid UTF-8), which rendered as junk glyphs — public diagram labels are now **ASCII** (`/` separators) in `docs/assets/*.svg`. | [VISUAL_INDEX.md](VISUAL_INDEX.md), committed SVG source |
| “README says this C file never uses `malloc`.” | The portable kernel is **pedagogical C11**; workspace **M4 / native** guidance may prefer `aligned_alloc` for hot buffers — that is **silicon guidance**, not a claim that every line in every subtree is `malloc`-free. | `.cursorrules` (Creation OS), [ANALYSIS.md](ANALYSIS.md) |
| “A raster export of the README is sufficient citation.” | **Cite the Markdown revision** (commit hash); raster exports drop footnotes, blur small text, and are not canonical for numbers or qualifiers. | README *Measured results* footnotes, [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) |
| “Scope disclaimers only appear deep in the README.” | **Forbidden merges** and the evidence ladder are linked in the README documentation hub and beside the benchmark table. | README *Documentation hub*, [evidence-ladder.svg](assets/evidence-ladder.svg) |
| “This repo replaces transformers / GPT-class models.” | It does **not**; the portable file is a **teaching kernel** and cost-shape demo; full inference stays in other planes when integrated. | README *AGI map*, [ANALYSIS.md](ANALYSIS.md) |
| “192× means 192× better on benchmarks like MMLU.” | **Forbidden merge:** 192× is an **op-count proxy** for the defined microbench, not a harness score delta. | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) (Forbidden merges) |
| “σ is consciousness in the philosophy sense.” | Section 13 is a **toy composite** in C demos, not a clinical or philosophical proof. | README *Limitations*, [MODULE_EVIDENCE_INDEX.md](MODULE_EVIDENCE_INDEX.md) |
| “Oracle output quality equals LLM quality.” | Oracle uses a **tiny n-gram codebook** — it demonstrates σ-as-attention **mechanics**, not chat parity. | README *Limitations*, CLAIM_DISCIPLINE |
| “JEPA energy drop proves generalization.” | Toy codebook memorization — energy falls for that reason; generalization needs **held-out** protocol. | README *Limitations*, CLAIM_DISCIPLINE |
| “Genesis / Noether proves physics / QFT.” | **Forbidden merge:** toy universe, not lattice QCD. | CLAIM_DISCIPLINE, [ADVERSARIAL_REVIEW_CHECKLIST.md](ADVERSARIAL_REVIEW_CHECKLIST.md) (Physics rhetoric) |
| “`make bench` throughput is portable across machines.” | **Measured** class — always archive host + compiler metadata. | [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md) |
| “Arithmetic ratios depend on my GPU.” | **32× RAM** and **192× op proxy** follow from `D=4096` encodings — **not** GPU-dependent; wall-clock is. | README table footnotes, BENCHMARK_PROTOCOL |
| “Named labs endorsed this repo.” | **Illustrative public themes only** — no partnership claim. | [COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md](COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md) scope |
| “One C file is the entire production safety system for robots.” | Layered safety needs mechanics + control + **your** gates; σ receipts are one **audit** ingredient. | COHERENCE doc (Robotics section), ADVERSARIAL (Architecture attacks) |
| “We can skip `make check` if README only changed.” | **Run it anyway** before merge — numbers and prose drift together. | [CONTRIBUTING.md](../CONTRIBUTING.md) |
| “`creation_os_v6.c` reproduces COGITATE / IIT / named papers or replaces `make bench`.” | **Forbidden merge:** v6 is a **schematic** σ–K–L kernel with **internal** `self_test` checks — not a paper replication, not harness scores, not the 4096-bit BSC reference (`creation_os_v2.c` + `make test`). | [LIVING_KERNEL_V6.md](LIVING_KERNEL_V6.md), [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) |
| “`creation_os_v7.c` kills production hallucinations / vendor LM behavior.” | **Forbidden merge:** v7 adds toy σ readouts (anchors, association, bluff, context rot, JEPA–Oracle); `self_test` is internal consistency, not measured frontier hallucination rates. | [HALLUCINATION_KILLER_V7.md](HALLUCINATION_KILLER_V7.md), [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) |
| “`creation_os_v9.c` is a tape-out / RTL / measured TOPS/W proof.” | **Forbidden merge:** v9 adds **schematic** stack/silicon-shaped σ toys (M24–M29); numbers are placeholders unless separately validated against real silicon or benchmarks. | [PARAMETERS_IN_SILICON_V9.md](PARAMETERS_IN_SILICON_V9.md), [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) |
| “`creation_os_v10.c` *is* a distilled LM / AGI / measured MMLU proof.” | **Forbidden merge:** v10 adds closed-form **toys** (M30–M33); `self_test` checks internal consistency, not public benchmark rows or a trained model. | [THE_REAL_MIND_V10.md](THE_REAL_MIND_V10.md), [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) |
| “`creation_os_v11.c` *is* a trained matmul-free LM / BitNet reproduction / measured edge TOPS.” | **Forbidden merge:** v11 adds one **schematic** M34 path (ternary accum + MLGRU toy); 49 `self_test` checks are internal algebra, not published LM or silicon receipts. | [THE_MATMUL_FREE_MIND_V11.md](THE_MATMUL_FREE_MIND_V11.md), [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) |

---

*Spektre Labs · Creation OS · 2026*
