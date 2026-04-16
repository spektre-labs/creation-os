# Local OpenAI-shaped stub (protocol wiring)

This repository ships a **tiny loopback HTTP server** binary: `creation_os_openai_stub`.

## What it does (honest)

- **Loopback only** (`127.0.0.1`) — not a public internet server.
- **Endpoints:**
  - `GET /health` → `ok`
  - `GET /v1/models` → a one-entry model list (`creation-os-stub`)
  - `POST /v1/chat/completions` → parses a simplified Chat Completions JSON body (last `"role":"user"` message) and returns a **deterministic stub** assistant string (`CREATION_OS_OPENAI_STUB:…`).
  - `POST /v1/completions` → reads `"prompt"` / optional `"suffix"` and returns a **stub** FIM-shaped string (`CREATION_OS_OPENAI_STUB_FIM:…`).
- **Not implemented:** SSE streaming (`"stream":true` returns **501**).

## What it does *not* do

- It does **not** load BitNet / LLM weights.
- It does **not** replace Cursor, VS Code, or any cloud product — it is a **local protocol shim** for wiring tests.

## Build / self-test

```bash
make standalone-openai-stub
./creation_os_openai_stub --self-test
./creation_os_openai_stub --port 8080
```

## Continue.dev (example)

See [`vscode-extension/setup_continue.md`](../vscode-extension/setup_continue.md).

## Aider (example)

See [`vscode-extension/setup_aider.md`](../vscode-extension/setup_aider.md).

## One-liner bootstrap (no downloads)

```bash
./scripts/bootstrap_coding_agent.sh
```

That script prints orientation and **does not** download weights by default.
