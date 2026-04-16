# Creation OS — “sick infra” stack

Optional, **host-agnostic** layers on top of `make merge-gate`. Nothing here replaces the Makefile; these are shells and runners.

| Piece | Language / runtime | What it does |
|------|--------------------|----------------|
| [flake.nix](../flake.nix) | **Nix** | `nix develop` — clang, make, yosys, verilator, symbiyosys, Rust, JDK 17, sbt. |
| [Earthfile](../Earthfile) | **Earthly** (HCL-ish) | `earthly +merge-gate` — same gate inside a Nix-powered container. |
| [Justfile](../Justfile) | **Just** | `just merge-gate` / `just rust-gate` / `just cue-vet`. |
| [cos_gate/](cos_gate/) | **Rust** | `cargo run --manifest-path infra/cos_gate/Cargo.toml` → `make merge-gate`. |
| [cos_gate.zig](cos_gate.zig) | **Zig** | `zig run infra/cos_gate.zig` (Zig 0.13+ `Child.run`). |
| [config.cue](config.cue) | **Cue** | `cue vet infra/config.cue` — tiny contract doc. |
| [.envrc](../.envrc) | **direnv** | `use flake` when you enter `creation-os/`. |

## OMNIGATE (polyglot chaos)

See [`../omnigate/README.md`](../omnigate/README.md) — Go, Julia, Deno, Nushell, Zig, Python, Perl, Raku, Awk runners + `nix develop .#omnigate`.

## HYPERVEC (swarm / toy ℝⁿ fingerprint)

See [`../hypervec/README.md`](../hypervec/README.md) — multiprocessing shard swarm + optional `HYPERVEC_MATERIALIZE=1` → one `make merge-gate`.

## POLYGLOT (asm / Elixir / …)

See [`../polyglot/README.md`](../polyglot/README.md) — AArch64 mix primitive + `merge-gate` shims in Elixir, Crystal, Lua, Janet, Nim, Racket, WAT.

## Quickstart

```bash
cd creation-os
nix develop -c make merge-gate    # after: nix flake lock
just merge-gate
CREATION_OS_ROOT=. cargo run --manifest-path infra/cos_gate/Cargo.toml --release   # binary: cos_gate
zig run infra/cos_gate.zig
cue vet infra/config.cue
```

Lockfile: run **`nix flake lock`** once (commit `flake.lock` for CI reproducibility). Earthfile runs `nix flake update` inside the build if you have not committed a lock yet.
