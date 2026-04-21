# `cos-a2a` — agent-to-agent with σ-trust (NEXT-5)

Google's **A2A (Agent2Agent)** standard lets agents advertise
capabilities via a `.well-known/agent.json` card and dispatch
capability-gated tasks.  Creation OS adds one thing every other A2A
runtime in 2026 is missing: **a σ-trust EMA per peer**.

> If a remote agent keeps giving you answers with high σ (≥ τ_block),
> Creation OS will stop dialing it — automatically, permanently, per
> session.  This is the peer-level equivalent of the σ-gate that
> `cos chat` already applies to its own outputs.

The kernel (`src/sigma/pipeline/a2a.c`, OMEGA-2) ships the state
machine; `cos-a2a` wraps it in a CLI with persistent state at
`~/.cos/a2a.json`.

## Commands

```
cos-a2a card                              print this agent's card JSON
cos-a2a register <id> <url> [caps...]     add or overwrite a peer
cos-a2a request  <id> <cap> <sigma>       record a task outcome
                                          (sigma in [0,1]; updates σ_trust EMA)
cos-a2a list                              peer table (σ_trust + blocklist)
cos-a2a demo                              canonical 12-exchange script
cos-a2a reset                             drop persisted state
cos-a2a --self <id>                       override self_id (default: creation-os)
```

State lives at `~/.cos/a2a.json` (override via `COS_A2A_STATE`).

## σ-trust EMA

```
fresh peer           → σ_trust = 0.50 (neutral)
low σ response       → σ_trust ↓ (more trusted)
high σ response      → σ_trust ↑ (more suspect)
σ_trust > τ_block    → peer is BLOCKED (COS_A2A_ERR_BLOCKED)
```

Update rule: `σ_trust ← σ_trust + lr · (σ_response − σ_trust)`.
Defaults: `lr = 0.20`, `τ_block = 0.90`, `τ_warn = 0.70`.

Starting from the neutral 0.50, sustained σ_response ≈ 0.95 converges
on 0.95 and crosses 0.90 at roughly the tenth exchange.  A single bad
answer is forgiven; systematic hallucination is not.

## Example session

```
$ cos-a2a register alice https://alice.example/.well-known/agent.json qa code
registered alice @ https://alice.example/... (2 capabilities)

$ cos-a2a request alice qa 0.12
OK        peer=alice                    cap=qa              σ_resp=0.120 → σ_trust=0.424 …

$ cos-a2a list
peers: 1 · τ_block=0.90 · lr=0.20
  ID                        URL                               σ_trust   exchanges block?
  alice                     https://alice.example/...         0.424      1        -
```

## Self-card (A2A `/.well-known/agent.json`)

Default capabilities advertised by this kernel:

```json
{
  "name": "creation-os",
  "protocol": "a2a/1.0",
  "sigma_gate": true,
  "scsl_compliant": true,
  "capabilities": ["qa", "code", "sigma_gate", "conformal"]
}
```

## Evidence

```bash
make cos-a2a
make check-cos-a2a            # card + register + request + EMA + blocklist + persist
./cos-a2a demo                # deterministic 12-exchange envelope
```

## Why this matters

Without σ-trust, A2A becomes "let us all hallucinate together but in a
standard JSON wrapper".  With σ-trust, **an agent network self-heals
toward truth**: misbehaving peers drop out, well-calibrated peers rise.
That is the difference between a protocol and an ecology — and it is
the reason Creation OS participates in A2A rather than replacing it.

The σ-MCP + σ-A2A + `cos-mcp` + `cos-a2a` quartet makes Creation OS
the first runtime that speaks *both* open 2026 standards with a σ-gate
on every edge.  See also [docs/MCP_COS_TOOLS.md](MCP_COS_TOOLS.md)
for the complementary tool-call surface.
