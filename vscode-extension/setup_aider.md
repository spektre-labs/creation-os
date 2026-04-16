# Aider → local Creation OS stub (example)

This is an **example** of pointing **Aider** at the in-repo OpenAI-shaped stub for wiring tests.

## 1) Run the stub

```bash
make standalone-openai-stub
./creation_os_openai_stub --port 8080
```

## 2) Point Aider at the stub

Exact flags vary by Aider version; the intent is:

```bash
export OPENAI_API_BASE=http://127.0.0.1:8080/v1
export OPENAI_API_KEY=none
```

Then run Aider with a model id your stub advertises (`creation-os-stub` from `GET /v1/models`).

## Limitations

- This stub does **not** run a real code LM.
- Streaming may not be supported (stub returns **501** for `stream:true` chat requests).

See [docs/LOCAL_OPENAI_STUB.md](../docs/LOCAL_OPENAI_STUB.md).
