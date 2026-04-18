# creation-os (npm launcher)

```bash
npx creation-os --help
```

This npm package is a **thin launcher**. It does not ship the
native binary on the npm registry; instead, on first invocation it
fetches the matching `cos` binary for your platform + architecture
from the canonical GitHub release:

> [https://github.com/spektre-labs/creation-os/releases/latest](https://github.com/spektre-labs/creation-os/releases/latest)

and caches it in `~/.creation-os/bin/`. Subsequent invocations
exec the cached binary directly.

For the full build-from-source experience prefer Homebrew, Docker,
or a git clone + `make merge-gate` from the repo root:

- `brew install spektre-labs/tap/creation-os`
- `docker pull spektrelabs/creation-os`
- `git clone https://github.com/spektre-labs/creation-os && cd creation-os && make merge-gate`

## License

Dual-licensed: **SCSL-1.0** (primary, commercial) / **AGPL-3.0-only**
(fallback). See [LICENSE](../../LICENSE) and
[COMMERCIAL_LICENSE.md](../../COMMERCIAL_LICENSE.md).
