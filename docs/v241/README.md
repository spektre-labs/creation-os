# v241 — σ-API-Surface (`docs/v241/`)

Unified public rajapinta for Creation OS.  238 kernels inside, one
typed API surface outside, four first-class SDKs, a stable
OpenAI-compatible endpoint for any tool that already speaks the
OpenAI wire protocol.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Unified API (10 endpoints, v1)

| method | path                     | purpose                       | OpenAI-compat |
|--------|--------------------------|-------------------------------|---------------|
| POST   | `/v1/chat/completions`   | OpenAI-compatible chat        | yes           |
| POST   | `/v1/reason`             | multi-path reasoning          | no            |
| POST   | `/v1/plan`               | multi-step planning           | no            |
| POST   | `/v1/create`             | creative generation           | no            |
| POST   | `/v1/simulate`           | domain simulation             | no            |
| POST   | `/v1/teach`              | Socratic mode                 | no            |
| GET    | `/v1/health`             | system status                 | no            |
| GET    | `/v1/identity`           | who am I                      | no            |
| GET    | `/v1/coherence`          | K_eff dashboard               | no            |
| GET    | `/v1/audit/stream`       | real-time audit stream        | no            |

Every endpoint emits `X-COS-Sigma`, `X-COS-Kernel-Path`, and
`X-COS-Audit-Chain` headers alongside the normal response body.

### SDKs

| language   | install                                              |
|------------|------------------------------------------------------|
| python     | `pip install creation-os`                            |
| javascript | `npm install creation-os`                            |
| rust       | `cargo add creation-os`                              |
| go         | `go get github.com/spektre-labs/creation-os-go`      |

### Backward compatibility

`POST /v1/chat/completions` accepts any OpenAI-shaped request
unchanged — request schema, model field, streaming, tool calling.
The Creation OS σ-envelope rides on response headers so existing
OpenAI clients never see new body fields they don't expect.

### Versioning

```
api_version_major = 1
api_version_minor = 0
```

`/v1/` is promised stable across kernel rev-ups; `/v2/` is reserved
for a future breaking change and runs side by side with `/v1/`.

### σ_api

```
σ_api = 1 − (endpoints_passing_audit / n_endpoints)
```

Low σ_api ⇒ every endpoint is declared with method + path + X-COS
headers + compat flag; high ⇒ the surface is fraying.  The v0 tree
requires σ_api == 0.0.

## Merge-gate contract

`bash benchmarks/v241/check_v241_api_openai_compatible.sh`

- self-test PASSES
- `n_endpoints == 10`, all paths under `/v1/`, methods from
  `{GET, POST}`, every endpoint emits X-COS headers
- exactly one OpenAI-compat endpoint (`POST /v1/chat/completions`)
- `n_sdks == 4`; names in order `python, javascript, rust, go`;
  all maintained; non-empty install strings
- `api_version_major == 1`; `api_version_minor ≥ 0`
- `σ_api ∈ [0, 1]` AND `σ_api == 0.0` in v0
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 10-endpoint typed manifest, 4-SDK manifest,
  audit predicate per endpoint, `σ_api` metric, FNV-1a chain.
- **v241.1 (named, not implemented)** — live HTTP router bound to
  these descriptors, SDK package-lock generation, GraphQL mirror,
  SSE streaming for `/v1/audit/stream`.

## Honest claims

- **Is:** a typed surface manifest with audit semantics and a
  byte-deterministic replay.
- **Is not:** an HTTP daemon.  v241.1 is where the same manifest
  drives a real router.

---

*Spektre Labs · Creation OS · 2026*
