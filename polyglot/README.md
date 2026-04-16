# Polyglot — “sickest” peripheral runners (optional hosts)

Same law: **`make merge-gate`** from **`CREATION_OS_ROOT`** (default `.` = `creation-os/`).  
These are **not** CI gates unless you wire them yourself — they are extra tongues around the kernel.

| Path | Stack | Run |
|------|-------|-----|
| [`asm/arm64/cos_looplm_mix_byte.s`](asm/arm64/cos_looplm_mix_byte.s) | **GNU as / LLVM asm** (Apple Silicon) | `clang -c asm/arm64/cos_looplm_mix_byte.s -o /tmp/cos_mix.o` |
| [`elixir/cos_gate.exs`](elixir/cos_gate.exs) | **Elixir** | `elixir elixir/cos_gate.exs` |
| [`crystal/cos_gate.cr`](crystal/cos_gate.cr) | **Crystal** | `crystal run crystal/cos_gate.cr` |
| [`lua/cos_gate.lua`](lua/cos_gate.lua) | **Lua 5.x** | `lua lua/cos_gate.lua` |
| [`janet/cos_gate.janet`](janet/cos_gate.janet) | **Janet** | `janet janet/cos_gate.janet` |
| [`nim/cos_gate.nim`](nim/cos_gate.nim) | **Nim** | `nim c -r nim/cos_gate.nim` |
| [`wat/k_eff.wat`](wat/k_eff.wat) | **WebAssembly text** | `wat2wasm k_eff.wat -o /tmp/k_eff.wasm` then `wasmtime run --invoke k_eff …` (optional) |
| [`racket/cos_gate.rkt`](racket/cos_gate.rkt) | **Racket** | `racket polyglot/racket/cos_gate.rkt` |

`just polyglot-list` prints this table. Individual `just polyglot-elixir` etc. spawn the runner if the binary exists.
