# Draft (do not post until `FINAL_RESULTS.md` is no longer STUB) — r/MachineLearning

**Title:** “σ-gated inference: measuring `accuracy_answered` vs `accuracy_total` on small models (with an assurance pack)”

We are benchmarking a small model stack with an uncertainty-gated abstention layer (σ-gating) across standard suites **once the harness is pinned** (TruthfulQA / MMLU-mini / ARC / HellaSwag / Winogrande slots exist in `make v50-benchmark`).

**Planned headline (requires real numbers):**

- `accuracy_answered` (accuracy on items the model chooses to answer) vs `accuracy_total`
- abstention rate, calibration gap, σ overhead (ms / %)

**Assurance (repo-native, not ‘trust us’):**

- `make certify` (Frama-C / SymbiYosys targets + coverage + audit + trace + red-team hooks; SKIPs explicit)
- v48 red-team catalog + v49 certification pack

**Links (canonical):**

- Code: `https://github.com/spektre-labs/creation-os`
- Results (generated): `benchmarks/v50/FINAL_RESULTS.md`
- Repro (today): `make v50-benchmark`

**Not claiming frontier raw accuracy.** Claiming: selective reliability metrics + explicit abstention + verifiable engineering artifacts — once measurements land.
