<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
-->

# Running `cos chat` with a local model

This guide assumes **`pip install 'creation-os[chat]'`** (optional **OpenAI** client). The σ-gate itself has **zero** core Python dependencies; scoring still runs locally on each assistant reply.

Use any **OpenAI-compatible** HTTPS server that exposes `POST /v1/chat/completions` (same JSON shape as the OpenAI API).

---

## Option A: llama.cpp (recommended)

Install the `llama-server` binary (package name varies by platform):

```bash
# macOS (Homebrew)
brew install llama.cpp

# or build from source: https://github.com/ggml-org/llama.cpp
```

Start the server with a **Qwen3.6-class** GGUF (example Hugging Face repo tag — use a model you are licensed to run):

```bash
llama-server \
  -hf unsloth/Qwen3.6-35B-A3B-GGUF:UD-Q4_K_XL \
  --port 8001 \
  --chat-template-kwargs '{"preserve_thinking": true}'
```

Optional sampling (aligned with Qwen3.6 hints):

```bash
llama-server \
  -hf unsloth/Qwen3.6-35B-A3B-GGUF:UD-Q4_K_XL \
  --port 8001 \
  --temp 0.7 --top-p 0.8 --top-k 20 \
  --chat-template-kwargs '{"preserve_thinking": true}'
```

**Chat with σ-gate** (defaults to `http://localhost:8000/v1` if you omit `--endpoint`; match your server port):

```bash
pip install 'creation-os[chat]'
cos chat --endpoint http://localhost:8001/v1
```

Or run **`python examples/07_chat_local.py`** after installing `[chat]`.

---

## Option B: Ollama

```bash
ollama serve
ollama run qwen3:30b-a3b
```

```bash
pip install 'creation-os[chat]'
cos chat --endpoint http://localhost:11434/v1 --model qwen3:30b-a3b
```

Use the exact **model name** Ollama reports for your pull (tags differ by registry).

---

## Option C: LM Studio

Download from [lmstudio.ai](https://lmstudio.ai). Load a model, enable the **local server** (default port is often **1234**).

```bash
pip install 'creation-os[chat]'
cos chat --endpoint http://localhost:1234/v1
```

---

## Option D: Any OpenAI-compatible cloud API

```bash
pip install 'creation-os[chat]'
export OPENAI_API_KEY=...   # or COS_API_KEY
cos chat --endpoint https://api.openai.com/v1 --model gpt-4o
```

---

## Troubleshooting

- **`ImportError: openai`** — install **`creation-os[chat]`** (or **`[openai]`**).
- **Connection errors** — confirm the base URL ends with **`/v1`** (no trailing slash required in Creation OS; the client normalizes).
- **Thinking / Qwen3.6** — enable **`preserve_thinking`** on the **server** (e.g. `chat-template-kwargs`); Creation OS strips thinking for display and runs **σ only on assistant content** (`python/cos/chat.py`).

---

## See also

- `docs/QUICKSTART.md` — short install path.
- `examples/07_chat_local.py` — minimal REPL wired to `SigmaChat`.

---

*Spektre Labs · Creation OS · 2026*
