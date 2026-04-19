# v249 — σ-MCP (`docs/v249/`)

Model Context Protocol integration, both directions — plus
the one thing MCP itself does not ship: a typed σ on every
tool call.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Server side (Creation OS as MCP server)

JSON-RPC 2.0.  Exactly 5 tools:

```
reason · plan · create · simulate · teach
```

Exactly 3 resources:

```
memory · ontology · skills
```

### Client side (Creation OS as MCP consumer)

Exactly 4 external server types in v0 fixture, each with
a σ_trust ∈ [0, 1] measuring the reliability of the
provider in the lineage audit:

| external server | endpoint                        | σ_trust |
|-----------------|---------------------------------|--------:|
| `database`      | `mcp://db.local/sqlite`         | 0.15    |
| `api`           | `mcp://api.local/rest`          | 0.32    |
| `filesystem`    | `mcp://fs.local/ro-mount`       | 0.09    |
| `search`        | `mcp://search.local/brave`      | 0.41    |

### σ-gated tool use

Three-way decision per call, thresholds are hard:

| σ range                 | decision | meaning                 |
|-------------------------|----------|-------------------------|
| `σ ≤ τ_tool   (0.40)`   | `USE`    | trust result, use it    |
| `τ_tool < σ ≤ τ_refuse` | `WARN`   | use with σ-label        |
| `σ > τ_refuse (0.75)`   | `REFUSE` | drop result, do not use |

The v0 fixture is **deliberately adversarial**: it runs 5
calls (one per server-side tool) and requires ≥ 3 USE,
≥ 1 WARN, ≥ 1 REFUSE — so the merge-gate has proof the
gating *has teeth*, not just the code path.

### Discovery (exactly 3 modes)

```
local · mdns · registry
```

Every mode is audited as `mode_ok` and v169 ontology mapping
is asserted (`ontology_mapping_ok`).

### σ_mcp

```
σ_mcp = 1 − (tools_ok + resources_ok + externals_ok +
             gate_decisions_ok + discovery_ok) /
            (5 + 3 + 4 + 5 + 3)
```

v0 requires `σ_mcp == 0.0`.

## Merge-gate contract

`bash benchmarks/v249/check_v249_mcp_server_register.sh`

- self-test PASSES
- `jsonrpc_version == "2.0"`
- 5 tools, 3 resources, 4 externals, 5 calls, 3 discovery
  modes — all in canonical order
- ≥ 3 USE, ≥ 1 WARN, ≥ 1 REFUSE; decision matches σ vs
  thresholds byte-for-byte
- `σ_mcp ∈ [0, 1]` AND `σ_mcp == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed MCP server / client / gate /
  discovery manifest with FNV-1a chain.
- **v249.1 (named, not implemented)** — live JSON-RPC
  transport, real mDNS advertisement, live v169
  ontology-driven tool selection, persistent σ_trust
  lineage per external server, streamed tool results with
  incremental σ.

## Honest claims

- **Is:** a typed, falsifiable MCP-surface manifest with a
  σ-gate whose branches are all exercised by a v0 fixture.
- **Is not:** a running MCP server.  v249.1 is where the
  manifest drives a real JSON-RPC listener.

---

*Spektre Labs · Creation OS · 2026*
