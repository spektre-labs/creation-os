# Suite lab (optional local demo)

Small, **honest** pieces for experimenting with “modes” and browser wiring:

| Piece | Role |
|--------|------|
| `src/suite/core.c` | Static strings: mode name → short system prompt stub + tool name list (not executed here). |
| `creation_os_suite_stub` | CLI: `--mode chat\|code\|paper\|corpus\|agent` prints that metadata; `--self-test`. |
| `web/static/suite_lab.html` | Minimal page that POSTs to `creation_os_openai_stub` (deterministic stub reply). |
| `scripts/launch_suite.sh` | Starts stub on `:8080`, serves `web/static` on `:3000`, opens the lab page. |

## What this is not

- Not a replacement for an IDE, cloud assistants, or a full local LLM stack.
- Not merge-gate; optional only. Claims tier: see [WHAT_IS_REAL.md](WHAT_IS_REAL.md) and [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

## Build / run

```bash
make standalone-suite-stub test-suite-stub
./creation_os_suite_stub --mode code

make standalone-openai-stub
./scripts/launch_suite.sh
```

The stub sends **CORS** headers so a page opened from `http://127.0.0.1:3000` can call `http://127.0.0.1:8080`. For `file://` opens, use the script (HTTP origin) instead.

## Stop

Press Ctrl+C in the terminal running `launch_suite.sh`, or close that shell after killing the background jobs it started.
