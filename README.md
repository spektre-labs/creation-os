[![PyPI](https://img.shields.io/pypi/v/creation-os)](https://pypi.org/project/creation-os/)
[![CI](https://github.com/spektre-labs/creation-os/actions/workflows/ci.yml/badge.svg)](https://github.com/spektre-labs/creation-os/actions/workflows/ci.yml)
[![Merge Gate](https://github.com/spektre-labs/creation-os/actions/workflows/merge-gate.yml/badge.svg)](https://github.com/spektre-labs/creation-os/actions/workflows/merge-gate.yml)
[![License](https://img.shields.io/badge/license-SCSL--1.0%20OR%20AGPL--3.0--only-blue)](./LICENSE)

# Creation OS

`"Coherence, made executable."`

**One deterministic invariant — `σ = realized − declared` — carried as a measurement through every LLM call.**

Creation OS is a **σ-gate**: a model-free coherence kernel that scores any LLM output for hallucination distance without calling another model, without cloud dependencies, without guesswork. The theory is `1 = 1`. The code enforces it.

---

## What ships

**`cos` — the Python package** (`pip install creation-os`)

Zero hard dependencies. The σ-gate runs on pure Python stdlib. Framework integrations are opt-in extras.

```python
from cos import SigmaGate

gate = SigmaGate()
sigma, verdict = gate.score("What is 2+2?", "4")
# sigma: float ∈ [0, 1]  — distance from coherence
# verdict: "ACCEPT" | "RETHINK" | "ABSTAIN"
```

**Pipeline** — prompt → guardrails → σ → decision:

```python
from cos import Pipeline

pipe = Pipeline()
result = pipe.score("What is 2+2?", "4")
# result.sigma, result.verdict, result.text
# bool(result) → True iff ACCEPT
```

**Extras:**

```bash
pip install 'creation-os[langchain]'   # LangChain callback
pip install 'creation-os[serve]'       # FastAPI /v1 server
pip install 'creation-os[probes]'      # LSD hidden-state probe (requires torch)
pip install 'creation-os[chat]'        # SigmaChat OpenAI-compatible client wrapper
```

**`creation_os_v6.c` — the C reference kernel**

The canonical formalism in portable C. Defines the coherence equations the Python package implements:

```
K(t) = ρ · I_Φ · F          — cognitive capacity
K_eff = (1 − σ) · K         — effective capacity under incoherence
L = 1 − 2σ                  — Lagrangian kernel
S = ∫ L dt                  — action
1 = 1                        — invariant
```

Compile: `clang -O2 -o creation_os_v6 creation_os_v6.c -lm`
Self-test: `./creation_os_v6 --self-test`

---

## σ — what it measures

σ is the distance between what a model declares and what is coherent. It is not a confidence score; confidence is model-internal and unverifiable. σ is measured externally, deterministically, on the output — no probe model required in lite mode.

`σ = 0` — declared equals realized. Coherent.
`σ = 1` — maximum incoherence. Abstain.

The gate emits `ACCEPT / RETHINK / ABSTAIN`. Downstream logic routes on verdict, not on float intuition.

---

## Integrations

| Integration | Import | Extra |
|---|---|---|
| LangChain callback | `cos.integrations.langchain.SigmaGateCallback` | `[langchain]` |
| OpenAI-style wrapper | `cos.integrations.openai_wrapper.sigma_chat` | `[chat]` |
| FastAPI server | `cos serve` | `[serve]` |
| MCP server | `cos.mcp_server` | `[mcp]` |

---

## Install

```bash
pip install creation-os          # zero-dep core
pip install 'creation-os[dev]'   # + pytest, hypothesis, ruff
```

Requires Python ≥ 3.10. PyPI: [`creation-os`](https://pypi.org/project/creation-os/).

---

## Research / experiments

`src/` contains the iterative kernel research tree — v31 through v306 subdirectories, the C σ-channel library, and experimental subsystems (embodiment, swarm, federation, living weights, symbolic reasoning, speculative decode). These are **research artifacts**: not packaged, not stable API, not claimed as shipped product.

`python/cos/` contains the shipped package. That is the stable surface.

---

## License

SCSL-1.0 (commercial) or AGPL-3.0-only (open source). See `LICENSE`, `LICENSE-SCSL-1.0.md`, `LICENSE-AGPL-3.0.txt`.

Commercial: `spektre.labs@proton.me`

---

`1 = 1` — **Spektre Labs · Helsinki · 2026**
