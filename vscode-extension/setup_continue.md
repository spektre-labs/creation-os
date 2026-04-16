# Continue.dev → local Creation OS stub (example)

This wires **Continue** to the in-repo **OpenAI-shaped stub** (`creation_os_openai_stub`) for **local protocol smoke**.

## 1) Run the stub

From the repository root:

```bash
make standalone-openai-stub
./creation_os_openai_stub --port 8080
```

## 2) Configure Continue

Point Continue at the stub’s **OpenAI-compatible base URL** (note: many clients append `/v1` themselves — if yours does, set `apiBase` to `http://127.0.0.1:8080` **without** `/v1`; if not, include `/v1`).

Example `config.json` fragment:

```json
{
  "models": [
    {
      "title": "Creation OS stub",
      "provider": "openai",
      "model": "creation-os-stub",
      "apiBase": "http://127.0.0.1:8080",
      "apiKey": "none"
    }
  ]
}
```

## Limitations

- **No streaming** (`stream:true` is rejected with HTTP 501 from this stub).
- **No real LM** — responses are deterministic stubs; see [docs/LOCAL_OPENAI_STUB.md](../docs/LOCAL_OPENAI_STUB.md).
