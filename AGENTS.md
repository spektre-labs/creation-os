# Agent instructions — Creation OS (`creation-os`)

Guidance for autonomous coding tools (Cursor, Copilot, etc.) working **in this tree**.

## Language

- **Canonical policy:** [docs/LANGUAGE_POLICY.md](docs/LANGUAGE_POLICY.md) — **English only** for all committed prose and new maintainer-facing comments.
- Do not introduce non-English technical documentation into tracked markdown or source.

## Evidence hygiene (non-negotiable)

1. Read [docs/CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md) before editing benchmark prose or numbers; for thesis-level scope and non-goals read [docs/RESEARCH_AND_THESIS_ARCHITECTURE.md](docs/RESEARCH_AND_THESIS_ARCHITECTURE.md).
2. Never merge **microbench throughput** with **harness MMLU / ARC** in a single headline sentence.
3. **Arithmetic ratios** (192× ops, 32× RAM) in the README follow from the stated `D=4096` encodings; **throughput** requires `make bench` + archived host metadata ([docs/REPRO_BUNDLE_TEMPLATE.md](docs/REPRO_BUNDLE_TEMPLATE.md)).

## What to optimize for

- **Correctness:** `make test` must pass. If you edit [`creation_os_v6.c`](../creation_os_v6.c) or [`creation_os_v7.c`](../creation_os_v7.c), also run **`make check-v6`** / **`make check-v7`** respectively (Living Kernel vs Hallucination Killer self-tests).
- **Portability:** Prefer C11 + libm; optional `-march=native` is Makefile default — document if you change it.
- **Minimal diffs:** Do not refactor unrelated modules; match existing style.

## Navigation

- Full doc map: [docs/DOC_INDEX.md](docs/DOC_INDEX.md)
- Contributor workflow: [CONTRIBUTING.md](CONTRIBUTING.md)
- External “superiority” claims must cite [docs/HDC_VSA_ENGINEERING_SUPERIORITY.md](docs/HDC_VSA_ENGINEERING_SUPERIORITY.md) or [docs/EXTERNAL_EVIDENCE_AND_POSITIONING.md](docs/EXTERNAL_EVIDENCE_AND_POSITIONING.md) — never invent paper outcomes not stated there.
- **Scope:** treat this checkout as **Creation OS only**; kernel PRs and measured claims live here. **Publishing:** [docs/publish_checklist_creation_os.md](docs/publish_checklist_creation_os.md).

---

*Spektre Labs · Creation OS · 2026*
