# v123 lab scaffolds (TTT, JEPA, MCP/A2A, docs site)

English-only maintainer note. See `docs/CLAIM_DISCIPLINE.md` before citing numbers.

## Python (lab)

- **`cos ttt`**: `--learn`, `--eval` / `--before-after`, `--chunk-size`, `--max-steps`, `--dim`, `--engram-file`; Engram off when `--eval` uses ephemeral `engram_path=None` in `build_sigma_gated_ttt_lab`.
- **`cos think`**: `SigmaJEPA.plan_argmin_sigma` and `--visualize` latent polyline.
- **`cos mcp`**: `--serve-http` + `--host` / `--port`; `--connect-url` for GET smoke.
- **`cos a2a`**: `--card`, `--delegate --to … --task …`, existing `--send`.
- **`cos deploy --docs`**: `npm run build` in `web/`.

## Web

Static export lives under `web/` (Next.js App Router, Tailwind v4, `motion/react`). Diagrams Vite app remains under `web/spektre-diagrams/`.

## Tests

`tests/test_v123_lab.py` covers catalog JSON, A2A card/delegate, MCP HTTP JSON-RPC ping, JEPA plan keys, and CLI smoke for `think`.
