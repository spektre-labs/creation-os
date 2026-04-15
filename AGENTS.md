# Agent instructions — Creation OS (`creation-os`)

Guidance for autonomous coding tools (Cursor, Copilot, etc.) working **in this tree**.

## Canonical Git (non-negotiable)

- **Only** [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) on GitHub is the real Creation OS remote. Read **[docs/CANONICAL_GIT_REPOSITORY.md](docs/CANONICAL_GIT_REPOSITORY.md)**.
- **Never** add that URL as `origin` on a parent “protocol” / umbrella monorepo, and **never** `git push` Creation OS from outside the Creation OS repo root. Use a normal clone of `creation-os` or `make publish-github` from **this** tree’s root (`creation_os_v2.c` present).

## Language

- **Canonical policy:** [docs/LANGUAGE_POLICY.md](docs/LANGUAGE_POLICY.md) — **English only** for all committed prose and new maintainer-facing comments.
- Do not introduce non-English technical documentation into tracked markdown or source.

## Evidence hygiene (non-negotiable)

1. Read [docs/CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md) before editing benchmark prose or numbers; for thesis-level scope and non-goals read [docs/RESEARCH_AND_THESIS_ARCHITECTURE.md](docs/RESEARCH_AND_THESIS_ARCHITECTURE.md). When editing dissertation-facing prose, follow the ordered **doctoral read path** in [README.md](README.md#doctoral-and-committee-read-path) so v6–v12 stay **Lab demo (C)** and never substitute for harness, silicon, or quantum claims.
2. Never merge **microbench throughput** with **harness MMLU / ARC** in a single headline sentence.
3. **Arithmetic ratios** (192× ops, 32× RAM) in the README follow from the stated `D=4096` encodings; **throughput** requires `make bench` + archived host metadata ([docs/REPRO_BUNDLE_TEMPLATE.md](docs/REPRO_BUNDLE_TEMPLATE.md)).

## What to optimize for

- **Correctness:** `make test` must pass. If you edit [`creation_os_v6.c`](creation_os_v6.c), [`creation_os_v7.c`](creation_os_v7.c), [`creation_os_v9.c`](creation_os_v9.c), [`creation_os_v10.c`](creation_os_v10.c), [`creation_os_v11.c`](creation_os_v11.c), or [`creation_os_v12.c`](creation_os_v12.c), also run **`make check-v6`**, **`make check-v7`**, **`make check-v9`**, **`make check-v10`**, **`make check-v11`**, or **`make check-v12`** respectively (Living Kernel through Tensor mind self-tests).
- **Portability:** Prefer C11 + libm; optional `-march=native` is Makefile default — document if you change it.
- **Minimal diffs:** Do not refactor unrelated modules; match existing style.

## Navigation

- Full doc map: [docs/DOC_INDEX.md](docs/DOC_INDEX.md)
- Contributor workflow: [CONTRIBUTING.md](CONTRIBUTING.md)
- External “superiority” claims must cite [docs/HDC_VSA_ENGINEERING_SUPERIORITY.md](docs/HDC_VSA_ENGINEERING_SUPERIORITY.md) or [docs/EXTERNAL_EVIDENCE_AND_POSITIONING.md](docs/EXTERNAL_EVIDENCE_AND_POSITIONING.md) — never invent paper outcomes not stated there.
- **Scope:** treat this checkout as **Creation OS only**; kernel PRs and measured claims live here. **Publishing:** [docs/publish_checklist_creation_os.md](docs/publish_checklist_creation_os.md).

---

*Spektre Labs · Creation OS · 2026*
