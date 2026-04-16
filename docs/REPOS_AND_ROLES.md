# Repositories, roles, and how they meet in Creation OS

**Hard rule for this product line:** all **kernel / portable-tree / CI** work is **committed and pushed only to**  
**[spektre-labs/creation-os](https://github.com/spektre-labs/creation-os)** (`main`).  
Do not land Creation OS kernel changes on protocol, corpus, or Genesis remotes by mistake.

The other repositories remain **active parts of the same intellectual project** — different surfaces, different evidence classes.

---

## Four repositories (plus Zenodo)

| Repository | What it is | What you push there | How Creation OS uses it |
|--------------|-------------|------------------------|-------------------------|
| **[spektre-labs/creation-os](https://github.com/spektre-labs/creation-os)** | Portable **C11** kernel, standalones, benchmarks, **merge-gate** CI, engineering docs | **All** Creation OS code + kernel docs | **Source of truth** for anything that `make check` / `merge-gate` proves |
| **[spektre-labs/spektre-protocol](https://github.com/spektre-labs/spektre-protocol)** | **Spektre v1.1** layered archive: `protocol_core/`, `formal_structure/`, `execution_system/`, essays, … | **Only** when editing that archive (prose, canons, formal notes) | **Conceptual spine**: invariants, ordering (state before interpretation), agency language. In-tree echo: [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md), [ANALYSIS.md](ANALYSIS.md), paradigm docs — **not** a second place to push the `creation_os_v*.c` tree |
| **[spektre-labs/corpus](https://github.com/spektre-labs/corpus)** | **Open theory corpus** — papers, CC BY 4.0, Zenodo DOIs, README index | Paper metadata, PDFs, bib updates | **Bibliographic** home cited from [ANALYSIS.md](ANALYSIS.md), [EXTERNAL_EVIDENCE_AND_POSITIONING.md](EXTERNAL_EVIDENCE_AND_POSITIONING.md), [CITATION.bib](CITATION.bib). Not the CI surface for the C kernel |
| **[lauri-elias-rainio/creation-os](https://github.com/lauri-elias-rainio/creation-os)** | Author **Spektri-Genesis** substrate — polyglot runtime (TS, C++, SV, …), UI, scripts, broader “execution” experiments | Separate clone / workflow when working **that** tree | **Narrative + gateway alignment** only in this portable repo: see [CANONICAL_GIT_REPOSITORY.md](CANONICAL_GIT_REPOSITORY.md) and `core/cos_genesis_identity.h` (identity string latches for edge integration). No requirement to vendor the whole Genesis monorepo here |

**Zenodo:** [Spektre Labs community](https://zenodo.org/communities/spektre-labs/) — archived papers and bundles; still **not** a `git push` target for kernel code.

---

## Unified “oivallukset” → where they live

| Theme | Protocol / Genesis / Corpus angle | Creation OS home (evidence) |
|--------|-----------------------------------|----------------------------|
| **1 = 1** / non-contradiction | Canon + README invariant | Self-tests in standalones; stated in README + [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) |
| **State before interpretation** | `STATE_BEFORE_INTERPRETATION` ordering | Plane A / retrieval-first story in [ANALYSIS.md](ANALYSIS.md); abstention gates in module narratives |
| **Commit vs simulation** | `STATE_COMMIT`, execution layer prose | Claim discipline: harness vs lab demo vs frontier rows |
| **Formal limits** (halting, Gödel, NFL, oracle) | `SPEKTRE_FORMAL_LIMITS.md` | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) — no merged evidence classes; no “total safety proof” from demos |
| **Formal stack step** (no skip without receipt) | `FORMALISM.md` iterated `X_{k+1}` | `rtl/cos_formal_iron_combo.sv` (`stack_step_ok`, `level_tag`) + [RTL_SILICON_MIRROR.md](RTL_SILICON_MIRROR.md) |
| **Corpus / citations** | corpus repo + DOIs | [ANALYSIS.md](ANALYSIS.md), [EXTERNAL_EVIDENCE_AND_POSITIONING.md](EXTERNAL_EVIDENCE_AND_POSITIONING.md), [CITATION.bib](CITATION.bib) |
| **Genesis identity / ledger language** | Lauri README (`X-Identity`, MasterLedger) | `core/cos_genesis_identity.h` — constants only; gateway code stays in Genesis tree until you explicitly bridge |
| **Control matrix (execute / hold / stabilize)** | Execution-system control prose | `rtl/cos_agency_iron_combo.sv` + [RTL_SILICON_MIRROR.md](RTL_SILICON_MIRROR.md) |
| **Clock-domain / async tool boundary** | Heterogeneous dispatch story | `rtl/cos_boundary_sync.sv` — 2-flop sampler for single-bit cross into kernel `clk` |

---

## Monorepo checkouts

If your disk layout is **umbrella** (e.g. protocol + `creation-os/` subfolder): **set `git remote` and `cwd` deliberately** before `git push`. Wrong remote = wrong history on the wrong GitHub object — see [CANONICAL_GIT_REPOSITORY.md](CANONICAL_GIT_REPOSITORY.md).

---

## Spektre Labs distribution note (triad)

The three-lab triad (creation-os / protocol / corpus) is also summarized under the umbrella docs repo as **distribution and remotes** — keep that document aligned with **this** file; **this** file is the copy **shipped with the kernel** and is the one agents should read first.

---

## Silicon RTL mirror (extra “oivallukset”)

Where prose and C stop, **SystemVerilog** and **Chisel** can still enforce the same **shape**: formal stack discipline, agency control triad, commit channel separation, and metastability-safe sampling at boundaries. Module map, build targets, and CI hook: **[RTL_SILICON_MIRROR.md](RTL_SILICON_MIRROR.md)** (`make formal-rtl-lint`); typed generator path: **`hw/chisel/`** (`make chisel-verilog`).

---

*Spektre Labs · Creation OS · 2026*
