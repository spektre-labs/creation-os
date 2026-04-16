# v49 certification pack (lab artifacts)

This directory contains **DO-178C-aligned documentation and scripts** for the Creation OS **σ-critical path** (v47 kernel surface + v49 abstention gate).

**This is not FAA/EASA (or any authority) certification.** It is an **in-repo assurance pack**: plans, requirements, trace hooks, coverage runs, and optional formal tooling (Frama-C / SymbiYosys) where installed.

Start here:

- Requirements: [`HLR.md`](HLR.md) · [`LLR.md`](LLR.md)
- Traceability: [`traceability_matrix.md`](traceability_matrix.md) · [`trace_manifest.json`](trace_manifest.json)
- Coverage: [`structural_coverage.md`](structural_coverage.md) · generated summaries in [`coverage/`](coverage/)
- Formal methods posture: [`formal_methods_report.md`](formal_methods_report.md)
- Plans: [`PSAC.md`](PSAC.md) · [`SDP.md`](SDP.md) · [`SVP.md`](SVP.md) · [`SCMP.md`](SCMP.md) · [`SQAP.md`](SQAP.md) · [`SDD.md`](SDD.md)
- Assurance ladder: [`ASSURANCE_HIERARCHY.md`](ASSURANCE_HIERARCHY.md)

Run (from repo root): `make certify`
