# v111.2 — POST /v1/reason

_The reasoning layer of the six-layer σ-stack, exposed as a single HTTP
endpoint that wires v62 + v64 + v45 + v101 together behind the v106
server._

## What it does

Given a user prompt the endpoint:

1. expands the prompt into `k_candidates` variants using the eight
   pre-registered prompt-stem perturbations in `cos_v111_reason_stem`
   (index 0 = identity, index 1 = "Think step by step, then …", etc.);
2. runs greedy generation per variant through the v101 bridge, capturing
   the full 8-channel σ profile and the scalar σ_product for each
   candidate;
3. selects the candidate with the lowest σ_product (most confident under
   the v105 default aggregator);
4. runs a short re-generation of the chosen candidate's prompt under
   stem 0 and compares the observed σ_product to the predicted one — a
   v45-style introspection waypoint check;
5. abstains whenever either (a) σ_product exceeds `tau_abstain`, or
   (b) the best-to-mean σ margin is below `min_margin` (the k-way vote
   carries no rank signal).

The pipeline adds **no new model weights**.  It composes existing v101
generations with multi-candidate σ-ranking plus an introspection probe.

## Request

```
POST /v1/reason
Content-Type: application/json

{
  "prompt":       "…",               # or "messages":[{"role":"user","content":"…"}]
  "k_candidates": 3,                  # 1..8,  default 3
  "max_tokens":   128,                # 1..2048, default 128
  "tau_abstain":  0.70                # override of server-config default
}
```

## Response — 200 OK

```json
{
  "object": "reasoning",
  "model":  "creation-os",
  "k_candidates": 3,
  "chosen_index": 2,
  "chosen_sigma_product": 0.087,
  "mean_sigma_product":   0.143,
  "margin":               0.056,
  "abstained": false,
  "abstain_reason": null,
  "waypoint_match": true,
  "waypoint_sigma_predicted": 0.087,
  "waypoint_sigma_observed":  0.091,
  "chosen": {
    "text": "…",
    "sigma_product": 0.087,
    "sigma_mean":    0.114,
    "sigma_profile": [..., ..., ..., ..., ..., ..., ..., ...],
    "stem_id": 2
  },
  "candidates": [
    {"text": "…", "sigma_product": 0.143, "sigma_mean": 0.161, "stem_id": 0},
    {"text": "…", "sigma_product": 0.188, "sigma_mean": 0.198, "stem_id": 1},
    {"text": "…", "sigma_product": 0.087, "sigma_mean": 0.114, "stem_id": 2}
  ]
}
```

### Abstain response — 200 OK with `abstained: true`

```json
{
  "object": "reasoning",
  "abstained": true,
  "abstain_reason": "sigma_product exceeded tau_abstain",
  "chosen_index": -1,
  ...
}
```

### Error responses

| code | reason                                                    | when                                    |
|------|-----------------------------------------------------------|-----------------------------------------|
| 400  | `missing prompt or messages`                              | neither `prompt` nor `messages` in body |
| 503  | `not_loaded`                                              | v106 is in stub mode                    |
| 500  | `reasoning pipeline failed` / `reason payload overflow`   | infrastructure error                    |

## How abstention is decided

Two independent gates:

1. **τ gate** — `chosen_sigma_product > tau_abstain` (default 0.70).
   This mirrors the v103 τ-sweep on which server-side defaults are
   calibrated.
2. **margin gate** — `mean_sigma_product − chosen_sigma_product <
   min_margin` (default 0.005).  If all k candidates agree within
   noise the σ signal carries no rank information, and the safer
   action is abstention with a diagnostic.

## Contract guarantees

- Deterministic given (bridge, prompt, opts).  No temperature sampling,
  no RNG, no wall-clock-dependent behaviour.
- σ profile values are the **same math** the v103/v104/v111.1 offline
  analysers consume.  A candidate's `sigma_product` is exact-equal to
  `benchmarks/v111/frontier_signals.op_sigma_product` over the same
  profile.
- The endpoint is always routed, even in stub mode: stub returns
  `503 not_loaded` rather than 404, so a front-end can probe the
  endpoint shape without a GGUF.
- The JSON response body never exceeds 64 KiB.  If the candidates are
  longer than fit, the server returns `500 reason payload overflow`
  (rather than truncating silently).

## Performance notes

- k_candidates × max_tokens forward passes (plus one short probe) per
  request.  On a 7 W M3 with BitNet 2B that is ≈ k × 0.3 s for a
  128-token response — default k=3 is ≈ 1 s end-to-end.
- Prompt-stem expansion is done in-process; no extra round-trip to the
  bridge.
- v101's `cos_v101_bridge_generate` is **not** thread-safe; v106 handles
  reasoning sequentially under the same single-threaded accept loop as
  `/v1/chat/completions`.

## CLI examples

```bash
# start the server (stub mode is fine for API exploration)
make standalone-v106 && ./creation_os_server --host 127.0.0.1 --port 8080

# happy path (requires real-mode build, see Makefile)
curl -s -X POST http://127.0.0.1:8080/v1/reason \
  -H 'Content-Type: application/json' \
  -d '{"prompt":"What is the capital of Finland?","k_candidates":3,"max_tokens":48}' \
  | jq .
```

## Self-test coverage

- `make check-v111-reason` — 25 pure-C self-test assertions + 5 loopback
  curl probes against the stub-mode server (400/503/204/404 contract).
- `make check-v111` — rolls up matrix + reason + SFT smoke.

## Relation to the other kernels

- `src/v62/` Energy-based path consistency: re-expressed here as a
  σ-margin threshold on k candidates (`min_margin`).  The dedicated
  v62 integer kernel remains the authoritative mathematical proof.
- `src/v64/` MCTS-style intellect: re-expressed here as argmin over the
  candidate σ_product (a single-level tree).  Multi-level tree search
  is tracked in `docs/ROADMAP_TO_AGI.md` as a future extension.
- `src/v45/` Introspection: re-expressed here as the waypoint-match
  check.  The dedicated v45 integer kernel remains the authoritative
  proof; this endpoint reports the match boolean without re-deriving it
  from integer hypervectors.
- `src/v101/` Bridge: used unchanged.  The endpoint is a pure consumer
  of `cos_v101_bridge_generate` and the v105 aggregator defaults.
