<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
-->

# Creation OS — Air-gapped deployment

**Purpose:** describe a **local-first**, **no-internet-required** operator path for σ-gated workflows, without claiming formal air-gap certification. Heuristic checks in `cos offline --verify` are best-effort; site policy and network segmentation remain authoritative. See [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

## Design intent

- **Local-first:** core σ-gate scoring and many `cos` subcommands run without calling cloud APIs when you configure them that way.
- **Optional stack:** GGUF (or other local weights), SQLite-backed memory labs, and vector retrieval (e.g. FAISS) are **integration choices** — install only what your deployment needs.
- **No telemetry channel in this tree:** the repository does not implement a license phone-home or automatic update channel; operational updates are whatever you ship (wheels, media, config).

## What typically works without internet

- **σ-gate L1 / lite scoring** without optional Python deps (see [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) — evidence class: verified invariant where covered by `make check-*`).
- **`cos score` / `cos gate`**, **`cos boot`**, **`cos think`**, and other CLI paths that only use local code and local endpoints.
- **Memory / graph / engram labs** when persistence paths are local (e.g. SQLite under your chosen directory).
- **`cos chat`** when pointed at a **local** OpenAI-compatible server (e.g. llama.cpp `llama-server`), not a cloud base URL.
- **Voice labs** when **faster-whisper**, **kokoro**, and audio deps are pre-installed from offline packages.

## What does **not** work without connectivity (unless pre-staged)

- **Remote model downloads** (e.g. Hugging Face, vendor CDNs): stage weights and wheels on transferable media first.
- **`cos serve` or chat** configured against **cloud** model endpoints.
- **Web search or browsing tools** wired to the public internet.

## Setup on a connected preparer machine

Illustrative flow (adjust paths and extras to your policy):

```bash
# 1) Python package tree for transfer
pip install 'creation-os[chat,voice]' --target /media/usb/cos-pkg

# 2) GGUF or other weights (example ID only — verify license + SHA256 at your org)
huggingface-cli download <org>/<repo> --local-dir /media/usb/models

# 3) Optional voice wheels for offline install
pip download faster-whisper kokoro soundfile -d /media/usb/voice-pkg
```

Copy `/media/usb/*` to the isolated host via approved media or internal package mirror.

## Install on the air-gapped host

```bash
pip install --no-index --find-links /media/usb/cos-pkg creation-os
pip install --no-index --find-links /media/usb/voice-pkg faster-whisper kokoro soundfile
```

Start a **local** inference server (example only):

```bash
llama-server -m /media/usb/models/<your>.gguf --port 8001
```

## Example `cos` usage (local endpoint)

```bash
cos score --prompt "test" --response "hello"
cos chat --endpoint http://127.0.0.1:8001/v1 --prompt "Hi"
cos boot
cos think "What is the capital of France?"
cos voice status
```

## Verification

Heuristic connectivity checks (not a full port scan or IDS replacement):

```bash
cos offline --verify
# exits 0 when probes suggest no generic public DNS/TCP path; 1 when a probe succeeded
COS_FORCE_AIRGAP_OK=1 cos offline --verify   # CI / forced policy: skip network probes only
```

Machine-readable:

```bash
cos offline --verify --json
```

**CI:** set `COS_FORCE_AIRGAP_OK=1` so DNS/TCP probes are skipped while `Fabric().boot()` still runs; combine with your own network policy tests.

Read the `disclaimer` field in JSON output: internal DNS or split-horizon routes may still exist.

## Updates

Package new wheels on a connected preparer → transfer → `pip install --no-index --find-links ...`. There is **no** in-tree auto-update channel.

## License

SCSL-1.0 OR AGPL-3.0-only — see repository `LICENSE`.
