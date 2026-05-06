<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
-->

# Examples

Runnable scripts aligned with `docs/QUICKSTART.md`. **English only** in committed prose.

## Run any example

```bash
pip install creation-os
python examples/01_basic_score.py
```

For chat, LangChain, or HTTP samples, install the matching extra from `pyproject.toml` (`[chat]`, `[langchain]`, `[serve]`, …).

## Index

| # | File | What it shows |
|---|------|---------------|
| 1 | `01_basic_score.py` | Minimal `SigmaGate().score(...)` |
| 2 | `02_cascade.py` | `score_cascade` — L1 always; L2–L6 when extras + tensors exist |
| 3 | `03_decorator.py` | `@sigma_gated` on a text-returning callable |
| 4 | `04_batch.py` | Score many prompt/response pairs in one script |
| 5 | `05_langchain.py` | LangChain-oriented sample (needs `[langchain]`) |
| 6 | `06_knowledge_graph.py` | σ-validated knowledge-graph lab path |
| 7 | `07_chat_local.py` | Local OpenAI-compatible chat + σ (needs `[chat]` + running server) |

## Next steps

- `docs/QUICKSTART.md` — install → score → MCP in one pass.
- `docs/ARCHITECTURE.md` — L1 vs cascade vs C core.
- `docs/CLAIM_DISCIPLINE.md` — positives **and** negatives (**HaluEval 0.514**); **NOT AGI ACHIEVED**.

---

*Spektre Labs · Creation OS · 2026*
