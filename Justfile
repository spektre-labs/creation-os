# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Commercial:    spektrelabs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Just — task runner (https://github.com/casey/just). Install: brew install just | cargo install just
set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

default:
	@just --list

merge-gate:
	make merge-gate

flake-lock:
	nix flake lock

develop:
	nix develop

rust-gate:
	cargo run --manifest-path infra/cos_gate/Cargo.toml --release

cue-vet:
	cue vet infra/config.cue

# OMNIGATE — polyglot probes (then ONE merge-gate; never 9× full CI locally).
omnigate-probe:
	@echo "== probe interpreters (creation-os/) =="
	@command -v nu >/dev/null 2>&1 && nu -c "version" | head -1 || echo "nu: MISSING"
	@command -v julia >/dev/null 2>&1 && julia -e 'print(VERSION)' || echo "julia: MISSING"
	@command -v go >/dev/null 2>&1 && go version || echo "go: MISSING"
	@command -v deno >/dev/null 2>&1 && deno --version || echo "deno: MISSING"
	@command -v zig >/dev/null 2>&1 && zig version || echo "zig: MISSING"
	@command -v python3 >/dev/null 2>&1 && python3 --version || echo "python3: MISSING"
	@command -v perl >/dev/null 2>&1 && perl -v | head -1 || echo "perl: MISSING"
	@command -v raku >/dev/null 2>&1 && raku -v | head -1 || echo "raku: MISSING"
	@command -v gawk >/dev/null 2>&1 && gawk --version | head -1 || echo "gawk: MISSING"

omnigate-all: omnigate-probe
	@echo "== single merge-gate (not 9×) =="
	make merge-gate

omnigate-nu:
	nu omnigate/nu/gate.nu

omnigate-julia:
	julia omnigate/julia/gate.jl

omnigate-go:
	cd omnigate/go && go run .

omnigate-deno:
	deno run -A omnigate/ts/gate.ts

omnigate-zig:
	zig run omnigate/zig/storm.zig

omnigate-py:
	python3 omnigate/py/sigma_storm.py

omnigate-perl:
	perl omnigate/perl/gate.pl

omnigate-raku:
	raku omnigate/raku/gate.raku

omnigate-awk:
	gawk -f omnigate/awk/gate.awk

# HYPERVEC — process swarm folds SHA → ℝⁿ toy fingerprint (+ optional single merge-gate)
hypervec-swarm:
	python3 -m hypervec.swarm

hypervec-materialize:
	HYPERVEC_MATERIALIZE=1 python3 -m hypervec.swarm

# POLYGLOT — extra languages (merge-gate shims + one arm64 mix primitive)
polyglot-list:
	@cat polyglot/README.md

polyglot-asm-arm64:
	clang -c -arch arm64 polyglot/asm/arm64/cos_looplm_mix_byte.s -o /tmp/cos_looplm_mix_byte.o

polyglot-elixir:
	elixir polyglot/elixir/cos_gate.exs

polyglot-crystal:
	crystal run polyglot/crystal/cos_gate.cr

polyglot-lua:
	lua polyglot/lua/cos_gate.lua

polyglot-janet:
	janet polyglot/janet/cos_gate.janet

polyglot-nim:
	nim c -r polyglot/nim/cos_gate.nim

polyglot-racket:
	racket polyglot/racket/cos_gate.rkt
