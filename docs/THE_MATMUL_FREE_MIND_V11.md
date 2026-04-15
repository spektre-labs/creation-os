# The MatMul-free mind — `creation_os_v11.c` (v11.0)

**Audience:** integrators comparing **v11** to **v10** ([THE_REAL_MIND_V10.md](THE_REAL_MIND_V10.md)), **v9** ([PARAMETERS_IN_SILICON_V9.md](PARAMETERS_IN_SILICON_V9.md)), **v7** ([HALLUCINATION_KILLER_V7.md](HALLUCINATION_KILLER_V7.md)), and **v6** ([LIVING_KERNEL_V6.md](LIVING_KERNEL_V6.md)).

**Evidence class:** same as prior standalone kernels — **Lab demo / schematic C** ([CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md)). The title is **narrative**; it does **not** claim measured parity with BitNet / Zhu et al. / production neuromorphic tape-out, or a trained matmul-free language model in this repository.

---

## What v11 adds on top of v10

`creation_os_v11.c` is **`creation_os_v10.c` plus one schematic channel** (M34):

| Id | Name | Toy output |
|----|------|------------|
| M34 | MatMul-free LM | Ternary `BitLinearLayer` (accumulate ±input / skip), element-wise `MLGRUState` recurrence, `mmf_forward` scalar readout; `sigma_matmul` fixed at **0** in this toy (no dense GEMM path in the schematic) |

**Genesis** wires M34 with **deterministic** dimensions so **`make check-v11`** (49 `self_test` checks: all v10 checks plus T47–T49) stays reproducible.

---

## Build

```bash
make check-v11
```

Same warning flags as v6–v10 for unused pedagogical statics.

---

## Non-claims

- **Not** a reproduction of published matmul-free LM throughput, memory, or accuracy tables — only closed-form scaffolding and internal tests.
- **Not** proof that “attention is replaced” in a trained transformer — the MLGRU block is a **toy** recurrence next to the existing attention demo in earlier sections of the same file.
- **Not** clinical or safety certification.

---

*Spektre Labs · Creation OS · 2026*
