# v166 — σ-Stream: real-time streaming with per-token σ

## Problem

`v106 σ-HTTP` is request-response: the user blocks until a whole
completion arrives.  A streaming chat UI wants partial tokens.
But *partial* tokens invite partial hallucinations: the model
has already shipped the beginning of a sentence before it
realizes it doesn't know how to finish.

## σ-innovation

- **Per-token σ on the wire.**  Every emitted token carries an
  8-channel σ vector + `sigma_product = geomean(channels)`.
  Colour the token on the UI live — green → yellow → red as σ
  climbs.

- **Interrupt-on-sigma.**  The stream stops itself the moment
  `sigma_product > τ_interrupt`.  A frame with
  `kind = "interrupted"` declares the trigger token and the
  reason (`"sigma_burst σ=… > τ=…"`).  The model stops mid-word
  if it has to.

- **Voice hook (v127 extension).**
  `audible_delay_ms = 40 + 400 · σ_product`.  When σ rises, the
  voice pipeline literally slows down.  Uncertainty becomes
  audible prosody.

- **NDJSON shape compatible with WebSocket frames.**  The v166.0
  CLI emits line-delimited JSON; v166.1 wraps the same frames
  in real `ws://` binary frames.

## Merge-gate

`benchmarks/v166/check_v166_stream_websocket.sh` asserts:

1. self-test
2. the stream opens with a `start` frame (seed + n_tokens) and
   closes with `complete` or `interrupted`
3. each token frame has `{kind, seq, token, sigma_product,
   channels[8], audible_delay_ms}`
4. `sigma_product ∈ [0,1]` and equals `geomean(channels[8])`
5. baseline run (τ=0.95) never interrupts
6. injected burst at seq=3 interrupts exactly at seq=3
7. `audible_delay_ms = 40 + ⌊400 · σ_product⌋`
8. byte-identical NDJSON across two runs with the same seed

## v166.0 vs v166.1

| Aspect | v166.0 (this release) | v166.1 (planned) |
|---|---|---|
| Transport | NDJSON to stdout | Real `ws://` server |
| Tokenizer | Whitespace + punctuation | GGUF vocab |
| Voice | `audible_delay_ms` hint | Actual TTS throttle in v127 |
| Generator | SplitMix64-keyed σ vector | σ out of the running decoder |

## Honest claims

- **Lab demo (C)**: deterministic per-token σ emitter and
  interrupt-on-sigma scheduler with a NDJSON wire format.
- **Not yet**: real WebSocket server, real decoder, TTS throttle.
- v166.0 freezes the wire protocol and the interrupt semantics;
  v166.1 plugs the real ports.
