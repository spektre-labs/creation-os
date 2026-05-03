# Supported paths (production-facing)

This document lists surfaces we expect integrators to use **today**, plus **evidence** and **non-claims**. It does not replace archived harness JSON or machine metadata (`docs/REPRO_BUNDLE_TEMPLATE.md`).

---

## Works today (Python package)

- **`pip install creation-os`** → `cos score` → σ + verdict (lite entropy probe by default).
- **`cos serve`** → FastAPI ASGI app → `/v1/score` and OpenAPI **`/docs`** (install `creation-os[serve]`).
- **`cos chat`** → σ-aware chat against any **OpenAI-compatible** `/v1` backend (install `creation-os[chat]`).
- **`@sigma_gated`** (`cos.integrations.decorator`) → wrap callables; get `SigmaResult` or dict telemetry.
- **L1 entropy probe** → **zero** optional Python dependencies for the default `SigmaGate()`.
- **LangChain**-style wiring → optional extra `creation-os[langchain]` (see `pyproject.toml`).

---

## Evidence (M-tier context — negatives mandatory)

| Item | Note |
|------|------|
| TruthfulQA MC | AUROC **0.982** (⚠ **benchmark saturated** since ~2024 — not a solo headline). |
| TriviaQA | AUROC **0.960** (bind to harness JSON + git SHA when publishing). |
| HaluEval QA | AUROC **0.514** — **probe fails** on this distribution; **always** disclose in tables. |
| SimpleQA / FACTS / FaithDial / multi-model host eval | **Pending** until archived run artifacts exist. |

**NOT AGI ACHIEVED.**

---

## Not claimed

- **AGI** or human-parity reasoning guarantees.
- **Multimodal** paths as calibrated safety proofs without a harness (many are **lab / heuristic**).
- **EU AI Act** or other compliance docs as legally validated filings (draft / outline tier only).
- **Single-number** hallucination rate from one benchmark alone.

For the full ladder and posting strategy see `docs/CLAIM_DISCIPLINE.md` and `docs/REDDIT_MACHINELEARNING_STRATEGY.md`.

---

*Spektre Labs · Creation OS · 2026*
