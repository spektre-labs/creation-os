<div align="center">

# Creation OS

`"Coherence, made executable."`

</div>

[![Merge Gate](https://github.com/spektre-labs/creation-os/actions/workflows/merge-gate.yml/badge.svg)](https://github.com/spektre-labs/creation-os/actions/workflows/merge-gate.yml)
[![CI stack](https://github.com/spektre-labs/creation-os/actions/workflows/ci-stack.yml/badge.svg)](https://github.com/spektre-labs/creation-os/actions/workflows/ci-stack.yml)
[![License](https://img.shields.io/badge/license-SCSL--1.0%20OR%20AGPL--3.0--only-blue)](./LICENSE)

**One deterministic invariant — `σ = realized − declared` — carried as a measurement through every LLM call.**

Creation OS is a **σ-gate**: a model-free coherence kernel that scores any LLM output for
hallucination distance **without calling another model**, without cloud dependencies, without
guesswork. The theory is `1 = 1`. The code enforces it.

---

## The proven core

Two artifacts are the stable, tested surface of this repo. Everything else is research (see
[Exploratory tree](#exploratory-tree)).

### 1. `cos` — the Python σ-gate

Zero hard dependencies; the σ-gate runs on pure Python stdlib. Framework integrations are opt-in
extras.

```python
from cos import SigmaGate

gate = SigmaGate()
sigma, verdict = gate.score("What is 2+2?", "4")
# sigma: float ∈ [0, 1]  — distance from coherence
# verdict: "ACCEPT" | "RETHINK" | "ABSTAIN"
```

Pipeline — prompt → guardrails → σ → decision:

```python
from cos import Pipeline

pipe = Pipeline()
result = pipe.score("What is 2+2?", "4")
# result.sigma, result.verdict, result.text
# bool(result) → True iff ACCEPT
```

### 2. `creation_os_v6.c` — the C reference kernel

The canonical formalism in portable C11. Defines the coherence equations the Python package
implements:

```
K(t) = ρ · I_Φ · F          — cognitive capacity
K_eff = (1 − σ) · K         — effective capacity under incoherence
L = 1 − 2σ                  — Lagrangian kernel
S = ∫ L dt                  — action
1 = 1                        — invariant
```

```bash
clang -O2 -o creation_os_v6 creation_os_v6.c -lm
./creation_os_v6 --self-test
```

---

## Quickstart — install from source

> **Note:** Creation OS is **not currently published on PyPI**. Install from this repository.

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os
pip install .                        # installs the `cos` package (import: `cos`)
pip install '.[dev]'                 # + pytest, hypothesis, ruff
```

Requires Python ≥ 3.10. The σ-gate core has **zero hard runtime dependencies**.

**Verify the build** (the green badges above):

```bash
make merge-gate                      # builds creation_os + runs the v6…v26 --self-test chain
```

Optional integration extras (opt-in, pull third-party packages):

```bash
pip install '.[langchain]'           # LangChain callback
pip install '.[serve]'               # FastAPI /v1 server
pip install '.[chat]'                # OpenAI-compatible client wrapper
```

---

## σ — what it measures

σ is the distance between what a model declares and what is coherent. It is **not** a confidence
score; confidence is model-internal and unverifiable. σ is measured externally, deterministically,
on the output — no probe model required in lite mode.

- `σ = 0` — declared equals realized. Coherent.
- `σ = 1` — maximum incoherence. Abstain.

The gate emits `ACCEPT / RETHINK / ABSTAIN`. Downstream logic routes on verdict, not on float
intuition.

## Integrations

| Integration | Import | Extra |
|---|---|---|
| LangChain callback | `cos.integrations.langchain.SigmaGateCallback` | `[langchain]` |
| OpenAI-style wrapper | `cos.integrations.openai_wrapper.sigma_chat` | `[chat]` |
| FastAPI server | `cos serve` | `[serve]` |
| MCP server | `cos.mcp_server` | `[mcp]` |

---

## What it is / is not

- **Is:** a deterministic, model-free σ-gate (Python `cos` + C reference kernel) that scores LLM
  output for coherence distance with no second model and no network. The `merge-gate` / `CI stack`
  workflows run green and exercise the C self-test chain.
- **Is not:** a published PyPI package (install from source), nor a finished product across its
  entire tree. Several auxiliary workflows (mutation testing, the full Python lab) are **not
  green** — only the proven core gates above are. The `src/` research tree is explicitly *not* a
  stable API.

## Exploratory tree

`src/` contains the iterative kernel research tree — v31 through v306 subdirectories, the C
σ-channel library, and experimental subsystems (embodiment, swarm, federation, living weights,
symbolic reasoning, speculative decode). These are **research artifacts**: not packaged, not a
stable API, **not claimed as shipped product**. The shipped, stable surface is `python/cos/`
(plus `creation_os_v6.c`). When in doubt, the proven core above is the whole product; the rest is
exploration kept on purpose.

## Sibling estate

- [**spektre-protocol**](https://github.com/spektre-labs/spektre-protocol) — the state-first protocol canon (`1 = 1`); vendors a copy of this kernel.
- [**corpus**](https://github.com/spektre-labs/corpus) — the open-access research archive behind σ.
- [**railo-fabric**](https://github.com/spektre-labs/railo-fabric) · [**railo-stdlib-tools**](https://github.com/spektre-labs/railo-stdlib-tools) — the stdlib-only tooling estate.

## License

SCSL-1.0 (commercial) **or** AGPL-3.0-only (open source). See `LICENSE`, `LICENSE-SCSL-1.0.md`,
`LICENSE-AGPL-3.0.txt`. Commercial: `spektre.labs@proton.me`

---

`1 = 1` — **Spektre Labs · Helsinki · 2026**
