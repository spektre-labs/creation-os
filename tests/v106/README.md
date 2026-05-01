<!-- SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
     SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
     Source:        https://github.com/spektre-labs/creation-os-kernel
     Website:       https://spektrelabs.org
     Commercial:    spektre.labs@proton.me
     License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt -->
# v106 — σ-Server tests

## Surfaces

1. **C self-test** (`./creation_os_server --self-test`) — zero
   network.  Covers:
   - `cos_v106_config_defaults` yields host=127.0.0.1, port=8080,
     aggregator="product", tau_abstain=0.7, n_ctx=2048.
   - `cos_v106_json_get_string` / `_get_int` / `_get_double`
     round-trip on a representative body.
   - `cos_v106_json_stream_flag` returns 1 on `"stream": true` and
     0 on `"description":"stream: true"` (no false positives
     inside string values).
   - `cos_v106_json_extract_last_user` picks the last `role:"user"`
     .content and correctly decodes `\n` in the value.
   - `cos_v101_aggregator_name` yields `"mean"` / `"product"`.
2. **Loopback curl smoke** (`benchmarks/v106/check_v106.sh`) —
   starts the server on a random free port (stub mode, no GGUF),
   issues 9 requests with `curl`:
   - `GET /health` → 200 `ok`
   - `GET /v1/models` → 200 JSON with `data[0].id` and
     `sigma_aggregator:"product"`
   - `GET /v1/sigma-profile` → 200 JSON with `has_data:false`
     before any inference
   - `POST /v1/chat/completions` → 503 `not_loaded` in stub mode
   - `OPTIONS /v1/chat/completions` → 204 with CORS headers
   - Unknown method → 405
   - Malformed request → 400
3. **Real-model benchmark** (`benchmarks/v106/bench_v106.sh`) —
   requires a GGUF on disk.  Runs 5 chat-completion requests and
   reports per-request wall-time plus σ_product, σ_abstained from
   the `creation_os` response block.  Not part of `merge-gate`;
   invoked manually via `make bench-v106` once the installer (v107)
   has populated `models/`.

## Merge-gate contract

`make check-v106` runs (1) + (2).  Both must exit 0.  The loopback
smoke auto-SKIPs if `curl` is missing on the host, so it is
portable to minimal CI images.

## Manual sanity (common failures)

- **Bind failure on 8080.** Expected if another process already
  owns the port.  `check_v106.sh` picks a random free port via
  Python, so this is only a problem when the user explicitly
  passes `--port 8080` to the binary by hand.
- **`not_loaded` 503 on real chat.** Config has no `gguf_path` set
  (or the path does not exist).  Server log has the exact reason
  on startup.
- **JSON response longer than 32 KB.** The current server-side
  buffers cap at 32 KB per response.  `max_tokens <= 4096`
  generation produces ≈ 16 KB worst case.  If you hit this, the
  server returns 500 `response overflow` rather than truncating
  silently.
