# BREAK IT (manual stress)

Adversarial prompt matrix: `tests/stress/break_it.csv`.

Run from the repository root (after `make cos` + `make cos-chat` so `./cos chat` exists):

```bash
bash scripts/real/run_break_it.sh
```

The harness invokes **`./cos chat`** when the `cos` binary is present and executable; otherwise it falls back to **`./cos-chat`** with the same flags (`--once --prompt … --multi-sigma --json`).

Outputs (overwritten each run):

- `benchmarks/break_it/results.jsonl`
- `benchmarks/break_it/SUMMARY.md`

Requires a reachable inference backend when not using the stub (see `COS_BITNET_SERVER_PORT`, `COS_BITNET_CHAT_MODEL`). Per-prompt timeout defaults to 120s (`BREAK_IT_TIMEOUT`).

`cos-chat --once --json` prints one JSON object on stdout (action, sigma, response, route, model, audit_id); logs go to stderr.
