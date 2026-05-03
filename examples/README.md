# Examples — Creation OS

Runnable scripts that mirror `docs/QUICKSTART.md`. **English only** in committed prose.

## Run any example

```bash
pip install creation-os
python examples/01_basic_score.py
```

For chat, cascade, or LangChain samples, install the matching extra from `pyproject.toml` (`[chat]`, `[serve]`, `[langchain]`, …).

## Index

| # | File | What it shows |
|---|------|---------------|
| 1 | `01_basic_score.py` | Minimal `SigmaGate().score(...)` |
| 2 | `02_cascade.py` | `score_cascade` — L1 always; L2–L6 when extras + tensors exist |
| 3 | `03_decorator.py` | `@sigma_gated` on any text-returning callable |
| 4 | `04_batch.py` | Score many prompt/response pairs in one script |
| 5 | `05_langchain.py` | LangChain-oriented integration (needs `[langchain]`) |
| 6 | `06_knowledge_graph.py` | σ-validated knowledge-graph lab path |
| 7 | `07_chat_local.py` | Local OpenAI-compatible chat + σ (needs `[chat]` + running server) |

## Next steps

- `docs/QUICKSTART.md` — copy-paste paths in under five minutes.
- `docs/ARCHITECTURE.md` — L1 vs cascade vs probes.
- `docs/CLAIM_DISCIPLINE.md` — what we claim / do not claim (**HaluEval 0.514** always on the ladder).

---

*Spektre Labs · Creation OS · 2026*
