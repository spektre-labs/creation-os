# v107 — Installer (brew · curl · Docker)

Three distribution methods for the same build artefact:

| channel | command | artefact |
|---|---|---|
| Homebrew | `brew tap spektre-labs/creation-os && brew install creation-os` | [`packaging/brew/creation-os.rb`](../../packaging/brew/creation-os.rb) |
| curl     | `curl -fsSL https://get.creation-os.dev \| bash`                  | [`scripts/install.sh`](../../scripts/install.sh) |
| Docker   | `docker run -p 8080:8080 spektrelabs/creation-os`                 | [`Dockerfile`](../../Dockerfile) |

All three end at the same place: `creation_os_server` bound on
`127.0.0.1:8080`, with the v108 σ-UI served at `/` and the
OpenAI-compatible API at `/v1/*`.

## What the curl installer does

In order, the script:

1. detects the platform (`macOS` / `Linux`, `arm64` / `x86_64`)
2. installs a C toolchain + `git` if missing (Xcode CLT / apt / dnf /
   pacman / apk)
3. clones `spektre-labs/creation-os` into `~/creation-os`
4. builds `cos`, `creation_os_server`, and `creation_os`
5. runs **every** `check-v60` … `check-v100` self-test, reports rollup
6. downloads the default model (BitNet-b1.58-2B Q4_K_M, ≈1.6 GB) into
   `~/.creation-os/models/` — opt out via `COS_V107_SKIP_MODEL=1`
7. writes `~/.creation-os/config.toml` pointing at the model
8. runs `./cos demo`
9. `exec`s `creation_os_server` on `:8080` — Ctrl-C to stop; re-run
   with `creation_os_server --config ~/.creation-os/config.toml`

All operations are idempotent and safe to re-run.  No file is
written outside `~/creation-os` or `~/.creation-os`.

### Environment knobs

| variable | effect |
|---|---|
| `COS_INSTALL_DIR` | override clone target (default `~/creation-os`) |
| `COS_REPO_URL`    | override git source (default `spektre-labs/creation-os`) |
| `COS_HOME`        | override runtime dir (default `~/.creation-os`) |
| `COS_V107_MODEL_URL`  | override default model URL |
| `COS_V107_MODEL_NAME` | override filename under `models/` |
| `COS_V107_SKIP_MODEL` | skip model download |
| `COS_V107_OVERWRITE_CONFIG` | overwrite an existing `config.toml` |
| `COS_V107_NO_START`   | do not boot the server after install |

## Release pipeline

[`.github/workflows/release.yml`](../../.github/workflows/release.yml)
builds, on every pushed tag `v*`:

- **macOS** universal binaries (arm64 + x86_64, fused via `lipo`)
- **Linux** binaries for `x86_64` and `arm64`
- **Docker** multi-arch image `spektrelabs/creation-os:{latest,<tag>}`

All tarballs attach to the GitHub Release.  Docker push is gated on
the `DOCKERHUB_TOKEN` secret (skipped cleanly when unavailable).

## Smoke gate

`scripts/v107/check_v107_install_macos.sh` validates, offline:

- `install.sh` passes `bash -n` and contains every new v107 knob
- `Dockerfile` is multi-stage and health-checked
- the brew formula is `ruby -c`-clean and installs the three binaries
- the release workflow exposes the three expected jobs

Merge-gate entry point:

```bash
make check-v107-install-macos
```

## Not (yet) shipped

- `get.creation-os.dev` DNS + GitHub Pages redirector — coming with
  v110 launch material
- Windows binaries — deferred; WSL is the supported path for now
- Signed Homebrew bottles — this is a `-source` formula today
