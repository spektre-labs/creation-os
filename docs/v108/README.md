# v108 — σ-UI (web/index.html)

Single-file chat interface with live σ-channel visualisation.
No build system, no npm, no framework — pure vanilla HTML5 + JS.

## What it shows

- a chat log (user messages right, model messages left)
- the **eight σ-channels** of the last answer as a sidebar of live
  bars: `entropy`, `mean`, `max`, `product`, `tail`, `n_eff`,
  `margin`, `stab`
- a summary tile: `σ_product`, `σ_mean`, `σ_max_token`, `abstained`
- per-answer badges: aggregator, σ value, abstention flag, latency

The UI polls [`/v1/sigma-profile`](../../src/v106/server.c) every
three seconds; on each completion it also updates immediately from
the `sigma*` fields in the response.

## How it is served

`web/index.html` is copied into the image / installed tree.  The
v106 σ-server resolves `GET /` to `<web_root>/index.html`
([`src/v106/server.c`](../../src/v106/server.c)), so:

```bash
creation_os_server --gguf $HOME/.creation-os/models/bitnet-b1.58-2B-4T-Q4_K_M.gguf
# then open http://127.0.0.1:8080
```

The same static root is shipped in the Docker image at
`/usr/local/share/creation-os/web`, mounted via the
`COS_V106_WEB_ROOT` env var.

## Smoke gate

`web/check_v108_ui_renders.sh` validates, offline:

- HTML5 shape (`<!doctype html>`, `<html>`, `<body>`)
- ≥ 6 of the 8 canonical σ-channel labels present
- references `/v1/chat/completions`, `/v1/sigma-profile`,
  `/v1/models`
- σ_product summary + abstain indicator present
- if `creation_os_server` is built: `GET /` returns 200 with our
  markup (live render on loopback)

Merge-gate entry point:

```bash
make check-v108-ui-renders
```

## Claims

- **Local-only.** No external network calls; no telemetry; no CDN
  fonts.  All styling is inlined.  Offline-capable after first load.
- **Zero build.** Ship-and-serve.  No transpile, no bundler, no node
  modules.  Every byte in `web/index.html` is the byte the browser
  runs.
- **Honest display.**  If `/v1/sigma-profile` reports `has_data:false`
  (no inference run yet), the bars stay at zero — we never fabricate
  a live σ.

## Not (yet) shipped

- streaming token render (non-stream JSON works today; SSE path
  lives in the server but the UI renders only after the final JSON)
- multi-turn persistence across reloads (the chat log is in-memory)
- settings panel (τ, aggregator, model picker) — deferred to v112
