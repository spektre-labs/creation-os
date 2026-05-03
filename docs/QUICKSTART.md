# Creation OS — Quickstart (< 5 minutes)

**Goal:** from `pip install` to a real **σ** score and verdict on your machine.

---

## Install

```bash
pip install creation-os
```

Optional extras: `pip install 'creation-os[chat]'`, `'creation-os[serve]'`, `'creation-os[langchain]'` (see `pyproject.toml`).

---

## Score your first response (CLI)

```bash
cos score --prompt "What is the capital of France?" --response "Paris"
```

Typical **lite** output (entropy probe; numbers vary slightly by text and version):

`σ≈0.18 ACCEPT`

A **middle-band** example (repetitive answer → higher σ with default thresholds):

```bash
cos score --prompt "Name any color." --response "red red red red red red"
```

Typical output: `σ≈0.31 RETHINK`

> **Note:** Default `SigmaGate()` is an **entropy / statistics** probe, not a fact checker. Wrong-but-fluent answers can still score low σ. For probe bundles and harness metrics see `docs/CLAIM_DISCIPLINE.md`.

---

## Python API

```python
from cos import SigmaGate

gate = SigmaGate()
sigma, verdict = gate.score("What is 2+2?", "4")
print(f"σ={sigma:.3f} → {verdict}")
```

Typical lite output: `σ≈0.25 → ACCEPT` (exact value depends on the prompt+response text).

---

## Chat (OpenAI-compatible server)

Start any **OpenAI-compatible** `/v1` server (vLLM, llama.cpp `llama-server`, SGLang, …). Example (your model path will differ):

```bash
# Example only — pick a model you have licensed and downloaded.
llama-server -hf unsloth/Qwen3.6-35B-A3B-GGUF:UD-Q4_K_XL --port 8001
```

Then:

```bash
pip install 'creation-os[chat]'
cos chat --endpoint http://127.0.0.1:8001/v1
```

---

## Protect any Python function

```python
from cos.integrations.decorator import sigma_gated

@sigma_gated
def my_llm(prompt: str) -> str:
    return call_my_model(prompt)

result = my_llm("What is quantum computing?")
print(f"σ={result.sigma:.3f} {result.verdict}: {result.text}")
```

---

## HTTP API server (`cos serve`)

```bash
pip install 'creation-os[serve]'
cos serve --port 8420
```

Score endpoint (OpenAPI at `/docs`), e.g.:

`POST http://127.0.0.1:8420/v1/score` with JSON `{"prompt":"...","response":"..."}`.

---

## What the verdicts mean

| Verdict   | Meaning (operator-facing) |
|-----------|---------------------------|
| **ACCEPT**  | σ below the accept threshold — treat as lower-risk for the configured probe. |
| **RETHINK** | σ in the middle band — verify before trusting downstream. |
| **ABSTAIN** | σ above the abstain threshold — treat as unreliable for the configured probe. |

Thresholds default to τ_accept=0.3 and τ_abstain=0.7 unless you pass `SigmaGate(tau_accept=..., tau_abstain=...)`.

---

## Next steps

- `examples/` — runnable scripts (see `examples/README.md`).
- `docs/ARCHITECTURE.md` — how σ-gate is structured (L1 vs cascade vs probe).
- `docs/CLAIM_DISCIPLINE.md` — what we claim, what we do **not** claim, and the evidence ladder (**NOT AGI ACHIEVED**).

---

*Spektre Labs · Creation OS · 2026*
