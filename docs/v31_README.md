# Creation OS v31 — “The Purge” (lab, honest)

This is an **optional lab track**: keep the portable kernel + merge gate strict, while experimenting with a **thin POSIX wrapper** around an upstream BitNet inference binary.

## What is shipped in-tree today

- `src/v31/creation_os_v31.c`: deterministic σ-style telemetry math + optional spawn hook
- `make check-v31`: builds and runs `./creation_os_v31 --self-test`
- `docs/WHAT_IS_REAL_v31.md`: tier-tagged contract for what v31 may claim

## What is intentionally NOT shipped automatically

- No submodule vendoring
- No multi‑GB weight downloads
- No “v31 is now the default entrypoint” rewrite of the repository story

## Quickstart (safe)

```bash
make check-v31
```

## Optional upstream checkout (explicit network)

```bash
chmod +x scripts/v31_setup_bitnet_cpp.sh
./scripts/v31_setup_bitnet_cpp.sh --clone
./scripts/v31_setup_bitnet_cpp.sh --print-env
```

Then follow upstream’s own build instructions inside `external/BitNet/` and export:

```bash
export COS_BITNET_CLI=/path/to/upstream-binary
export COS_BITNET_MODEL=/path/to/model.gguf
```

## Chat mode (best-effort lab)

```bash
make standalone-v31
./creation_os_v31
```

Upstream CLIs differ; treat this as a **protocol integration experiment** until stabilized and archived with receipts (`docs/REPRO_BUNDLE_TEMPLATE.md`).
