<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
-->

# Creation OS — Quickstart

**Target:** under five minutes from `pip install` to a working **σ** score, chat hook, or MCP tool.

---

## 30 seconds: install and score

```bash
pip install creation-os
cos score --prompt "What is the capital of France?" --response "Paris"
```

Typical **lite** `SigmaGate()` output uses an entropy-style probe; **exact σ and verdict vary** with text and version. You should see a **σ** value and one of **ACCEPT**, **RETHINK**, or **ABSTAIN**.

A clearly wrong factual answer often lands in a higher-σ band (still not a guaranteed fact checker):

```bash
cos score --prompt "Who invented the telephone?" --response "Edison"
```

---

## Python API

```python
from cos import SigmaGate

gate = SigmaGate()
sigma, verdict = gate.score("What is 2+2?", "4")
print(f"σ={sigma:.3f} → {verdict}")
```

Default thresholds come from `cos.config.DEFAULT_CONFIG` (**threshold_accept=0.15**, **threshold_abstain=0.85**). Tune per deployment with `SigmaGate(threshold_accept=..., threshold_abstain=...)` or personas (see `cos.persona`).

To force an **ACCEPT** pedagogical example when lite σ sits just above the default accept band:

```python
gate = SigmaGate(threshold_accept=0.30, threshold_abstain=0.90)
sigma, verdict = gate.score("What is 2+2?", "4")
print(f"σ={sigma:.3f} → {verdict}")
```

---

## Protect any function

```python
from cos.integrations.decorator import sigma_gated

@sigma_gated
def my_llm(prompt):
    return call_my_model(prompt)

result = my_llm("What is quantum computing?")
# result.sigma, result.verdict, result.text
```

---

## Chat with a local model

```bash
pip install 'creation-os[chat]'
```

Start any **OpenAI-compatible** `/v1` server (vLLM, llama.cpp `llama-server`, SGLang, …). Example:

```bash
# Example model id — use one you have licensed and downloaded.
llama-server -hf unsloth/Qwen3.6-35B-A3B-GGUF:UD-Q4_K_XL --port 8001
```

Then:

```bash
cos chat --endpoint http://localhost:8001/v1
```

For Qwen3.6 thinking chains and σ on **assistant content only**, see `python/cos/chat.py` and your server’s `preserve_thinking` / chat-template options.

---

## MCP server (Claude Desktop, Cursor, VS Code)

```bash
pip install 'creation-os[mcp]'
cos mcp   # stdio — default for local MCP clients
```

**Claude Desktop** (`~/.claude/claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "sigma-gate": {
      "command": "cos",
      "args": ["mcp"]
    }
  }
}
```

**Streamable HTTP** (e.g. remote agents):

```bash
cos mcp --transport http --host 127.0.0.1 --port 8000
```

---

## API server

```bash
pip install 'creation-os[serve]'
cos serve --port 8000
```

Score a pair:

```bash
curl -s -X POST http://127.0.0.1:8000/v1/score \
  -H 'Content-Type: application/json' \
  -d '{"prompt":"Hello","response":"Hi"}'
```

(OpenAPI docs are served under `/docs` when enabled in your configuration.)

---

## What do the verdicts mean?

- **ACCEPT** — σ below **threshold_accept**; lower risk *for the configured probe* (not universal truth).
- **RETHINK** — σ between accept and abstain; verify before trusting downstream.
- **ABSTAIN** — σ at or above **threshold_abstain**; treat as unreliable for this probe.

---

## Next steps

- `examples/` — seven runnable scripts (`examples/README.md`).
- `docs/ARCHITECTURE.md` — probes, cascade, C core vs Python package.
- `docs/CLAIM_DISCIPLINE.md` — what we claim, mandatory negatives (**HaluEval 0.514**), **NOT AGI ACHIEVED**.

---

*Spektre Labs · Creation OS · 2026*
