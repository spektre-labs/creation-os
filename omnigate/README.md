# OMNIGATE — polyglot merge-gate summoners

The Makefile remains law. This folder is **ceremonial redundancy**: the same `make merge-gate` invoked through **eight** runtimes so your laptop sounds like a datacenter.

| Runner | File | Invoke |
|--------|------|--------|
| **Nushell** | [`nu/gate.nu`](nu/gate.nu) | `nu nu/gate.nu` (from `creation-os/`) |
| **Julia** | [`julia/gate.jl`](julia/gate.jl) | `julia julia/gate.jl` |
| **Go** | [`go/`](go/) | `cd go && go run .` |
| **Deno** | [`ts/gate.ts`](ts/gate.ts) | `deno run -A ts/gate.ts` |
| **Zig** | [`zig/storm.zig`](zig/storm.zig) | `zig run zig/storm.zig` |
| **Python** | [`py/sigma_storm.py`](py/sigma_storm.py) | `python3 py/sigma_storm.py` |
| **Perl** | [`perl/gate.pl`](perl/gate.pl) | `perl perl/gate.pl` |
| **Raku** | [`raku/gate.raku`](raku/gate.raku) | `raku raku/gate.raku` |
| **Awk** | [`awk/gate.awk`](awk/gate.awk) | `gawk -f awk/gate.awk` |

All honour **`CREATION_OS_ROOT`** (default `.`), expected to be the **`creation-os/`** directory.

## Nix shell with every interpreter

```bash
nix develop .#omnigate
just omnigate-all
```

`just omnigate-all` runs **`omnigate-probe`** (version sniff for each runtime) then **exactly one** `make merge-gate`.  
Running every `omnigate-*` recipe in a row would repeat the full gate **nine times** — do not unless you enjoy heat death.

Per-runner: `just omnigate-go`, `just omnigate-deno`, … (each = full merge-gate).

See root [`flake.nix`](../flake.nix) `devShells.omnigate`.

## Why

Because we can. If any runner disagrees with `make`, **Make wins**.
