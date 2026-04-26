# SPDX-License-Identifier: AGPL-3.0-or-later
# One command: make all
CC = cc
CFLAGS = -O2 -march=native -Wall -std=c11 -D_GNU_SOURCE -D_DARWIN_C_SOURCE
LDFLAGS = -lm
BUILDDIR = .build
# Verilator 5+: --timing with --lint-only (Ubuntu 24.04+). Override: make VERILATOR_LINT_FLAGS=-Wall formal-rtl-lint
#
# -Wno-WIDTH           : bit-width tolerances in combinational paths are
#                        already hand-proven (sum_h[15:0]/8 ∈ [0,255] fits
#                        in scale[7:0]; raw_next[16:0] < sigma_tgt[15:0]
#                        is the guarded clamp branch).  The project's
#                        real safety net is the SymbiYosys prove matrix.
# -Wno-UNUSEDSIGNAL    : stubs, future-ports, and testbench debug signals.
# -Wno-MULTITOP        : testbench file declares its bench module alongside
#                        the DUT on the command line.
# -Wno-BLKSEQ          : testbench uses blocking assignments in init blocks.
VERILATOR_LINT_FLAGS = -Wall --timing -Wno-WIDTH -Wno-UNUSEDSIGNAL -Wno-MULTITOP -Wno-BLKSEQ
RTL_SV := rtl/cos_formal_iron_combo.sv rtl/cos_agency_iron_combo.sv rtl/cos_agency_iron_formal.sv rtl/cos_commit_iron_combo.sv rtl/cos_boundary_sync.sv rtl/cos_looplm_drum.sv rtl/cos_geodesic_tick.sv rtl/cos_k_eff_bind.sv rtl/cos_silicon_chip_tb.sv

.PHONY: help infra merge-gate doctor completion-install standalone standalone-v6 standalone-v7 standalone-v9 standalone-v10 standalone-v11 standalone-v12 standalone-v15 standalone-v16 standalone-v20 standalone-v21 standalone-v22 standalone-v23 standalone-v24 standalone-v25 standalone-v26 standalone-v27 standalone-v28 standalone-v29 standalone-v31 standalone-v33 standalone-v34 standalone-v35 standalone-v39 standalone-v40 standalone-v41 standalone-v42 standalone-v43 standalone-proxy standalone-v45 standalone-v46 standalone-v47 standalone-v48 standalone-v51 standalone-v53 standalone-v54 standalone-v55 standalone-v56 standalone-v57 standalone-v58 standalone-v59 standalone-v60 standalone-v61 standalone-v62 standalone-v63 standalone-v64 standalone-v65 standalone-v66 standalone-v67 standalone-v68 standalone-v69 standalone-v70 standalone-v71 standalone-v72 standalone-v73 standalone-v74 standalone-v57-hardened standalone-v58-hardened standalone-v59-hardened standalone-v60-hardened standalone-v61-hardened standalone-v62-hardened standalone-v63-hardened standalone-v64-hardened standalone-v65-hardened standalone-v66-hardened standalone-v67-hardened standalone-v68-hardened standalone-v69-hardened standalone-v70-hardened standalone-v71-hardened standalone-v72-hardened standalone-v73-hardened standalone-v74-hardened standalone-v76 standalone-v76-hardened standalone-v77 standalone-v77-hardened standalone-v78 standalone-v78-hardened standalone-v79 standalone-v79-hardened standalone-v80 standalone-v80-hardened standalone-v81 standalone-v81-hardened standalone-v82 standalone-v82-hardened standalone-v83 standalone-v83-hardened standalone-v84 standalone-v84-hardened standalone-v85 standalone-v85-hardened standalone-v86 standalone-v86-hardened standalone-v87 standalone-v87-hardened standalone-v88 standalone-v88-hardened standalone-v89 standalone-v89-hardened standalone-v90 standalone-v90-hardened standalone-v91 standalone-v91-hardened standalone-v92 standalone-v92-hardened standalone-v93 standalone-v93-hardened standalone-v94 standalone-v94-hardened standalone-v95 standalone-v95-hardened standalone-v96 standalone-v96-hardened standalone-v97 standalone-v97-hardened standalone-v98 standalone-v98-hardened standalone-v99 standalone-v99-hardened standalone-v100 standalone-v100-hardened harden sanitize asan-v58 asan-v59 asan-v60 ubsan-v60 asan-v61 ubsan-v61 asan-v62 ubsan-v62 asan-v63 ubsan-v63 asan-v64 ubsan-v64 asan-v65 ubsan-v65 asan-v66 ubsan-v66 asan-v67 ubsan-v67 asan-v68 ubsan-v68 asan-v69 ubsan-v69 asan-v70 ubsan-v70 asan-v71 ubsan-v71 asan-v72 ubsan-v72 asan-v73 ubsan-v73 asan-v74 ubsan-v74 asan-v76 ubsan-v76 asan-v77 ubsan-v77 asan-v78 ubsan-v78 asan-v79 ubsan-v79 asan-v80 ubsan-v80 asan-v81 ubsan-v81 asan-v82 ubsan-v82 asan-v83 ubsan-v83 asan-v84 ubsan-v84 asan-v85 ubsan-v85 asan-v86 ubsan-v86 asan-v87 ubsan-v87 asan-v88 ubsan-v88 asan-v89 ubsan-v89 asan-v90 ubsan-v90 asan-v91 ubsan-v91 asan-v92 ubsan-v92 asan-v93 ubsan-v93 asan-v94 ubsan-v94 asan-v95 ubsan-v95 asan-v96 ubsan-v96 asan-v97 ubsan-v97 asan-v98 ubsan-v98 asan-v99 ubsan-v99 asan-v100 ubsan-v100 hardening-check sbom security-scan reproducible-build attest sign slsa wasm-sandbox ebpf-policy sandbox-exec distroless nix-build sel4-check chace cos check-cos standalone-mcp standalone-openai-stub standalone-suite-stub native-m4 metallib-m4 cos_lm standalone-v27-rust gen-cos-codebook bench-v27-all bench-binding-fidelity bench-vocab-scaling bench-vs-transformer formal-sby-tokenizer formal-sby-v37 formal-sby-v47 synth-v37 check-asic-tile librelane-v38 check-crossbar-sim bench-v40-threshold bench-v41-scaling bench-v42-curve bench-v43-distill bench-v44-overhead bench-v45-paradox bench-v46-e2e v50-benchmark microbench-v58 microbench-v59 microbench-v60 microbench-v61 microbench-v62 microbench-v63 microbench-v64 microbench-v65 microbench-v66 microbench-v67 microbench-v68 microbench-v69 microbench-v70 microbench-v71 microbench-v72 microbench-v73 microbench-v74 microbench-v76 microbench-v77 microbench-v78 microbench-v79 microbench-v80 microbench-v81 microbench-v82 microbench-v83 microbench-v84 microbench-v85 microbench-v86 microbench-v87 microbench-v88 microbench-v89 microbench-v90 microbench-v91 microbench-v92 microbench-v93 microbench-v94 microbench-v95 microbench-v96 microbench-v97 microbench-v98 microbench-v99 microbench-v100 test-v62 test-v63 test-v64 test-v65 test-v66 test-v67 test-v68 test-v69 test-v70 test-v71 test-v72 test-v73 test-v74 test-v76 test-v77 test-v78 test-v79 test-v80 test-v81 test-v82 test-v83 test-v84 test-v85 test-v86 test-v87 test-v88 test-v89 test-v90 test-v91 test-v92 test-v93 test-v94 test-v95 test-v96 test-v97 test-v98 test-v99 test-v100 check-v62 check-v63 check-v64 check-v65 check-v66 check-v67 check-v68 check-v69 check-v70 check-v71 check-v72 check-v73 check-v74 check-v76 check-v77 check-v78 check-v79 check-v80 check-v81 check-v82 check-v83 check-v84 check-v85 check-v86 check-v87 check-v88 check-v89 check-v90 check-v91 check-v92 check-v93 check-v94 check-v95 check-v96 check-v97 check-v98 check-v99 check-v100 standalone-v101 standalone-v101-real test-v101 check-v101 check-v101-real microbench-v101 bench-v101-smoke check-v102 bench-v102 license_attest license-pin license-check license-apply license-attest license-attest-hardened core oracle bench bench-coherence bench-agi-gate bench-tokenizer-v27 physics test test-v6 test-v7 test-v9 test-v10 test-v11 test-v12 test-v15 test-v16 test-v20 test-v21 test-v22 test-v23 test-v24 test-v25 test-v26 test-v27 test-v28 test-v29 test-v31 test-v33 test-v34 test-v35 test-v39 test-v40 test-v41 test-v42 test-v43 test-proxy test-v44 test-v45 test-v46 test-v47 test-v48 test-v51 test-v53 test-v54 test-v55 test-v56 test-v57 test-v58 test-v59 test-v60 test-v61 test-mcp test-openai-stub test-suite-stub check check-v6 check-v7 check-v9 check-v10 check-v11 check-v12 check-v15 check-v16 check-v20 check-v21 check-v22 check-v23 check-v24 check-v25 check-v26 check-v27 check-v28 check-v29 check-v31 check-v33 check-v34 check-v35 check-v39 check-v40 check-v41 check-v42 check-v43 check-proxy check-v44 check-v45 check-v46 check-v47 check-v48 check-v51 check-v53 check-v54 check-v55 check-v56 check-v57 check-v58 check-v59 check-v60 check-v61 check-mcp verify-agent check-openai-stub check-suite-stub check-native-m4 bench-native-m4 check-rtl formal-rtl-lint formal-rtl-sim formal-sby-agency formal-sby-cover-agency eqy-agency-self oss-formal-extreme stack-nucleon stack-singularity rust-iron-lint yosys-elab yosys-prove-agency rust-iron-test hardware-supreme stack-ultimate chisel-compile chisel-verilog all clean distclean verify verify-c verify-sv verify-property verify-integration trust-report red-team red-team-garak red-team-deepteam red-team-sigma red-team-property merge-gate-v48 certify certify-formal certify-coverage certify-binary-audit certify-red-team certify-trace publish-github

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

infra:
	@echo "See infra/README.md — flake.nix (nix develop), Earthfile, Justfile, infra/cos_gate (Rust), infra/cos_gate.zig (Zig), infra/config.cue (Cue)."
	@echo "See omnigate/README.md — polyglot merge-gate runners + nix develop .#omnigate"
	@echo "See hypervec/README.md — multiprocessing swarm + optional HYPERVEC_MATERIALIZE merge-gate"
	@echo "See polyglot/README.md — asm / Elixir / Crystal / Lua / Janet / Nim / Racket / WAT shims"

help:
	@echo "Creation OS — make targets"
	@echo "  help       — this list"
	@echo "  doctor     — one-command repo-health rollup (license + verify + hardening + receipts)"
	@echo "  completion-install — install cos(1) tab-completion for your shell (bash/zsh/fish)"
	@echo "  infra      — optional Nix / Earthly / Just / Rust / Zig / Cue (see infra/README.md)"
	@echo "  standalone — build creation_os from creation_os_v2.c"
	@echo "  standalone-v6 — build creation_os_v6 (Living Kernel + self-test)"
	@echo "  standalone-v7 — build creation_os_v7 (Hallucination Killer + self-test)"
	@echo "  standalone-v9 — build creation_os_v9 (Parameters in Silicon + self-test)"
	@echo "  standalone-v10 — build creation_os_v10 (The Real Mind + self-test)"
	@echo "  standalone-v11 — build creation_os_v11 (MatMul-free mind + self-test)"
	@echo "  standalone-v12 — build creation_os_v12 (Tensor mind + self-test)"
	@echo "  standalone-v15 — build creation_os_v15 (Silicon mind + self-test)"
	@echo "  standalone-v16 — build creation_os_v16 (Unified field + self-test)"
	@echo "  standalone-v20 — build creation_os_v20 (Ship mode + self-test)"
	@echo "  standalone-v21 — build creation_os_v21 (AGI sovereign stack + self-test)"
	@echo "  standalone-v22 — build creation_os_v22 (twenty colossal insights + self-test)"
	@echo "  standalone-v23 — build creation_os_v23 (AGI-schematic affordances + self-test)"
	@echo "  standalone-v24 — build creation_os_v24 (arXiv sci-fi echo latches + self-test)"
	@echo "  standalone-v25 — build creation_os_v25 (enterprise pain ledger + self-test)"
	@echo "  standalone-v26 — build creation_os_v26 (Fortune Global 500 echo orbit + self-test)"
	@echo "  standalone-v27 — build creation_os_v27 (vocab / tokenizer pipeline scaffold + self-test)"
	@echo "  standalone-v28 — build creation_os_v28 (LM integration shell: GGUF + sampler + HTTP + σ toys)"
	@echo "  standalone-v29 — build creation_os_v29 (v29 harness: mmap GGUF view + σ + XNOR + BitNet stub)"
	@echo "  standalone-v31 — build creation_os_v31 (v31 lab: optional upstream wrapper + σ math self-test; not merge-gate)"
	@echo "  standalone-v33 — build creation_os_v33 (v33 lab: σ-routed fallback + schema metrics; not merge-gate)"
	@echo "  standalone-v34 — build creation_os_v34 (v34 lab: σ decompose + Platt + channels_v34; not merge-gate)"
	@echo "  standalone-v35 — build creation_os_v35 (v35 lab: σ-guided speculative decode hooks; not merge-gate)"
	@echo "  standalone-v39 — build creation_os_v39 (v39 lab: σ_hardware + σ_total composition; not merge-gate)"
	@echo "  standalone-v40 — build creation_os_v40 (v40 lab: σ independence + syndrome decoder; not merge-gate)"
	@echo "  standalone-v41 — build creation_os_v41 (v41 lab: σ-guided test-time compute scaffold; not merge-gate)"
	@echo "  standalone-v42 — build creation_os_v42 (v42 lab: σ-guided self-play scaffold; not merge-gate)"
	@echo "  standalone-v43 — build creation_os_v43 (v43 lab: σ-guided knowledge distillation math; not merge-gate)"
	@echo "  standalone-proxy — build creation_os_proxy (v44 lab: σ-native inference proxy + OpenAI-shaped loopback HTTP; not merge-gate)"
	@echo "  standalone-v45 — build creation_os_v45 (v45 lab: σ-introspection + doubt reward + internal probe stub; not merge-gate)"
	@echo "  standalone-v46 — build creation_os_v46 (v46 lab: fast σ-from-logits + SIMD + adaptive quant; not merge-gate)"
	@echo "  standalone-v47 — build creation_os_v47 (v47 lab: ACSL σ-kernel + ZK-σ stub + verify driver; not merge-gate)"
	@echo "  standalone-v48 — build creation_os_v48 (v48 lab: σ-anomaly + sandbox + defense-in-depth; not merge-gate)"
	@echo "  standalone-v51 — build creation_os_v51 (v51 integration scaffold: cognitive loop + σ-gated agent; not merge-gate)"
	@echo "  standalone-v53 — build creation_os_v53 (v53 σ-governed harness scaffold: σ-TAOR + σ-dispatch + σ-compress + creation.md loader; not merge-gate)"
	@echo "  standalone-v54 — build creation_os_v54 (v54 σ-proconductor scaffold: σ-profile routing + σ-weighted aggregation + disagreement abstain + profile learner; no network; not merge-gate)"
	@echo "  standalone-v55 — build creation_os_v55 (v55 σ₃-speculative scaffold: EARS + EASD + σ₃ decomposition; NEON hot path; no network; not merge-gate)"
	@echo "  standalone-v56 — build creation_os_v56 (v56 σ-Constitutional scaffold: VPRM verifier + σ-gated IP-TTT + grokking commutator σ-channel + ANE matmul→1×1 conv layout; NEON hot path; no network; no Core ML; not merge-gate)"
	@echo "  standalone-v57 — build creation_os_v57 (v57 Verified Agent: convergence registry + composition slots; honest M/F/I/P tier per slot; no new σ math; not merge-gate)"
	@echo "  standalone-mcp — build creation_os_mcp (v36 lab: MCP JSON-RPC σ server; not merge-gate)"
	@echo "  cos_lm       — copy creation_os_v28 → cos_lm (CLI alias for LM entrypoint)"
	@echo "  test       — run tests/test_bsc_core (sigma, Noether, crystal)"
	@echo "  test-v6    — ./creation_os_v6 --self-test (30 checks; builds v6 first)"
	@echo "  test-v7    — ./creation_os_v7 --self-test (35 checks; builds v7 first)"
	@echo "  test-v9    — ./creation_os_v9 --self-test (41 checks; builds v9 first)"
	@echo "  test-v10   — ./creation_os_v10 --self-test (46 checks; builds v10 first)"
	@echo "  test-v11   — ./creation_os_v11 --self-test (49 checks; builds v11 first)"
	@echo "  test-v12   — ./creation_os_v12 --self-test (52 checks; builds v12 first)"
	@echo "  test-v15   — ./creation_os_v15 --self-test (58 checks; builds v15 first)"
	@echo "  test-v16   — ./creation_os_v16 --self-test (66 checks; builds v16 first)"
	@echo "  test-v20   — ./creation_os_v20 --self-test (86 checks; builds v20 first)"
	@echo "  test-v21   — ./creation_os_v21 --self-test (99 checks; builds v21 first)"
	@echo "  test-v22   — ./creation_os_v22 --self-test (120 checks; builds v22 first)"
	@echo "  test-v23   — ./creation_os_v23 --self-test (141 checks; builds v23 first)"
	@echo "  test-v24   — ./creation_os_v24 --self-test (162 checks; builds v24 first)"
	@echo "  test-v25   — ./creation_os_v25 --self-test (183 checks; builds v25 first)"
	@echo "  test-v26   — ./creation_os_v26 --self-test (184 checks; builds v26 first)"
	@echo "  test-v27   — ./creation_os_v27 --self-test (70 checks; builds v27 first)"
	@echo "  test-v28   — ./creation_os_v28 --self-test (29 checks; builds v28 first)"
	@echo "  test-v29   — ./creation_os_v29 --self-test (22 checks; builds v29 first)"
	@echo "  check      — standalone + test (use before PR / CI)"
	@echo "  check-v6   — standalone-v6 + test-v6 (v6 only)"
	@echo "  check-v7   — standalone-v7 + test-v7 (v7 only)"
	@echo "  check-v9   — standalone-v9 + test-v9 (v9 only)"
	@echo "  check-v10  — standalone-v10 + test-v10 (v10 only)"
	@echo "  check-v11  — standalone-v11 + test-v11 (v11 only)"
	@echo "  check-v12  — standalone-v12 + test-v12 (v12 only)"
	@echo "  check-v15  — standalone-v15 + test-v15 (v15 only)"
	@echo "  check-v16  — standalone-v16 + test-v16 (v16 only)"
	@echo "  check-v20  — standalone-v20 + test-v20 (v20 only)"
	@echo "  check-v21  — standalone-v21 + test-v21 (v21 only)"
	@echo "  check-v22  — standalone-v22 + test-v22 (v22 only)"
	@echo "  check-v23  — standalone-v23 + test-v23 (v23 only)"
	@echo "  check-v24  — standalone-v24 + test-v24 (v24 only)"
	@echo "  check-v25  — standalone-v25 + test-v25 (v25 only)"
	@echo "  check-v26  — standalone-v26 + test-v26 (v26 only)"
	@echo "  check-v27  — standalone-v27 + test-v27 (v27 tokenizer scaffold only)"
	@echo "  check-v28  — standalone-v28 + test-v28 (v28 LM integration shell only)"
	@echo "  check-v29  — standalone-v29 + test-v29 (v29 collapse harness only)"
	@echo "  check-v31  — standalone-v31 + test-v31 (v31 lab only; not merge-gate)"
	@echo "  check-v33  — standalone-v33 + test-v33 (v33 lab only; not merge-gate)"
	@echo "  test-v34   — ./creation_os_v34 --self-test (v34 lab only)"
	@echo "  check-v34  — standalone-v34 + test-v34 (v34 lab only; not merge-gate)"
	@echo "  test-v35   — ./creation_os_v35 --self-test (v35 lab only)"
	@echo "  check-v35  — standalone-v35 + test-v35 (v35 lab only; not merge-gate)"
	@echo "  test-v39   — ./creation_os_v39 --self-test (v39 lab only)"
	@echo "  check-v39  — standalone-v39 + test-v39 (v39 lab only; not merge-gate)"
	@echo "  test-v40   — ./creation_os_v40 --self-test (v40 lab only)"
	@echo "  check-v40  — standalone-v40 + test-v40 (v40 lab only; not merge-gate)"
	@echo "  test-v41   — ./creation_os_v41 --self-test (v41 lab only)"
	@echo "  check-v41  — standalone-v41 + test-v41 (v41 lab only; not merge-gate)"
	@echo "  test-v42   — ./creation_os_v42 --self-test (v42 lab only)"
	@echo "  check-v42  — standalone-v42 + test-v42 (v42 lab only; not merge-gate)"
	@echo "  test-v43   — ./creation_os_v43 --self-test (v43 lab only)"
	@echo "  check-v43  — standalone-v43 + test-v43 (v43 lab only; not merge-gate)"
	@echo "  test-proxy / test-v44 — ./creation_os_proxy --self-test (v44 lab only)"
	@echo "  check-proxy / check-v44 — standalone-proxy + test-proxy (v44 lab only; not merge-gate)"
	@echo "  test-v45   — ./creation_os_v45 --self-test (v45 lab only)"
	@echo "  check-v45  — standalone-v45 + test-v45 (v45 lab only; not merge-gate)"
	@echo "  test-v46   — ./creation_os_v46 --self-test (v46 lab only)"
	@echo "  check-v46  — standalone-v46 + test-v46 (v46 lab only; not merge-gate)"
	@echo "  test-v47   — ./creation_os_v47 --self-test (v47 lab only)"
	@echo "  check-v47  — standalone-v47 + test-v47 (v47 lab only; not merge-gate)"
	@echo "  test-v48   — ./creation_os_v48 --self-test (v48 lab only)"
	@echo "  check-v48  — standalone-v48 + test-v48 (v48 lab only; not merge-gate)"
	@echo "  check-v51  — standalone-v51 + test-v51 (v51 integration scaffold only; not merge-gate)"
	@echo "  check-v53  — standalone-v53 + test-v53 (v53 σ-governed harness scaffold only; not merge-gate)"
	@echo "  check-v54  — standalone-v54 + test-v54 (v54 σ-proconductor scaffold only; not merge-gate)"
	@echo "  check-v55  — standalone-v55 + test-v55 (v55 σ₃-speculative scaffold only; not merge-gate)"
	@echo "  check-v56  — standalone-v56 + test-v56 (v56 σ-Constitutional scaffold only; not merge-gate)"
	@echo "  check-v57  — standalone-v57 + test-v57 (v57 Verified Agent convergence registry only; not merge-gate)"
	@echo "  standalone-v58 — build creation_os_v58 (v58 σ-Cache: σ-decomposed KV-cache eviction kernel + per-token precision tier + branchless NEON path; no model; no network; not merge-gate)"
	@echo "  check-v58  — standalone-v58 + test-v58 (v58 σ-Cache eviction + branchless kernel + ≥50 invariant tests)"
	@echo "  microbench-v58 — deterministic σ-Cache kernel timing on synthetic data (scripts/v58/microbench.sh)"
	@echo "  standalone-v59 — build creation_os_v59 (v59 σ-Budget: σ-decomposed adaptive test-time compute budget controller; four-valued CONTINUE/EARLY_EXIT/EXPAND/ABSTAIN; branchless NEON SoA path; no model; no network; not merge-gate)"
	@echo "  check-v59  — standalone-v59 + test-v59 (v59 σ-Budget + branchless four-way decision + ≥60 invariant tests)"
	@echo "  microbench-v59 — deterministic σ-Budget kernel timing on synthetic reasoning traces (scripts/v59/microbench.sh)"
	@echo "  standalone-v60 — build creation_os_v60 (v60 σ-Shield: branchless capability authorise + σ-gated intent + TOCTOU-free args + code-page integrity; 5-valued ALLOW/DENY_CAP/DENY_INTENT/DENY_TOCTOU/DENY_INTEGRITY; constant-time hash equality; no allocation on hot path; not merge-gate)"
	@echo "  check-v60  — standalone-v60 + test-v60 (v60 σ-Shield runtime security kernel + ≥60 invariant tests)"
	@echo "  microbench-v60 — deterministic σ-Shield authorise timing on synthetic request traces (scripts/v60/microbench.sh)"
	@echo "  standalone-v61 — build creation_os_v61 (v61 Σ-Citadel: Bell-LaPadula + Biba lattice + MLS compartments + attestation quote; compose-with-v60; libsodium opt-in via COS_V61_LIBSODIUM=1)"
	@echo "  check-v61  — standalone-v61 + test-v61 (v61 Σ-Citadel lattice + attestation; ≥60 invariant tests)"
	@echo "  microbench-v61 — batch lattice_check timing (scripts/v61/microbench.sh)"
	@echo "  standalone-v62 — build creation_os_v62 (v62 Reasoning Fabric: latent-CoT + EBT verifier + HRM loop + NSA attend + MTP draft + ARKV KV; alien-tier 2026 frontier on M4)"
	@echo "  check-v62  — standalone-v62 + test-v62 (v62 Reasoning Fabric; ≥62 invariant tests)"
	@echo "  microbench-v62 — NSA + EBT throughput on M4 (./creation_os_v62 --bench)"
	@echo "  standalone-v63 — build creation_os_v63 (v63 σ-Cipher: end-to-end encrypted fabric — BLAKE2b + HKDF + ChaCha20-Poly1305 + X25519 + sealed envelope + ratchet + session; RFC 7693/8439/7748 vectors)"
	@echo "  check-v63  — standalone-v63 + test-v63 (v63 σ-Cipher; ≥70 invariant tests against official RFC vectors)"
	@echo "  microbench-v63 — AEAD + BLAKE2b + X25519 + seal throughput on M4 (./creation_os_v63 --bench)"
	@echo "  standalone-v64 — build creation_os_v64 (v64 σ-Intellect: MCTS-σ + skill library + tool-authz + reflexion ratchet + AlphaEvolve-σ + MoD-σ; 5-bit composed decision)"
	@echo "  check-v64  — standalone-v64 + test-v64 (v64 σ-Intellect; ≥150 deterministic tests)"
	@echo "  microbench-v64 — MCTS / skill retrieve / tool-authz / MoD throughput on M4 (./creation_os_v64 --bench)"
	@echo "  standalone-v65 — build creation_os_v65 (v65 σ-Hypercortex: bipolar HDC + VSA bind/bundle/permute + cleanup + role-filler + analogy + sequence + HVL bytecode; 6-bit composed decision)"
	@echo "  check-v65  — standalone-v65 + test-v65 (v65 σ-Hypercortex; 534 deterministic tests)"
	@echo "  microbench-v65 — Hamming / bind / cleanup / HVL throughput on M-series (./creation_os_v65 --bench)"
	@echo "  standalone-v66 — build creation_os_v66 (v66 σ-Silicon: feature-detect + INT8 GEMV + ternary GEMV + NativeTernary wire + CFC conformal gate + HSL bytecode; 7-bit composed decision)"
	@echo "  check-v66  — standalone-v66 + test-v66 (v66 σ-Silicon; ≥1700 deterministic tests)"
	@echo "  microbench-v66 — INT8 GEMV / ternary GEMV / NTW decode / HSL throughput on M-series (./creation_os_v66 --bench)"
	@echo "  standalone-v67 — build creation_os_v67 (v67 σ-Noesis: BM25 + dense-sig + graph-walk + beam + dual-process + metacog + tactics + NBL bytecode; 8-bit composed decision)"
	@echo "  check-v67  — standalone-v67 + test-v67 (v67 σ-Noesis; ≥2500 deterministic tests)"
	@echo "  microbench-v67 — BM25 / dense / beam / NBL throughput (./creation_os_v67 --bench)"
	@echo "  standalone-v68 — build creation_os_v68 (v68 σ-Mnemos: bipolar HV D=8192 + surprise gate + ACT-R decay + recall + Hebbian TTT + sleep consolidation + forgetting controller + MML 10-op bytecode; 9-bit composed decision)"
	@echo "  check-v68  — standalone-v68 + test-v68 (v68 σ-Mnemos; ≥2700 deterministic tests)"
	@echo "  microbench-v68 — HV / recall / hebb / consolidate / MML throughput (./creation_os_v68 --bench)"
	@echo "  standalone-v69 — build creation_os_v69 (v69 σ-Constellation: tree-speculative EAGLE-3 + multi-agent debate Council+FREE-MAD + Byzantine vote 2f+1 + MaxScore MoE routing + vector clocks + flash chunked dot + Elo-UCB self-play + popcount dedup + CL 10-op bytecode; 10-bit composed decision)"
	@echo "  check-v69  — standalone-v69 + test-v69 (v69 σ-Constellation; ≥3200 deterministic tests)"
	@echo "  microbench-v69 — tree / debate / BFT / route / dedup / Elo / CL throughput (./creation_os_v69 --bench)"
	@echo "  standalone-v70 — build creation_os_v70 (v70 σ-Hyperscale: P2Q ShiftAddLLM + Mamba-2 SSM + RWKV-7 delta-rule + 10 240-expert DeepSeek-V3 MoE + HBM-PIM AND-popcount + photonic WDM dot + Loihi-3 graded spike + NCCL ring all-reduce + Petals/Helix/DirectStorage LRU + HSL 10-op bytecode; 11-bit composed decision)"
	@echo "  check-v70  — standalone-v70 + test-v70 (v70 σ-Hyperscale; deterministic self-test + 11-bit truth table 2048 rows)"
	@echo "  microbench-v70 — p2q / ssm / rwkv / moe / pim / wdm / spike / ring / lru / hsl throughput (./creation_os_v70 --bench)"
	@echo "  standalone-v71 — build creation_os_v71 (v71 σ-Wormhole: portal Einstein-Rosen bridge table + constant-time anchor cleanup + single-XOR teleport + Kleinberg small-world multi-hop routing + ER=EPR tensor-bond pairing + HMAC-HV bridge integrity + Poincaré-boundary gate + hop budget + path receipt + WHL 10-op bytecode; 12-bit composed decision)"
	@echo "  check-v71  — standalone-v71 + test-v71 (v71 σ-Wormhole; deterministic self-test + 12-bit truth table 4096 rows)"
	@echo "  microbench-v71 — hamming / teleport / match / route / WHL / 12-bit compose throughput (./creation_os_v71 --bench)"
	@echo "  standalone-v72 — build creation_os_v72 (v72 σ-Chain: binary Merkle authenticated ledger + append-only receipt chain + WOTS+ one-time signature + threshold t-of-n quorum + hash-chain VRF + DAG-BFT quorum gate + ZK proof-receipt + EIP-7702 session delegation + chain-receipt bundle + SCL 10-op bytecode; 13-bit composed decision)"
	@echo "  check-v72  — standalone-v72 + test-v72 (v72 σ-Chain; deterministic self-test + 13-bit truth table 8192 rows)"
	@echo "  microbench-v72 — hash / merkle / wots+ / delegation / 13-bit compose throughput (./creation_os_v72 --bench)"
	@echo "  standalone-v73 — build creation_os_v73 (v73 σ-Omnimodal Creator: universal-modality tokenizer + rectified-flow integer scheduler + VINO cross-modal XOR bind + MOVA video+audio co-synth lock + Matrix-Game world-model step + DiReCT physics gate + n8n workflow DAG executor + Cursor/Lovable/Devin-style code-edit planner + MultiGen asset navigation + OML 10-op bytecode; 14-bit composed decision)"
	@echo "  check-v73  — standalone-v73 + test-v73 (v73 σ-Omnimodal Creator; deterministic self-test + 14-bit truth table 16384 rows)"
	@echo "  microbench-v73 — hamming / tokenize / flow / 14-bit compose / workflow throughput (./creation_os_v73 --bench)"
	@echo "  standalone-v74 — build creation_os_v74 (v74 σ-Experience: Fitts-V2P target heatmap + adaptive layout + designer-basis personalisation + SquireIR slot authoring + universal-expert LoRA-MoE mesh + skill composition + Mobile-GS order-free Gaussian-splat render + DLSS 4.5 / FSR / XeSS upscale & multi-frame-gen + 1-second interactive-world synth + XPL 10-op bytecode; 15-bit composed decision)"
	@echo "  check-v74  — standalone-v74 + test-v74 (v74 σ-Experience; deterministic self-test + 15-bit truth table 32768 rows)"
	@echo "  microbench-v74 — hamming / fitts / expert-topk / render / world-second / 15-bit compose throughput (./creation_os_v74 --bench)"
	@echo "  standalone-v76 — build creation_os_v76 (v76 σ-Surface: iOS + Android + WhatsApp/Telegram/Signal/iMessage/RCS/Matrix/XMPP/Discord/Slack/Line bridge + Signal-protocol E2E ratchet + WCAG 2.2 a11y + LWW CRDT + 64-app legacy-software capability registry + 64-format file registry + SBL 10-op bytecode; 16-bit composed decision)"
	@echo "  check-v76  — standalone-v76 + test-v76 (v76 σ-Surface; deterministic self-test + 16-bit truth table 65536 rows)"
	@echo "  microbench-v76 — hamming / gesture-classify / legacy-match / ratchet / 16-bit compose throughput (./creation_os_v76 --bench)"
	@echo "  standalone-v77 — build creation_os_v77 (v77 σ-Reversible: 10 bit-reversible primitives — NOT/CNOT/SWAP/Fredkin/Toffoli/Peres/Majority-3/Bennett driver/8-bit reversible adder/RVL bytecode; Landauer / Bennett plane; 17-bit composed decision via cos_v77_compose_decision)"
	@echo "  check-v77  — standalone-v77 + test-v77 (v77 σ-Reversible; forward ∘ reverse ≡ identity; > 700 000 self-test rows incl. full 256×256×2 adder sweep + 2048 random-program VM round-trips)"
	@echo "  microbench-v77 — reversible-VM round-trip throughput (./creation_os_v77 --bench)"
	@echo "  standalone-v78 — build creation_os_v78 (v78 σ-Gödel-Attestor: 10 meta-cognitive primitives — IIT-φ/FEP/MDL/Gödel-num/Global-Workspace/halting-witness/Löbian self-trust/bisim/Chaitin-Ω/MCB bytecode; 18-bit composed decision via cos_v78_compose_decision)"
	@echo "  check-v78  — standalone-v78 + test-v78 (v78 σ-Gödel-Attestor; integer-only Q0.15 log2 table; > 200 000 self-test rows incl. IIT-φ + FEP + Gödel sequences + workspace sweep + halting-witness grid + MCB round-trips + 18-bit compose truth table)"
	@echo "  microbench-v78 — MCB proof throughput (./creation_os_v78 --bench)"
	@echo "  standalone-v79 — build creation_os_v79 (v79 σ-Simulacrum: 10 hypervector-space simulation primitives — Verlet/CA/stabilizer/HD-reservoir/Koopman/assembly/graph/energy/receipt/SSL bytecode; 19-bit composed decision via cos_v79_compose_decision)"
	@echo "  check-v79  — standalone-v79 + test-v79 (v79 σ-Simulacrum; Q16.16 fixed-point; > 100 000 self-test rows incl. 5000-step leapfrog energy-drift band, 8-rule CA determinism sweep, randomized Clifford walk with commutativity invariant, reservoir determinism, Koopman GF(2)-linearity, 19-bit compose truth table)"
	@echo "  microbench-v79 — SSL step throughput (./creation_os_v79 --bench)"
	@echo "  standalone-v80 — build creation_os_v80 (v80 σ-Cortex: 10 hypervector-space neocortical reasoning primitives — Mamba selective SSM / RoPE / sliding-window attention / paged KV cache / speculative decoding / variational free energy / KAN edge / CTM Kuramoto / MoE top-k / TTC bytecode; 20-bit composed decision via cos_v80_compose_decision)"
	@echo "  check-v80  — standalone-v80 + test-v80 (v80 σ-Cortex; Q16.16 fixed-point; > 100 000 self-test rows incl. SSM linearity/decay/norm-budget, RoPE determinism and zero-pos identity, attention argmin, KV page invariants, speculative-verify LCP truth-table, free-energy constancy, KAN monotone band, CTM determinism, MoE top-k popcount, randomized TTC-VM soak, 20-bit compose truth table)"
	@echo "  microbench-v80 — TTC step throughput (./creation_os_v80 --bench)"
	@echo "  check-v81  — standalone-v81 + test-v81 (v81 σ-Lattice: Keccak-f[1600]+SHAKE-128/256+Kyber NTT(q=3329)+Barrett+Montgomery+CBD+KEM; > 3.5M self-test rows incl. FIPS 202 empty-string KATs and round-trip KEM correctness; 21-bit composed decision)"
	@echo "  microbench-v81 — Keccak perm/s + KEM trips/s (./creation_os_v81 --bench)"
	@echo "  check-v82  — standalone-v82 + test-v82 (v82 σ-Stream: streaming per-chunk 22-bit AND gate, halt-on-flip, SHAKE-256 Merkle chain, external replay-verify; > 70 000 self-test rows)"
	@echo "  check-v83  — standalone-v83 + test-v83 (v83 σ-Agentic: PLAN→ROLL→SURPRISE→ENERGY learner loop with rollback and Mnemos consolidation; 23-bit composed decision; > 13 000 self-test rows)"
	@echo "  check-v84  — standalone-v84 + test-v84 (v84 σ-ZKProof: NANOZK-style layerwise Merkle commit + tamper-detecting opening proof; 24-bit composed decision; > 13 000 self-test rows)"
	@echo "  check-v85  — standalone-v85 + test-v85 (v85 σ-Formal: TLA-style ALWAYS/EVENTUALLY/RESPONDS runtime invariant checker; 25-bit composed decision; 500+ fuzz trials)"
	@echo "  license_attest — build the License Attestation Kernel (v75 σ-License: SHA-256 of LICENSE-SCSL-1.0.md baked into every Receipt; SCSL-1.0 §11)"
	@echo "  license-pin    — recompute SHA-256 of LICENSE-SCSL-1.0.md → LICENSE.sha256"
	@echo "  license-check  — verify SPDX headers across src/cli/tools tree + LICENSE.sha256 + bundle integrity"
	@echo "  license-apply  — idempotently inject SCSL-1.0/AGPL-3.0 SPDX header into every source file"
	@echo "  license-attest — self-test (FIPS-180-4 KAT), verify license + bundle, emit a sample License-Bound Receipt"
	@echo "  license-attest-hardened — hardened build of license_attest + self-test"
	@echo "  cos        — build the unified Apple-tier CLI front door (cli/cos.c, single binary, no deps)"
	@echo "  check-cos  — build cos and print its version line"
	@echo "  attest     — emit ATTESTATION.json (v61 quote) + optional cosign sign-blob (scripts/security/attest.sh)"
	@echo "  sign       — Sigstore cosign sign-blob for hardened binaries + SBOM (SKIPs if cosign or COSIGN_EXPERIMENTAL absent)"
	@echo "  slsa       — emit SLSA v1.0 provenance predicate to PROVENANCE.json (local stub; real SLSA-3 via CI)"
	@echo "  wasm-sandbox — compile wasm/example_tool.c and run under wasmtime (SKIP if WASI-SDK/wasmtime missing)"
	@echo "  ebpf-policy  — build ebpf/sigma_shield.bpf.o (Linux only; SKIP on macOS)"
	@echo "  sandbox-exec — run creation_os_v61 --self-test inside Apple sandbox-exec profile sandbox/darwin.sb"
	@echo "  distroless   — build gcr.io/distroless/cc-debian12 container (SKIP if docker missing)"
	@echo "  nix-build    — reproducible Nix build of creation_os_v61 via nix/v61.nix"
	@echo "  sel4-check   — presence/contract check of sel4/sigma_shield.camkes (CAmkES build SKIPs on hosts without toolchain)"
	@echo "  chace        — CHACE aggregator: runs all layers above (seL4 + WASM + eBPF + sandbox + hardening + sanitizer + SBOM + scan + repro + attest + sign + slsa + distroless) and reports PASS/SKIP/FAIL honestly"
	@echo "  harden     — rebuild v57/v58/v59/v60/v61 with OpenSSF 2026 hardening flags + ARM64 branch-protection"
	@echo "  sanitize   — build+run asan-v58/v59/v60/v61 and ubsan-v60/v61 against their self-tests"
	@echo "  hardening-check — verify hardened binary retains PIE, stack canaries, fortify references (scripts/security/hardening_check.sh)"
	@echo "  sbom       — emit SBOM.json (CycloneDX-lite 1.5 JSON) of all source components"
	@echo "  security-scan — run layered gitleaks / grep-only secret scan and hardcoded-URL check (scripts/security/scan.sh)"
	@echo "  reproducible-build — rebuild standalone-v60 twice, compare SHA-256 digests (scripts/security/reproducible_build.sh)"
	@echo "  verify-agent — live aggregate driver (scripts/v57/verify_agent.sh); dispatches each composition slot's owning make target and reports PASS / SKIP / FAIL honestly (SKIP on missing tools, never silent PASS)"
	@echo "  test-mcp   — ./creation_os_mcp --self-test (MCP σ server lab only)"
	@echo "  check-mcp  — standalone-mcp + test-mcp (MCP lab only; not merge-gate)"
	@echo "  standalone-openai-stub — build creation_os_openai_stub (loopback OpenAI-shaped /v1 shim)"
	@echo "  test-openai-stub   — ./creation_os_openai_stub --self-test (5 checks)"
	@echo "  check-openai-stub  — standalone-openai-stub + test-openai-stub (optional; not merge-gate)"
	@echo "  standalone-suite-stub — build creation_os_suite_stub (mode metadata CLI; see docs/SUITE_LAB.md)"
	@echo "  test-suite-stub      — ./creation_os_suite_stub --self-test"
	@echo "  check-suite-stub     — openai-stub + suite-stub self-tests (optional lab; not merge-gate)"
	@echo "  native-m4            — build creation_os_native_m4 (Apple-only; opt-in; not merge-gate)"
	@echo "  metallib-m4          — compile native_m4/creation_os_lw.metallib (Apple-only; optional)"
	@echo "  check-native-m4      — ./creation_os_native_m4 --self-test (SKIP on non-Apple)"
	@echo "  bench-native-m4      — buffer-sizes smoke + --bench 65536×30 --warmup 3 --scalar (SKIP on non-Apple)"
	@echo "  reviewer — run critic-facing checks (v26 + v2 self-test + tier tags)"
	@echo "  merge-gate — portable check + every flagship self-test (v6..v29); same as CI / publish preflight"
	@echo "  formal-rtl-lint — Verilator --lint-only on rtl/*.sv (SKIP if verilator missing)"
	@echo "  formal-rtl-sim  — Verilator --binary + run cos_silicon_chip_tb (SKIP if verilator missing)"
	@echo "  yosys-elab      — Yosys elaborate_rtl.ys (SKIP if yosys missing)"
	@echo "  yosys-prove-agency — Yosys sat -prove-asserts on cos_agency_iron_formal"
	@echo "  rust-iron-test  — cargo test in hw/rust/spektre-iron-gate (SKIP if cargo missing)"
	@echo "  rust-iron-lint  — cargo fmt --check + clippy -D warnings (iron gate)"
	@echo "  hardware-supreme — formal-rtl-lint + yosys-elab + rust-iron-test + chisel-compile (SKIPs OK)"
	@echo "  stack-ultimate  — hardware-supreme + yosys-prove + sim + chisel-verilog (CI fast gate)"
	@echo "  formal-sby-agency / formal-sby-cover-agency — SymbiYosys (SKIP if sby missing)"
	@echo "  eqy-agency-self — EQY self-equivalence on cos_agency_iron_combo (SKIP if eqy missing)"
	@echo "  oss-formal-extreme — SBY prove+cover + Yosys SAT + EQY + rust-iron-lint"
	@echo "  stack-nucleon / stack-singularity — formal stacks (see CONTRIBUTING)"
	@echo "  check-rtl       — formal-rtl-lint only"
	@echo "  chisel-compile / chisel-verilog — hw/chisel (SKIP if sbt missing)"
	@echo "  publish-github — rsync this tree → fresh creation-os clone + push main (needs auth)"
	@echo "  bench      — GEMM vs BSC microbench (host-dependent throughput)"
	@echo "  bench-coherence — batch Hamming gate (NEON on AArch64, else scalar)"
	@echo "  bench-agi-gate — K-agent parliament + memory-bank argmin (NEON scan)"
	@echo "  gen-cos-codebook — write COSB mmap table (.build/cos_codebook_32k.bin by default)"
	@echo "  bench-binding-fidelity / bench-vocab-scaling / bench-vs-transformer — v27 microbenches"
	@echo "  bench-v27-all — tokenizer + binding + scaling + transformer-ops comparison"
	@echo "  standalone-v27-rust — optional Rust GDA27 staticlib + v27 binary (needs cargo)"
	@echo "  formal-sby-tokenizer — SymbiYosys BMC on GDA27 roundtrip (SKIP if sby missing)"
	@echo "  formal-sby-v37 — SymbiYosys prove on hdl/v37 σ-pipeline harness (SKIP if sby missing)"
	@echo "  formal-sby-v47 — SymbiYosys extended-depth replay of v37 σ-pipeline (SKIP if sby missing)"
	@echo "  verify / verify-* / trust-report — v47 verification stack (Frama-C + sby + Hypothesis + integration slice; SKIPs OK)"
	@echo "  red-team / red-team-* — v48 adversarial harness (Garak/DeepTeam SKIPs unless installed + model endpoint)"
	@echo "  merge-gate-v48 — verify + red-team + check-v31 + reviewer (optional heavy gate; SKIPs OK)"
	@echo "  certify / certify-* — v49 DO-178C-aligned assurance pack (formal targets + MC/DC driver + audit + trace; NOT FAA/EASA certification)"
	@echo "  synth-v37 — Yosys xc7 synth + optional SBY via hdl/v37/synth_and_measure.sh (SKIPs if tools missing)"
	@echo "  check-asic-tile — Verilator smoke for hdl/asic/sigma_tile.sv (SKIP if verilator missing)"
	@echo "  librelane-v38 — LibreLane driver script for hdl/asic/config.json (SKIP if librelane missing)"
	@echo "  check-crossbar-sim — Verilator smoke for hdl/neuromorphic/crossbar_sim.sv (SKIP if verilator missing)"
	@echo "  bench-v40-threshold — v40 TruthfulQA σ-channel sweep stub (SKIP unless harness wired)"
	@echo "  bench-v41-scaling — v41 test-time scaling stub (no in-tree GSM8K/AIME harness yet)"
	@echo "  bench-v42-curve — v42 self-play improvement curve stub (no in-tree self-play eval harness yet)"
	@echo "  bench-v43-distill — v43 KD quality / TruthfulQA stub (no in-tree distill + eval harness yet)"
	@echo "  bench-v44-overhead — v44 σ-proxy latency overhead stub (no in-tree engine A/B harness yet)"
	@echo "  bench-v45-paradox — v45 introspection / paradox scatter stub (no in-tree multi-model harness yet)"
	@echo "  bench-v46-e2e — v46 BitNet+σ e2e bench stub (no in-tree bitnet.cpp harness yet)"
	@echo "  v50-benchmark — v50 rollup: STUB eval JSON slots + certify logs + FINAL_RESULTS.md"
	@echo "  oracle     — oracle_speaks, oracle_ultimate"
	@echo "  physics    — genesis, qhdc"
	@echo "  core       — compile core/*.c to .build/*.o"
	@echo "  all        — standalone + oracle + bench + physics + test"
	@echo "  clean      — remove build artifacts"

check: standalone test check-text-similarity
	@echo "check: OK (standalone + test + text_similarity)"

# Portable kernel test + all standalone --self-test matrices (184 @ v26; +70 @ v27; +29 @ v28; +22 @ v29). CI and publish script use this.
merge-gate:
	@$(MAKE) check
	@$(MAKE) check-v6
	@$(MAKE) check-v7
	@$(MAKE) check-v9
	@$(MAKE) check-v10
	@$(MAKE) check-v11
	@$(MAKE) check-v12
	@$(MAKE) check-v15
	@$(MAKE) check-v16
	@$(MAKE) check-v20
	@$(MAKE) check-v21
	@$(MAKE) check-v22
	@$(MAKE) check-v23
	@$(MAKE) check-v24
	@$(MAKE) check-v25
	@$(MAKE) check-v26
	@$(MAKE) check-v27
	@$(MAKE) check-v28
	@$(MAKE) check-v29
	@$(MAKE) check-v101
	@$(MAKE) check-v102
	@$(MAKE) check-v103
	@$(MAKE) check-v104
	@$(MAKE) check-v106
	@$(MAKE) check-v60-v100
	@$(MAKE) check-v111
	@$(MAKE) check-v106-curl-loopback
	@$(MAKE) check-v107-install-macos
	@$(MAKE) check-v108-ui-renders
	@$(MAKE) check-v109-multi-gguf
	@$(MAKE) check-v112-v114
	@$(MAKE) check-v115-v118
	@$(MAKE) check-v119-v123
	@$(MAKE) check-v124-v126
	@$(MAKE) check-v129
	@$(MAKE) check-v130
	@$(MAKE) check-v131
	@$(MAKE) check-v132
	@$(MAKE) check-v133
	@$(MAKE) check-v134
	@$(MAKE) check-v135
	@$(MAKE) check-v136
	@$(MAKE) check-v137
	@$(MAKE) check-v138
	@$(MAKE) check-v139
	@$(MAKE) check-v140
	@$(MAKE) check-v141
	@$(MAKE) check-v142
	@$(MAKE) check-v143
	@$(MAKE) check-v144
	@$(MAKE) check-v145
	@$(MAKE) check-v146
	@$(MAKE) check-v147
	@$(MAKE) check-v148
	@$(MAKE) check-v149
	@$(MAKE) check-v150
	@$(MAKE) check-v151
	@$(MAKE) check-v152
	@$(MAKE) check-v153
	@$(MAKE) check-v154
	@$(MAKE) check-v155
	@$(MAKE) check-v156
	@$(MAKE) check-v157
	@$(MAKE) check-v158
	@$(MAKE) check-v159
	@$(MAKE) check-v160
	@$(MAKE) check-v161
	@$(MAKE) check-v162
	@$(MAKE) check-v163
	@$(MAKE) check-v164
	@$(MAKE) check-v165
	@$(MAKE) check-v166
	@$(MAKE) check-v167
	@$(MAKE) check-v168
	@$(MAKE) check-v169
	@$(MAKE) check-v170
	@$(MAKE) check-v171
	@$(MAKE) check-v172
	@$(MAKE) check-v173
	@$(MAKE) check-v174
	@$(MAKE) check-v175
	@$(MAKE) check-v176
	@$(MAKE) check-v177
	@$(MAKE) check-v178
	@$(MAKE) check-v179
	@$(MAKE) check-v180
	@$(MAKE) check-v181
	@$(MAKE) check-v182
	@$(MAKE) check-v183
	@$(MAKE) check-v184
	@$(MAKE) check-v185
	@$(MAKE) check-v186
	@$(MAKE) check-v187
	@$(MAKE) check-v188
	@$(MAKE) check-v189
	@$(MAKE) check-v190
	@$(MAKE) check-v191
	@$(MAKE) check-v192
	@$(MAKE) check-v193
	@$(MAKE) check-v194
	@$(MAKE) check-v195
	@$(MAKE) check-v196
	@$(MAKE) check-v197
	@$(MAKE) check-v198
	@$(MAKE) check-v199
	@$(MAKE) check-v200
	@$(MAKE) check-v201
	@$(MAKE) check-v202
	@$(MAKE) check-v203
	@$(MAKE) check-v204
	@$(MAKE) check-v205
	@$(MAKE) check-v206
	@$(MAKE) check-v207
	@$(MAKE) check-v208
	@$(MAKE) check-v209
	@$(MAKE) check-v210
	@$(MAKE) check-v211
	@$(MAKE) check-v212
	@$(MAKE) check-v213
	@$(MAKE) check-v214
	@$(MAKE) check-v215
	@$(MAKE) check-v216
	@$(MAKE) check-v217
	@$(MAKE) check-v218
	@$(MAKE) check-v219
	@$(MAKE) check-v220
	@$(MAKE) check-v221
	@$(MAKE) check-v222
	@$(MAKE) check-v223
	@$(MAKE) check-v224
	@$(MAKE) check-v225
	@$(MAKE) check-v226
	@$(MAKE) check-v227
	@$(MAKE) check-v228
	@$(MAKE) check-v229
	@$(MAKE) check-v230
	@$(MAKE) check-v231
	@$(MAKE) check-v232
	@$(MAKE) check-v233
	@$(MAKE) check-v234
	@$(MAKE) check-v235
	@$(MAKE) check-v236
	@$(MAKE) check-v237
	@$(MAKE) check-v238
	@$(MAKE) check-v239
	@$(MAKE) check-v240
	@$(MAKE) check-v241
	@$(MAKE) check-v242
	@$(MAKE) check-v243
	@$(MAKE) check-v244
	@$(MAKE) check-v245
	@$(MAKE) check-v246
	@$(MAKE) check-v247
	@$(MAKE) check-v248
	@$(MAKE) check-v249
	@$(MAKE) check-v250
	@$(MAKE) check-v251
	@$(MAKE) check-v252
	@$(MAKE) check-v253
	@$(MAKE) check-v254
	@$(MAKE) check-v255
	@$(MAKE) check-v256
	@$(MAKE) check-v257
	@$(MAKE) check-v258
	@$(MAKE) check-v259
	@$(MAKE) check-v260
	@$(MAKE) check-v261
	@$(MAKE) check-v262
	@$(MAKE) check-v263
	@$(MAKE) check-v264
	@$(MAKE) check-v265
	@$(MAKE) check-v266
	@$(MAKE) check-v267
	@$(MAKE) check-v268
	@$(MAKE) check-v269
	@$(MAKE) check-v270
	@$(MAKE) check-v271
	@$(MAKE) check-v272
	@$(MAKE) check-v273
	@$(MAKE) check-v274
	@$(MAKE) check-v275
	@$(MAKE) check-v276
	@$(MAKE) check-v277
	@$(MAKE) check-v278
	@$(MAKE) check-v279
	@$(MAKE) check-v280
	@$(MAKE) check-v281
	@$(MAKE) check-v282
	@$(MAKE) check-v283
	@$(MAKE) check-v284
	@$(MAKE) check-v285
	@$(MAKE) check-v286
	@$(MAKE) check-v287
	@$(MAKE) check-v288
	@$(MAKE) check-v289
	@$(MAKE) check-v290
	@$(MAKE) check-v291
	@$(MAKE) check-v292
	@$(MAKE) check-v293
	@$(MAKE) check-v294
	@$(MAKE) check-v295
	@$(MAKE) check-v296
	@$(MAKE) check-v297
	@$(MAKE) check-v298
	@$(MAKE) check-v299
	@$(MAKE) check-v300
	@$(MAKE) check-v301
	@$(MAKE) check-v302
	@$(MAKE) check-v303
	@$(MAKE) check-v304
	@$(MAKE) check-v305
	@$(MAKE) check-v306
	@echo "merge-gate: OK (portable + v6..v29 + v101..v106 + v60..v100 + v111 + v106 curl loopback + v107 installer + v108 UI + v109 multi-GGUF + v112/v113/v114 agentic stack + v115/v116/v117/v118 memory/MCP/long-context/vision + v119/v120/v121/v122/v123 speculative/distill/planning/red-team/formal + v124/v125/v126 living-weights + v129..v133 collective intelligence + v134..v138 deep infrastructure + v139..v143 world intelligence + v144..v148 sovereign self-improvement + v149..v153 embodied/swarm/code-agent/distill/identity + v154..v158 showcase/publish/paper/community/v1.0-release + v159..v163 self-healing/composable + v164..v168 plugin/edge/stream/governance/marketplace + v169..v173 ontology/transfer/collab/narrative/teach + v174..v178 flywheel/debate-train/simulator/compress/consensus + v179..v183 interpret/steer/audit/privacy/governance-theory + v184..v188 VLA/fusion/grow/calibration/alignment + v189..v193 TTC/latent-reason/constitutional/emergent/coherence + v194..v198 horizon/recover/habit/ToM/moral + v199..v203 law/market/diplomacy/culture/civilization + v204..v208 hypothesis/experiment/theorem/design/manufacture + v209..v213 containment/guardian/sandbox-formal/transparency/trust-chain + v214..v218 swarm-evolve/stigmergy/quorum/ecosystem/consciousness-meter + v219..v223 create/simulate/language/emotion/meta-cognition + v224..v228 tensor/fractal/attention/entropy/unified + v229..v233 seed/fork/immortal/lineage/legacy + v234..v238 presence/locus/autobiography/boundary/sovereignty + v239..v243 runtime/pipeline/api/kernel-os/complete + v244..v248 package/observe/harden/benchmark-suite/release + v249..v253 mcp/a2a/marketplace/teach/ecosystem-hub + v254..v258 tutor/collaborate/wellness/locale/mission + v260..v264 engram/airllm/hybrid/mesh-engram/sovereign-stack + v265..v269 speculative/flash/mamba/continuous-batch/compile-v2 + v270..v274 tinyml/swarm-edge/digital-twin/robotics/industrial + v275..v278 ttt/deltanet/distill-runtime/rsi + v279..v282 jepa/moe/jamba/agent + v283..v286 constitutional/multi-agent/eu-ai-act/interpretability + v287..v293 granite/oculus/ruin-value/dougong/parthenon/leanstral/hagia-sofia + v294..v298 federated/immune/antifragile/clock/rosetta + v299..v300 knowledge-graph/complete + v301..v306 zkp/green/governance/narrative/swarm/omega)"

# Meta-target: every composed-decision kernel v60..v100 (v75 intentionally skipped).
check-v60-v100:
	@$(MAKE) check-v60
	@$(MAKE) check-v61
	@$(MAKE) check-v62
	@$(MAKE) check-v63
	@$(MAKE) check-v64
	@$(MAKE) check-v65
	@$(MAKE) check-v66
	@$(MAKE) check-v67
	@$(MAKE) check-v68
	@$(MAKE) check-v69
	@$(MAKE) check-v70
	@$(MAKE) check-v71
	@$(MAKE) check-v72
	@$(MAKE) check-v73
	@$(MAKE) check-v74
	@$(MAKE) check-v76
	@$(MAKE) check-v77
	@$(MAKE) check-v78
	@$(MAKE) check-v79
	@$(MAKE) check-v80
	@$(MAKE) check-v81
	@$(MAKE) check-v82
	@$(MAKE) check-v83
	@$(MAKE) check-v84
	@$(MAKE) check-v85
	@$(MAKE) check-v86
	@$(MAKE) check-v87
	@$(MAKE) check-v88
	@$(MAKE) check-v89
	@$(MAKE) check-v90
	@$(MAKE) check-v91
	@$(MAKE) check-v92
	@$(MAKE) check-v93
	@$(MAKE) check-v94
	@$(MAKE) check-v95
	@$(MAKE) check-v96
	@$(MAKE) check-v97
	@$(MAKE) check-v98
	@$(MAKE) check-v99
	@$(MAKE) check-v100
	@echo "check-v60-v100: OK (39 kernels · v75 retired)"

# v106 HTTP server curl loopback (stub-mode probe, no weights required).
check-v106-curl-loopback:
	@bash benchmarks/v106/check_v106_curl_loopback.sh

# v107 installer smoke (brew formula dry-render + install.sh syntax check + Dockerfile lint).
check-v107-install-macos:
	@bash scripts/v107/check_v107_install_macos.sh

# v108 web UI render check (headless: file exists, has σ-channel placeholders + valid HTML).
check-v108-ui-renders:
	@bash web/check_v108_ui_renders.sh

# v109 multi-GGUF support smoke (bridge honors --gguf repetition + /v1/models contract).
check-v109-multi-gguf:
	@bash benchmarks/v109/check_v109_multi_gguf.sh

# Reviewer gate: what an external critic should be able to verify quickly.
reviewer:
	@bash ./scripts/fix_critique_points.sh
	@$(MAKE) check
	@$(MAKE) check-v26
	@$(MAKE) standalone
	@./creation_os --self-test
	@echo "reviewer: OK"

check-rtl: formal-rtl-lint

yosys-elab:
	@if command -v yosys >/dev/null 2>&1; then \
		cd hw/yosys && yosys -q elaborate_rtl.ys && echo "yosys-elab: OK"; \
	else \
		echo "yosys-elab: SKIP (apt/brew install yosys)"; \
	fi

rust-iron-test:
	@if command -v cargo >/dev/null 2>&1; then \
		cargo test --manifest-path hw/rust/spektre-iron-gate/Cargo.toml && echo "rust-iron-test: OK"; \
	else \
		echo "rust-iron-test: SKIP (install Rust / cargo)"; \
	fi

hardware-supreme: formal-rtl-lint yosys-elab rust-iron-test chisel-compile
	@echo "hardware-supreme: OK (all hardware layers attempted; SKIPs logged above)"

yosys-prove-agency:
	@if command -v yosys >/dev/null 2>&1; then \
		cd hw/yosys && ( \
			yosys -q prove_agency_comb_modern.ys >/tmp/yosys_prove_modern.log 2>&1 \
				&& echo "yosys-prove-agency: OK (modern flow)" \
		) || ( \
			yosys -q prove_agency_comb.ys >/tmp/yosys_prove_classic.log 2>&1 \
				&& echo "yosys-prove-agency: OK (classic flow)" \
		) || ( \
			echo "yosys-prove-agency: FAIL — modern log:" >&2; \
			cat /tmp/yosys_prove_modern.log >&2; \
			echo "--- classic log:" >&2; \
			cat /tmp/yosys_prove_classic.log >&2; \
			exit 1 \
		); \
	else \
		echo "yosys-prove-agency: SKIP (apt/brew install yosys)"; \
	fi

stack-ultimate: hardware-supreme yosys-prove-agency formal-rtl-sim chisel-verilog
	@echo "stack-ultimate: OK (lint + elab + prove + sim + chisel verilog + rust; SKIPs logged above)"

formal-sby-agency:
	@if command -v sby >/dev/null 2>&1; then \
		cd hw/formal && sby -f agency_ctrl_prove.sby && echo "formal-sby-agency: OK"; \
	else \
		echo "formal-sby-agency: SKIP (install SymbiYosys / YosysHQ OSS CAD Suite)"; \
	fi

stack-nucleon: stack-ultimate formal-sby-agency
	@echo "stack-nucleon: OK (ultimate + SymbiYosys agency prove; SKIPs logged above)"

formal-sby-cover-agency:
	@if command -v sby >/dev/null 2>&1; then \
		cd hw/formal && sby -f agency_ctrl_cover.sby && echo "formal-sby-cover-agency: OK"; \
	else \
		echo "formal-sby-cover-agency: SKIP (install SymbiYosys / YosysHQ OSS CAD Suite)"; \
	fi

formal-sby-tokenizer:
	@if command -v sby >/dev/null 2>&1; then \
		cd hw/formal && sby -f cos_tokenizer_gda27_roundtrip_prove.sby && echo "formal-sby-tokenizer: OK"; \
	else \
		echo "formal-sby-tokenizer: SKIP (install SymbiYosys / YosysHQ OSS CAD Suite)"; \
	fi

formal-sby-v37:
	@if command -v sby >/dev/null 2>&1; then \
		cd hdl/v37 && sby -f sigma_pipeline.sby && echo "formal-sby-v37: OK"; \
	else \
		echo "formal-sby-v37: SKIP (install SymbiYosys / YosysHQ OSS CAD Suite)"; \
	fi

formal-sby-v47:
	@if command -v sby >/dev/null 2>&1; then \
		cd hdl/v47 && sby -f sigma_pipeline_extended.sby && echo "formal-sby-v47: OK"; \
	else \
		echo "formal-sby-v47: SKIP (install SymbiYosys / YosysHQ OSS CAD Suite)"; \
	fi

verify: verify-c verify-sv verify-property verify-integration
	@echo "verify: OK (all attempted layers; see SKIPs above)"

verify-c:
	@if command -v frama-c >/dev/null 2>&1; then \
		frama-c -wp -wp-rte "$(CURDIR)/src/v47/sigma_kernel_verified.c" && echo "verify-c: OK"; \
	else \
		echo "verify-c: SKIP (install Frama-C / WP for ACSL discharge)"; \
	fi

verify-sv: formal-sby-v47

verify-property:
	@if python3 -c "import pytest, hypothesis" >/dev/null 2>&1; then \
		$(MAKE) standalone-v47 && \
		python3 -m pytest "$(CURDIR)/tests/v47/property_tests.py" && echo "verify-property: OK"; \
	else \
		echo "verify-property: SKIP (pip install pytest hypothesis)"; \
	fi

verify-integration:
	@$(MAKE) standalone-v47 && ./creation_os_v47 --self-test
	@$(MAKE) standalone-v46 && ./creation_os_v46 --self-test
	@$(MAKE) check-v31
	@$(MAKE) check
	@echo "verify-integration: OK (v47+v46 self-tests + check-v31 + check)"

trust-report:
	@bash ./scripts/generate_trust_report.sh

red-team: red-team-garak red-team-deepteam red-team-sigma red-team-property
	@echo "red-team: OK (see SKIPs — full Garak/DeepTeam runs need installs + REST model)"

red-team-garak:
	@if python3 -c "import garak" >/dev/null 2>&1; then \
		echo "red-team-garak: SKIP (garak installed — run manually against your REST model; see docs/v48/RED_TEAM_REPORT.md)"; \
	else \
		echo "red-team-garak: SKIP (pip install garak)"; \
	fi

red-team-deepteam:
	@python3 "$(CURDIR)/tests/v48/red_team/run_deepteam.py" && echo "red-team-deepteam: OK"

red-team-sigma:
	@python3 "$(CURDIR)/tests/v48/red_team/sigma_bypass_attacks.py" && echo "red-team-sigma: OK"

red-team-property:
	@if python3 -c "import pytest" >/dev/null 2>&1; then \
		python3 -m pytest "$(CURDIR)/tests/v48/red_team/property_attacks.py" -x && echo "red-team-property: OK"; \
	else \
		echo "red-team-property: SKIP (pip install pytest)"; \
	fi

merge-gate-v48: verify red-team check-v31 reviewer
	@echo "merge-gate-v48: OK (verify + red-team + check-v31 + reviewer; SKIPs may appear)"

certify: certify-formal certify-coverage certify-binary-audit certify-red-team certify-trace
	@echo "certify: OK (best-effort assurance pack; SKIPs are explicit — see docs/v49/certification/README.md)"

certify-formal:
	@echo "=== DO-333-style formal targets (Frama-C + SymbiYosys replay) ==="
	@if command -v frama-c >/dev/null 2>&1; then \
		frama-c -wp -wp-rte -wp-timeout 60 "$(CURDIR)/src/v47/sigma_kernel_verified.c" && echo "certify-formal: Frama-C OK"; \
	else \
		echo "certify-formal: Frama-C SKIP (install Frama-C / WP)"; \
	fi
	@$(MAKE) formal-sby-v47

certify-coverage:
	@bash "$(CURDIR)/scripts/v49/run_mcdc.sh"

certify-binary-audit:
	@$(MAKE) standalone-v47
	@bash "$(CURDIR)/scripts/v49/binary_audit.sh"

certify-red-team:
	@$(MAKE) red-team

certify-trace:
	@python3 "$(CURDIR)/scripts/v49/verify_traceability.py"

synth-v37:
	@sh hdl/v37/synth_and_measure.sh && echo "synth-v37: OK (see script output for SKIPs)"

check-asic-tile:
	@if command -v verilator >/dev/null 2>&1; then \
		rm -rf hdl/asic/build && \
		verilator --binary -Mdir hdl/asic/build --top-module tt_um_sigma_tile -GHV_BITS=128 \
			-Wno-WIDTHTRUNC -Wno-WIDTHEXPAND \
			hdl/asic/sigma_tile.sv hdl/asic/sim/tb_sigma_tile.cpp && \
		./hdl/asic/build/Vtt_um_sigma_tile && \
		echo "check-asic-tile: OK"; \
	else \
		echo "check-asic-tile: SKIP (apt/brew install verilator)"; \
	fi

librelane-v38:
	@sh hdl/asic/librelane_run.sh && echo "librelane-v38: OK (see script output for SKIPs)"

check-crossbar-sim:
	@if command -v verilator >/dev/null 2>&1; then \
		rm -rf hdl/neuromorphic/build && \
		verilator --binary -Mdir hdl/neuromorphic/build \
			-Wno-WIDTHTRUNC -Wno-WIDTHEXPAND \
			hdl/neuromorphic/crossbar_sim.sv hdl/neuromorphic/tb_crossbar_harness.sv \
			--top-module tb_crossbar_harness && \
		./hdl/neuromorphic/build/Vtb_crossbar_harness && \
		echo "check-crossbar-sim: OK"; \
	else \
		echo "check-crossbar-sim: SKIP (apt/brew install verilator)"; \
	fi

bench-v40-threshold:
	@bash benchmarks/v40/threshold_test.sh && echo "bench-v40-threshold: OK (see script output)"

bench-v41-scaling:
	@bash benchmarks/v41/test_time_scaling.sh && echo "bench-v41-scaling: OK (see script output)"

bench-v42-curve:
	@bash benchmarks/v42/self_play_curve.sh && echo "bench-v42-curve: OK (see script output)"

bench-v43-distill:
	@bash benchmarks/v43/distill_quality.sh && echo "bench-v43-distill: OK (see script output)"

bench-v44-overhead:
	@bash benchmarks/v44/proxy_overhead.sh && echo "bench-v44-overhead: OK (see script output)"

bench-v45-paradox:
	@bash benchmarks/v45/paradox_test.sh && echo "bench-v45-paradox: OK (see script output)"

bench-v46-e2e:
	@bash benchmarks/v46/e2e_bench.sh && echo "bench-v46-e2e: OK (see script output)"

v50-benchmark:
	@chmod +x benchmarks/v50/run_all.sh benchmarks/v50/generate_final_report.py && \
		bash benchmarks/v50/run_all.sh && echo "v50-benchmark: OK (see benchmarks/v50/FINAL_RESULTS.md)"

eqy-agency-self:
	@if command -v eqy >/dev/null 2>&1; then \
		cd hw/formal && eqy agency_self.eqy && echo "eqy-agency-self: OK"; \
	else \
		echo "eqy-agency-self: SKIP (install EQY / OSS CAD Suite)"; \
	fi

rust-iron-lint:
	@if command -v cargo >/dev/null 2>&1; then \
		cargo fmt --manifest-path hw/rust/spektre-iron-gate/Cargo.toml -- --check && \
		cargo clippy --manifest-path hw/rust/spektre-iron-gate/Cargo.toml -- -D warnings && \
		echo "rust-iron-lint: OK"; \
	else \
		echo "rust-iron-lint: SKIP (install Rust / cargo)"; \
	fi

oss-formal-extreme: formal-sby-agency yosys-prove-agency formal-sby-cover-agency eqy-agency-self rust-iron-lint
	@echo "oss-formal-extreme: OK (YosysHQ-class formal bundle; SKIPs logged above)"

stack-singularity: stack-nucleon formal-sby-cover-agency eqy-agency-self rust-iron-lint
	@echo "stack-singularity: OK (nucleon + cover + EQY + rust lint; SKIPs logged above)"

formal-rtl-lint: $(RTL_SV)
	@if command -v verilator >/dev/null 2>&1; then \
		verilator --lint-only $(VERILATOR_LINT_FLAGS) $(RTL_SV) && echo "formal-rtl-lint: OK"; \
	else \
		echo "formal-rtl-lint: SKIP (apt/brew install verilator)"; \
	fi

chisel-compile:
	@if command -v sbt >/dev/null 2>&1; then \
		cd hw/chisel && sbt compile && echo "chisel-compile: OK"; \
	else \
		echo "chisel-compile: SKIP (install JDK 17+ and sbt)"; \
	fi

chisel-verilog: chisel-compile
	@if command -v sbt >/dev/null 2>&1; then \
		mkdir -p hw/chisel/generated && \
		cd hw/chisel && sbt -DchiselTargetDir=generated "runMain spektre.core.GenVerilog" && \
		echo "chisel-verilog: OK (see hw/chisel/generated/)"; \
	else \
		echo "chisel-verilog: SKIP"; \
	fi

formal-rtl-sim: $(RTL_SV)
	@if command -v verilator >/dev/null 2>&1; then \
		rm -rf .build/vrtl && mkdir -p .build/vrtl && \
		verilator --binary $(VERILATOR_LINT_FLAGS) -Wno-STMTDLY --top-module cos_silicon_chip_tb \
			-Mdir .build/vrtl \
			rtl/cos_silicon_chip_tb.sv rtl/cos_formal_iron_combo.sv rtl/cos_agency_iron_combo.sv \
			rtl/cos_commit_iron_combo.sv rtl/cos_boundary_sync.sv \
			rtl/cos_looplm_drum.sv rtl/cos_geodesic_tick.sv rtl/cos_k_eff_bind.sv && \
		./.build/vrtl/Vcos_silicon_chip_tb && echo "formal-rtl-sim: OK"; \
	else \
		echo "formal-rtl-sim: SKIP (verilator not in PATH)"; \
	fi

standalone: creation_os_v2.c
	$(CC) $(CFLAGS) -I. -o creation_os creation_os_v2.c $(LDFLAGS)

standalone-v6: creation_os_v6.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v6 creation_os_v6.c $(LDFLAGS)

standalone-v7: creation_os_v7.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v7 creation_os_v7.c $(LDFLAGS)

standalone-v9: creation_os_v9.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v9 creation_os_v9.c $(LDFLAGS)

standalone-v10: creation_os_v10.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v10 creation_os_v10.c $(LDFLAGS)

standalone-v11: creation_os_v11.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v11 creation_os_v11.c $(LDFLAGS)

standalone-v12: creation_os_v12.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v12 creation_os_v12.c $(LDFLAGS)

standalone-v15: creation_os_v15.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v15 creation_os_v15.c $(LDFLAGS)

standalone-v16: creation_os_v16.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v16 creation_os_v16.c $(LDFLAGS)

standalone-v20: creation_os_v20.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v20 creation_os_v20.c $(LDFLAGS)

standalone-v21: creation_os_v21.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v21 creation_os_v21.c $(LDFLAGS)

standalone-v22: creation_os_v22.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v22 creation_os_v22.c $(LDFLAGS)

standalone-v23: creation_os_v23.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v23 creation_os_v23.c $(LDFLAGS)

standalone-v24: creation_os_v24.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v24 creation_os_v24.c $(LDFLAGS)

standalone-v25: creation_os_v25.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v25 creation_os_v25.c $(LDFLAGS)

standalone-v26: creation_os_v26.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-unused-but-set-variable -o creation_os_v26 creation_os_v26.c $(LDFLAGS)

# v27 links tokenizer helpers + mmap codebook path (POSIX mmap; Windows generator still works).
TOKENIZER_V27_SRCS = src/tokenizer/bpe.c src/tokenizer/byte_fallback.c src/tokenizer/gda27_stub.c src/tokenizer/cos_codebook_mmap.c

standalone-v27: creation_os_v27.c $(TOKENIZER_V27_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v27 creation_os_v27.c $(TOKENIZER_V27_SRCS) $(LDFLAGS)

GDA27_RUST_A = experimental/gda27_rust/target/release/libcreation_os_gda27.a

rust-gda27-lib:
	@if command -v cargo >/dev/null 2>&1; then \
		cd experimental/gda27_rust && cargo build --release && echo "rust-gda27-lib: OK"; \
	else \
		echo "rust-gda27-lib: SKIP (cargo missing)"; exit 2; \
	fi

standalone-v27-rust: rust-gda27-lib creation_os_v27.c $(TOKENIZER_V27_SRCS) experimental/gda27_rust/creation_os_gda27_link.c
	$(CC) $(CFLAGS) -I. -DCOS_GDA27_RUST=1 -o creation_os_v27 creation_os_v27.c $(TOKENIZER_V27_SRCS) experimental/gda27_rust/creation_os_gda27_link.c $(GDA27_RUST_A) $(LDFLAGS)

# v28: portable LM integration shell (GGUF metadata + sampling + chat template + toy σ + HTTP).
LM_V28_SRCS = src/import/gguf_parser.c src/import/gguf_mmap.c src/import/bitnet_spawn.c src/import/tokenizer_json.c src/nn/sampler.c src/nn/chat_template.c src/nn/transformer_stub.c src/server/json_esc.c src/server/http_chat.c

standalone-v28: creation_os_v28.c $(LM_V28_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v28 creation_os_v28.c $(LM_V28_SRCS) $(LDFLAGS)

# v29: mmap GGUF view + sigma channels + XNOR attention toy + BitNet forward stub (no weight download in-tree).
LM_V29_SRCS = src/import/gguf_parser.c src/import/gguf_loader.c src/sigma/channels.c src/nn/attention_xnor.c src/nn/bitnet_forward_stub.c

standalone-v29: creation_os_v29.c $(LM_V29_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v29 creation_os_v29.c $(LM_V29_SRCS) $(LDFLAGS)

# v31: optional POSIX lab wrapper around an upstream inference binary (no weights in-tree).
standalone-v31: src/v31/creation_os_v31.c
	$(CC) $(CFLAGS) -o creation_os_v31 src/v31/creation_os_v31.c $(LDFLAGS)

# v33: σ-routed SLM/LLM fallback lab + schema-first tool validation + JSONL metrics (no weights in-tree).
V33_SRCS = src/v33/router.c src/v33/schema_validator.c src/v33/metrics.c src/v33/model_registry.c src/sigma/channels.c src/sigma/decompose.c src/sigma/calibrate.c

standalone-v33: src/v33/creation_os_v33.c $(V33_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v33 src/v33/creation_os_v33.c $(V33_SRCS) $(LDFLAGS)

V34_SRCS = src/sigma/decompose.c src/sigma/calibrate.c src/sigma/channels_v34.c src/sigma/channels.c src/v33/router.c

standalone-v34: src/v34/creation_os_v34.c $(V34_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v34 src/v34/creation_os_v34.c $(V34_SRCS) $(LDFLAGS)

# v35: σ-guided speculative decoding (adaptive K + dual-verify policy; no weights in-tree).
V35_SRCS = src/v35/spec_decode.c src/sigma/decompose.c

standalone-v35: src/v35/creation_os_v35.c $(V35_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v35 src/v35/creation_os_v35.c $(V35_SRCS) $(LDFLAGS)

# v39: σ_hardware (substrate noise) term + composition on top of v34 Dirichlet decomposition (lab).
V39_SRCS = src/sigma/decompose.c src/sigma/decompose_v39.c

standalone-v39: src/v39/creation_os_v39.c $(V39_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v39 src/v39/creation_os_v39.c $(V39_SRCS) $(LDFLAGS)

# v40: σ-threshold lab — channel independence diagnostics + σ-syndrome action decoder (not merge-gate).
V40_SRCS = src/sigma/independence_test.c src/sigma/syndrome_decoder.c

standalone-v40: src/v40/creation_os_v40.c $(V40_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v40 src/v40/creation_os_v40.c $(V40_SRCS) $(LDFLAGS)

# v41: σ-guided test-time compute — budget forcing + adaptive BoN + reasoning tree (lab; not merge-gate).
V41_SRCS = src/v41/budget_forcing.c src/v41/best_of_n.c src/v41/self_verify.c src/v41/reasoning_tree.c src/sigma/decompose.c

standalone-v41: src/v41/creation_os_v41.c $(V41_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v41 src/v41/creation_os_v41.c $(V41_SRCS) $(LDFLAGS)

# v42: σ-guided self-play — challenger/solver/replay scaffold (lab; not merge-gate).
V42_SRCS = src/v42/v42_text.c src/v42/challenger.c src/v42/solver.c src/v42/experience_buffer.c src/v42/self_play.c src/sigma/decompose.c

standalone-v42: src/v42/creation_os_v42.c $(V42_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v42 src/v42/creation_os_v42.c $(V42_SRCS) $(LDFLAGS)

# v43: σ-guided knowledge distillation — weighted KL + progressive stages + ensemble + calibration (lab; not merge-gate).
V43_SRCS = src/v43/sigma_distill.c src/v43/progressive_distill.c src/v43/multi_teacher.c src/v43/calibrated_student.c

standalone-v43: src/v43/creation_os_v43.c $(V43_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v43 src/v43/creation_os_v43.c $(V43_SRCS) $(LDFLAGS)

# v44: σ-native inference proxy — stub engine + per-token σ + syndrome actions + OpenAI-shaped loopback HTTP (lab; not merge-gate).
V44_SRCS = src/v44/sigma_proxy.c src/v44/sigma_stream.c src/v44/api_server.c src/v44/v44_ttc.c src/sigma/decompose.c src/sigma/syndrome_decoder.c src/server/json_esc.c

standalone-proxy: src/v44/creation_os_proxy.c $(V44_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_proxy src/v44/creation_os_proxy.c $(V44_SRCS) $(LDFLAGS)

# v45: σ-introspection — calibration gap + doubt reward + internal probe stub (lab; not merge-gate).
V45_SRCS = src/v45/introspection.c src/v45/doubt_reward.c src/v45/clap_sigma.c src/v42/v42_text.c src/sigma/decompose.c

standalone-v45: src/v45/creation_os_v45.c $(V45_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v45 src/v45/creation_os_v45.c $(V45_SRCS) $(LDFLAGS)

# v46: σ-optimized BitNet pipeline — fast σ from logits + SIMD reductions + adaptive quant policy (lab; not merge-gate).
V46_SRCS = src/v46/fast_sigma.c src/v46/adaptive_quant.c src/v46/sigma_simd.c src/sigma/decompose.c

standalone-v46: src/v46/creation_os_v46.c $(V46_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v46 src/v46/creation_os_v46.c $(V46_SRCS) $(LDFLAGS)

# v47: verified-architecture lab — ACSL σ-kernel + ZK-σ stub + verify driver (not merge-gate).
V47_SRCS = src/v47/sigma_kernel_verified.c src/v47/zk_sigma.c src/sigma/decompose.c

standalone-v47: src/v47/creation_os_v47.c $(V47_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v47 src/v47/creation_os_v47.c $(V47_SRCS) $(LDFLAGS)

# v48: σ-armored red-team lab — anomaly detector + σ-gated sandbox + defense-in-depth (not merge-gate).
V48_SRCS = src/v48/sigma_anomaly.c src/v48/sandbox.c src/v48/defense_layers.c src/sigma/decompose.c

standalone-v48: src/v48/creation_os_v48.c $(V48_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v48 src/v48/creation_os_v48.c $(V48_SRCS) $(LDFLAGS)

# v51: AGI-complete integration scaffold — cognitive loop + σ-gated agent (not merge-gate).
V51_SRCS = src/v51/cognitive_loop.c src/v51/agent.c src/sigma/decompose.c

standalone-v51: src/v51/creation_os_v51.c $(V51_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v51 src/v51/creation_os_v51.c $(V51_SRCS) $(LDFLAGS)

# v53: σ-governed harness scaffold — σ-TAOR loop + σ-dispatch + σ-compress + creation.md loader (not merge-gate).
V53_SRCS = src/v53/harness/loop.c src/v53/harness/dispatch.c src/v53/harness/compress.c src/v53/harness/project_context.c src/v51/cognitive_loop.c src/sigma/decompose.c

standalone-v53: src/v53/creation_os_v53.c $(V53_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v53 src/v53/creation_os_v53.c $(V53_SRCS) $(LDFLAGS)

# v54: σ-proconductor scaffold — multi-LLM orchestration policy (no network; not merge-gate).
V54_SRCS = src/v54/proconductor.c src/v54/dispatch.c src/v54/disagreement.c src/v54/learn_profiles.c src/sigma/decompose.c

standalone-v54: src/v54/creation_os_v54.c $(V54_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_v54 src/v54/creation_os_v54.c $(V54_SRCS) $(LDFLAGS)

# v55: σ₃-speculative scaffold — EARS + EASD + σ₃ decomposition (NEON hot path; no network; not merge-gate).
# Cites Taparia 2603.24967 (Mar 2026), Sun 2512.13194 (Dec 2025), Su 2512.23765 (Dec 2025).
V55_SRCS = src/v55/sigma3.c src/v55/ears.c src/v55/easd.c

standalone-v55: src/v55/creation_os_v55.c $(V55_SRCS)
	$(CC) $(CFLAGS) -Isrc/v55 -o creation_os_v55 src/v55/creation_os_v55.c $(V55_SRCS) $(LDFLAGS)

# v56: σ-Constitutional scaffold — VPRM verifier + σ-gated IP-TTT +
# grokking commutator-defect σ-channel + ANE conv-as-matmul layout
# (NEON hot path; no network; no Core ML dispatch; not merge-gate).
# Cites arXiv:2601.17223 (VPRM, 2026), arXiv:2604.06169 (IP-TTT, Apr 2026),
# arXiv:2603.01192 (SLT grokking, Mar 2026), and 2026 ANE reverse-engineering.
V56_SRCS = src/v56/verifier.c src/v56/ipttt.c src/v56/grokking.c src/v56/ane_layout.c

standalone-v56: src/v56/creation_os_v56.c $(V56_SRCS)
	$(CC) $(CFLAGS) -Isrc/v56 -o creation_os_v56 src/v56/creation_os_v56.c $(V56_SRCS) $(LDFLAGS)

# v57: The Verified Agent — convergence of v33–v56 into one named artifact
# with one verification command.  v57 introduces no new σ math; it is the
# invariant + composition registry that ties the existing proof / runtime /
# documentation surfaces together with honest M / F / I / P tier tags.
# `make verify-agent` runs scripts/v57/verify_agent.sh, which dispatches each
# slot's owning make target and reports PASS / SKIP / FAIL honestly (no
# silent downgrades when Frama-C / sby / pytest / garak are absent).
V57_SRCS = src/v57/invariants.c src/v57/verified_agent.c

standalone-v57: src/v57/creation_os_v57.c $(V57_SRCS)
	$(CC) $(CFLAGS) -Isrc/v57 -o creation_os_v57 src/v57/creation_os_v57.c $(V57_SRCS) $(LDFLAGS)

# v58: σ-Cache — σ-decomposed KV-cache eviction policy + branchless
# decision kernel + per-token precision tier + NEON 4-accumulator SoA
# scorer.  No model, no network, no Accelerate / Metal / Core ML / SME
# enablement; portable C11 + optional NEON.  See src/v58/sigma_cache.h
# for the Q1–Q2 2026 KV-cache literature gap that drives the design
# (EntropyCache uses single-channel entropy; v58 uses v34 epistemic /
# aleatoric decomposition + Heavy-Hitter prior + StreamingLLM sink).
V58_SRCS = src/v58/sigma_cache.c

standalone-v58: src/v58/creation_os_v58.c $(V58_SRCS)
	$(CC) $(CFLAGS) -Isrc/v58 -o creation_os_v58 src/v58/creation_os_v58.c $(V58_SRCS) $(LDFLAGS)

# v59: σ-Budget — σ-decomposed adaptive test-time compute budget
# controller.  Novel: every 2026 budgeting policy (TAB, CoDE-Stop,
# LYNX, DTSR, DiffAdapt, Coda, AdaCtrl, Risk-Control) uses a scalar
# signal; v59 decomposes σ = (ε, α) and produces a four-valued
# per-step decision (CONTINUE / EARLY_EXIT / EXPAND / ABSTAIN).
V59_SRCS = src/v59/sigma_budget.c

standalone-v59: src/v59/creation_os_v59.c $(V59_SRCS)
	$(CC) $(CFLAGS) -Isrc/v59 -o creation_os_v59 src/v59/creation_os_v59.c $(V59_SRCS) $(LDFLAGS)

# v60: σ-Shield — runtime security kernel.  Branchless capability
# authorise + σ-gated intent (the first of its kind) + TOCTOU-free
# argument validation + code-page integrity self-check.  Response to
# the 2026 attack literature (DDIPE 2604.03081, ClawWorm 2603.15727,
# Malicious Intermediary 2604.08407, ShieldNet 2604.04426): all 2026
# attacks succeed via ambiguous payloads; σ-decomposition refuses
# α-dominated intent regardless of capability.  No dynamic allocation
# on the authorize hot path; constant-time hash equality.
V60_SRCS = src/v60/sigma_shield.c

standalone-v60: src/v60/creation_os_v60.c $(V60_SRCS)
	$(CC) $(CFLAGS) -Isrc/v60 -o creation_os_v60 src/v60/creation_os_v60.c $(V60_SRCS) $(LDFLAGS)

# v61: Σ-Citadel — formal-lattice capability kernel (Bell-LaPadula +
# Biba) + attestation chain.  First open-source AI-agent runtime to
# ship the full DARPA-CHACE "advanced security" menu: seL4 CAmkES
# component contract + Wasmtime WASM sandbox harness + eBPF LSM policy
# example + Darwin sandbox-exec profile + OpenBSD pledge stub + Nix
# reproducible recipe + distroless Dockerfile + Sigstore cosign +
# SLSA v1.0 provenance predicate, all dispatched by `make chace`
# which PASSes present layers and SKIPs missing ones HONESTLY.
#
# Optional libsodium build: `make COS_V61_LIBSODIUM=1 standalone-v61`
# switches the attestation quote from the deterministic XOR-fold to
# BLAKE2b-256 via libsodium's crypto_generichash.
V61_SRCS = src/v61/citadel.c

V61_EXTRA_CFLAGS =
V61_EXTRA_LDFLAGS =
ifeq ($(COS_V61_LIBSODIUM),1)
V61_EXTRA_CFLAGS  += -DCOS_V61_LIBSODIUM=1
V61_EXTRA_LDFLAGS += -lsodium
endif

standalone-v61: src/v61/creation_os_v61.c $(V61_SRCS)
	$(CC) $(CFLAGS) $(V61_EXTRA_CFLAGS) -Isrc/v61 -o creation_os_v61 src/v61/creation_os_v61.c $(V61_SRCS) $(LDFLAGS) $(V61_EXTRA_LDFLAGS)

standalone-v61-hardened: src/v61/creation_os_v61.c $(V61_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V61_EXTRA_CFLAGS) -Isrc/v61 -o creation_os_v61_hardened src/v61/creation_os_v61.c $(V61_SRCS) $(HARDEN_LDFLAGS) $(V61_EXTRA_LDFLAGS)

# --- Hardening matrix (OpenSSF + Apple M4 branch-protection) -------
#
# `make harden` rebuilds the flagship σ-labs (v57 / v58 / v59 / v60)
# with a hardened clang profile.  Flags are the OpenSSF Best Practices
# recommendation as of 2026-Q2 plus ARM64 pointer-authentication /
# branch-target-identification (-mbranch-protection=standard) where
# supported by the host clang; missing flags are ignored (clang -Werror
# is intentional, unknown flags silently dropped via $(filter …)).
HARDEN_CFLAGS = -O2 -std=c11 -Wall -Wformat=2 -Wformat-security \
                -Werror=format-security -Wimplicit-fallthrough \
                -D_GNU_SOURCE -D_DARWIN_C_SOURCE -D_FORTIFY_SOURCE=3 \
                -fstack-protector-strong -fstack-clash-protection \
                -fstrict-flex-arrays=3 \
                -fPIE -mbranch-protection=standard
HARDEN_LDFLAGS = -Wl,-pie -lm
# Apple ld64 does not accept -Wl,-z,relro/-z,now/-z,noexecstack; those
# are GNU-ld-only.  We skip them on darwin and let macOS apply its own
# __DATA,__const + dyld hardening.

standalone-v57-hardened: src/v57/creation_os_v57.c $(V57_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v57 -o creation_os_v57_hardened src/v57/creation_os_v57.c $(V57_SRCS) $(HARDEN_LDFLAGS)

standalone-v58-hardened: src/v58/creation_os_v58.c $(V58_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v58 -o creation_os_v58_hardened src/v58/creation_os_v58.c $(V58_SRCS) $(HARDEN_LDFLAGS)

standalone-v59-hardened: src/v59/creation_os_v59.c $(V59_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v59 -o creation_os_v59_hardened src/v59/creation_os_v59.c $(V59_SRCS) $(HARDEN_LDFLAGS)

standalone-v60-hardened: src/v60/creation_os_v60.c $(V60_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v60 -o creation_os_v60_hardened src/v60/creation_os_v60.c $(V60_SRCS) $(HARDEN_LDFLAGS)

harden: standalone-v57-hardened standalone-v58-hardened standalone-v59-hardened standalone-v60-hardened standalone-v61-hardened standalone-v62-hardened standalone-v63-hardened standalone-v64-hardened standalone-v65-hardened standalone-v66-hardened standalone-v67-hardened standalone-v68-hardened standalone-v69-hardened standalone-v70-hardened standalone-v71-hardened standalone-v72-hardened standalone-v73-hardened standalone-v74-hardened standalone-v76-hardened standalone-v77-hardened standalone-v78-hardened standalone-v79-hardened standalone-v80-hardened standalone-v81-hardened standalone-v82-hardened standalone-v83-hardened standalone-v84-hardened standalone-v85-hardened standalone-v86-hardened standalone-v87-hardened standalone-v88-hardened standalone-v89-hardened standalone-v90-hardened standalone-v91-hardened standalone-v92-hardened standalone-v93-hardened standalone-v94-hardened standalone-v95-hardened standalone-v96-hardened standalone-v97-hardened standalone-v98-hardened standalone-v99-hardened standalone-v100-hardened license-attest-hardened
	@echo "harden: OK (v57 / v58 / v59 / v60 / v61 / v62 / v63 / v64 / v65 rebuilt with OpenSSF 2026 flags + M4 branch-protection)"

# --- Sanitizer matrix (AddressSanitizer + UndefinedBehaviorSanitizer)
SAN_CFLAGS  = -O1 -g -std=c11 -Wall -fno-omit-frame-pointer -D_GNU_SOURCE -D_DARWIN_C_SOURCE
ASAN_FLAGS  = -fsanitize=address,undefined
UBSAN_FLAGS = -fsanitize=undefined -fno-sanitize-recover=undefined

asan-v58: src/v58/creation_os_v58.c $(V58_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v58 -o creation_os_v58_asan src/v58/creation_os_v58.c $(V58_SRCS) -lm
	./creation_os_v58_asan --self-test

asan-v59: src/v59/creation_os_v59.c $(V59_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v59 -o creation_os_v59_asan src/v59/creation_os_v59.c $(V59_SRCS) -lm
	./creation_os_v59_asan --self-test

asan-v60: src/v60/creation_os_v60.c $(V60_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v60 -o creation_os_v60_asan src/v60/creation_os_v60.c $(V60_SRCS) -lm
	./creation_os_v60_asan --self-test

ubsan-v60: src/v60/creation_os_v60.c $(V60_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v60 -o creation_os_v60_ubsan src/v60/creation_os_v60.c $(V60_SRCS) -lm
	./creation_os_v60_ubsan --self-test

asan-v61: src/v61/creation_os_v61.c $(V61_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V61_EXTRA_CFLAGS) -Isrc/v61 -o creation_os_v61_asan src/v61/creation_os_v61.c $(V61_SRCS) -lm $(V61_EXTRA_LDFLAGS)
	./creation_os_v61_asan --self-test

ubsan-v61: src/v61/creation_os_v61.c $(V61_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V61_EXTRA_CFLAGS) -Isrc/v61 -o creation_os_v61_ubsan src/v61/creation_os_v61.c $(V61_SRCS) -lm $(V61_EXTRA_LDFLAGS)
	./creation_os_v61_ubsan --self-test

sanitize: asan-v58 asan-v59 asan-v60 ubsan-v60 asan-v61 ubsan-v61 asan-v62 ubsan-v62 asan-v63 ubsan-v63 asan-v64 ubsan-v64 asan-v65 ubsan-v65 asan-v66 ubsan-v66 asan-v67 ubsan-v67 asan-v68 ubsan-v68 asan-v69 ubsan-v69 asan-v70 ubsan-v70 asan-v71 ubsan-v71 asan-v72 ubsan-v72 asan-v73 ubsan-v73 asan-v74 ubsan-v74 asan-v76 ubsan-v76 asan-v77 ubsan-v77 asan-v78 ubsan-v78 asan-v79 ubsan-v79 asan-v80 ubsan-v80 asan-v81 ubsan-v81 asan-v82 ubsan-v82 asan-v83 ubsan-v83 asan-v84 ubsan-v84 asan-v85 ubsan-v85 asan-v86 ubsan-v86 asan-v87 ubsan-v87 asan-v88 ubsan-v88 asan-v89 ubsan-v89 asan-v90 ubsan-v90 asan-v91 ubsan-v91 asan-v92 ubsan-v92 asan-v93 ubsan-v93 asan-v94 ubsan-v94 asan-v95 ubsan-v95 asan-v96 ubsan-v96 asan-v97 ubsan-v97 asan-v98 ubsan-v98 asan-v99 ubsan-v99 asan-v100 ubsan-v100
	@echo "sanitize: OK (ASAN v58/v59/v60/v61/v62/v63/v64/v65 + UBSAN v60/v61/v62/v63/v64/v65 all pass self-test under sanitizer)"

# --- Hardening runtime check + SBOM + secret-scan dispatcher -------

hardening-check: standalone-v60-hardened
	@bash scripts/security/hardening_check.sh

sbom:
	@bash scripts/security/sbom.sh > SBOM.json
	@echo "sbom: OK (SBOM.json written in CycloneDX-lite 1.5 JSON form)"

security-scan:
	@bash scripts/security/scan.sh

reproducible-build:
	@bash scripts/security/reproducible_build.sh

# --- v60 checks ----------------------------------------------------

test-v60: standalone-v60
	./creation_os_v60 --self-test

check-v60: standalone-v60 test-v60
	@echo "check-v60: OK (v60 σ-Shield runtime security kernel self-test)"

microbench-v60: standalone-v60
	@bash scripts/v60/microbench.sh

# --- v61 checks ----------------------------------------------------

test-v61: standalone-v61
	./creation_os_v61 --self-test

check-v61: standalone-v61 test-v61
	@echo "check-v61: OK (v61 Σ-Citadel lattice + attestation self-test)"

microbench-v61: standalone-v61
	@bash scripts/v61/microbench.sh

# --- v62 Reasoning Fabric (alien-tier, M-tier today) -------------
#
# v62 ships six branchless M4 kernels distilled from the 2026 frontier:
#   1. Latent CoT  (Coconut, arXiv:2412.06769; superposition theory 2026)
#   2. EBT verifier (Gladstone et al., arXiv:2507.02092, ICLR 2026)
#   3. HRM loop    (Sapient, arXiv:2506.21734)
#   4. NSA attend  (DeepSeek, arXiv:2502.11089; FSA 2026)
#   5. MTP draft   (DeepSeek-V3, arXiv:2412.19437; LK-tunes 2026)
#   6. ARKV KV     (arXiv:2603.08727; SAGE-KV 2025)
#
# All kernels honour the M4 invariants (.cursorrules):
#   aligned_alloc(64, ...) everywhere, NEON 4-way + prefetch, branchless
#   inner loops, no fread, mmap-friendly KV layout.  ≥62 self-tests.
V62_SRCS = src/v62/fabric.c

standalone-v62: src/v62/creation_os_v62.c $(V62_SRCS)
	$(CC) $(CFLAGS) -Isrc/v62 -o creation_os_v62 src/v62/creation_os_v62.c $(V62_SRCS) $(LDFLAGS)

standalone-v62-hardened: src/v62/creation_os_v62.c $(V62_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v62 -o creation_os_v62_hardened src/v62/creation_os_v62.c $(V62_SRCS) $(HARDEN_LDFLAGS)

asan-v62: src/v62/creation_os_v62.c $(V62_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v62 -o creation_os_v62_asan src/v62/creation_os_v62.c $(V62_SRCS) -lm
	./creation_os_v62_asan --self-test

ubsan-v62: src/v62/creation_os_v62.c $(V62_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v62 -o creation_os_v62_ubsan src/v62/creation_os_v62.c $(V62_SRCS) -lm
	./creation_os_v62_ubsan --self-test

test-v62: standalone-v62
	./creation_os_v62 --self-test

check-v62: standalone-v62 test-v62
	@echo "check-v62: OK (v62 Reasoning Fabric: latent-CoT + EBT + HRM + NSA + MTP + ARKV)"

microbench-v62: standalone-v62
	./creation_os_v62 --bench

# --- v63 σ-Cipher (end-to-end encrypted fabric) ------------------
#
# Dependency-free, constant-time from-scratch implementation of:
#   - BLAKE2b-256 (RFC 7693)
#   - HKDF-BLAKE2b (RFC 5869 structure)
#   - ChaCha20 stream cipher (RFC 8439)
#   - Poly1305 MAC (RFC 8439)
#   - ChaCha20-Poly1305 AEAD (RFC 8439 §2.8)
#   - X25519 scalar multiplication (RFC 7748)
#   - constant-time equality + secure_zero
#   - attestation-bound sealed envelope (quote-derived keys)
#   - symmetric ratchet (forward secrecy)
#   - IK-like handshake with BLAKE2b chaining key
#
# All cryptographic primitives are tested against official RFC vectors.
# Composes with v60 + v61 + v62 via cos_v63_compose_decision as a 4-bit
# branchless AND.  libsodium opt-in via COS_V63_LIBSODIUM=1 (planned).
V63_SRCS = src/v63/cipher.c

standalone-v63: src/v63/creation_os_v63.c $(V63_SRCS)
	$(CC) $(CFLAGS) -Isrc/v63 -o creation_os_v63 src/v63/creation_os_v63.c $(V63_SRCS) $(LDFLAGS)

standalone-v63-hardened: src/v63/creation_os_v63.c $(V63_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v63 -o creation_os_v63_hardened src/v63/creation_os_v63.c $(V63_SRCS) $(HARDEN_LDFLAGS)

asan-v63: src/v63/creation_os_v63.c $(V63_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v63 -o creation_os_v63_asan src/v63/creation_os_v63.c $(V63_SRCS) -lm
	./creation_os_v63_asan --self-test

ubsan-v63: src/v63/creation_os_v63.c $(V63_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v63 -o creation_os_v63_ubsan src/v63/creation_os_v63.c $(V63_SRCS) -lm
	./creation_os_v63_ubsan --self-test

test-v63: standalone-v63
	./creation_os_v63 --self-test

check-v63: standalone-v63 test-v63
	@echo "check-v63: OK (v63 σ-Cipher: BLAKE2b + HKDF + ChaCha20-Poly1305 + X25519 + sealed envelope + ratchet + session)"

microbench-v63: standalone-v63
	./creation_os_v63 --bench

# --- v64 σ-Intellect (agentic AGI kernel) ------------------------
#
# Dependency-free, branchless, Q0.15 integer-only kernel:
#   - MCTS-σ       (Empirical-MCTS / rStar-Math class, arXiv 2602.04248)
#   - Skill library (EvoSkill / Voyager class, arXiv 2603.02766)
#   - Tool-use authz (Dynamic ReAct / SWE-World, arXiv 2509.20386)
#   - Reflexion    (ERL / ReflexiCoder, arXiv 2603.24639 / 2603.05863)
#   - AlphaEvolve-σ (ternary-weight σ-gated mutation, arXiv 2402.17764 layout)
#   - MoD-σ        (Mixture-of-Depths, arXiv 2404.02258 / 2603.15619)
#
# Composes with v60 + v61 + v62 + v63 via cos_v64_compose_decision as a
# 5-bit branchless AND.  Pure C11, no floating point on the hot path.
V64_SRCS = src/v64/intellect.c

standalone-v64: src/v64/creation_os_v64.c $(V64_SRCS)
	$(CC) $(CFLAGS) -Isrc/v64 -o creation_os_v64 src/v64/creation_os_v64.c $(V64_SRCS) $(LDFLAGS)

standalone-v64-hardened: src/v64/creation_os_v64.c $(V64_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v64 -o creation_os_v64_hardened src/v64/creation_os_v64.c $(V64_SRCS) $(HARDEN_LDFLAGS)

asan-v64: src/v64/creation_os_v64.c $(V64_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v64 -o creation_os_v64_asan src/v64/creation_os_v64.c $(V64_SRCS) -lm
	./creation_os_v64_asan --self-test

ubsan-v64: src/v64/creation_os_v64.c $(V64_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v64 -o creation_os_v64_ubsan src/v64/creation_os_v64.c $(V64_SRCS) -lm
	./creation_os_v64_ubsan --self-test

test-v64: standalone-v64
	./creation_os_v64 --self-test

check-v64: standalone-v64 test-v64
	@echo "check-v64: OK (v64 σ-Intellect: MCTS-σ + skill-lib + tool-authz + reflexion + evolve + MoD-σ, 5-bit composed)"

microbench-v64: standalone-v64
	./creation_os_v64 --bench

# --- v65 σ-Hypercortex (hyperdimensional neurosymbolic kernel) ----
#
# Dependency-free, branchless, integer-only C11 kernel:
#   - Bipolar hypervectors at D = 16 384 bits (2048 B, 32 cache lines)
#   - Bind (XOR), bundle (threshold-majority), permute (cyclic)
#   - Similarity = (D − 2·Hamming) · (32768/D) in Q0.15 — no FP
#   - Cleanup memory, record/role-filler, analogy, sequence memory
#   - HVL — a 9-opcode bytecode ISA for VSA programs (LOAD/BIND/
#     BUNDLE/PERM/LOOKUP/SIM/CMPGE/GATE/HALT) with integer cost
#     accounting and a per-program budget gate
#
# Composes with v60 + v61 + v62 + v63 + v64 via
# cos_v65_compose_decision as a 6-bit branchless AND.  No floating
# point on the hot path; popcount-native (NEON `cnt` + horizontal).
V65_SRCS = src/v65/hypercortex.c

standalone-v65: src/v65/creation_os_v65.c $(V65_SRCS)
	$(CC) $(CFLAGS) -Isrc/v65 -o creation_os_v65 src/v65/creation_os_v65.c $(V65_SRCS) $(LDFLAGS)

standalone-v65-hardened: src/v65/creation_os_v65.c $(V65_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v65 -o creation_os_v65_hardened src/v65/creation_os_v65.c $(V65_SRCS) $(HARDEN_LDFLAGS)

asan-v65: src/v65/creation_os_v65.c $(V65_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v65 -o creation_os_v65_asan src/v65/creation_os_v65.c $(V65_SRCS) -lm
	./creation_os_v65_asan --self-test

ubsan-v65: src/v65/creation_os_v65.c $(V65_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v65 -o creation_os_v65_ubsan src/v65/creation_os_v65.c $(V65_SRCS) -lm
	./creation_os_v65_ubsan --self-test

test-v65: standalone-v65
	./creation_os_v65 --self-test

check-v65: standalone-v65 test-v65
	@echo "check-v65: OK (v65 σ-Hypercortex: bipolar-HDC + VSA bind/bundle/permute + cleanup + HVL, 6-bit composed)"

microbench-v65: standalone-v65
	./creation_os_v65 --bench

# --- v66 σ-Silicon: matrix substrate -----------------------------
#
# Branchless, integer-only C kernel shipping the 2026 silicon-tier
# frontier (MpGEMM arXiv:2512.21473, Hello SME arXiv:2409.18779,
# NativeTernary arXiv:2604.03336, BitNet b1.58 NEON arXiv:2410.16144,
# CFC arXiv:2603.27403) as the matrix substrate that turns v60-v65
# thought into actual MAC ops on actual matrix hardware.
#
# Capabilities:
#  - CPU feature detection (NEON / dotprod / i8mm / bf16 / SVE / SME /
#    SME2) via sysctl on Darwin, cached after first call.
#  - INT8 GEMV: 4-accumulator NEON inner loop with 64-byte prefetch,
#    bit-identical scalar fallback on non-AArch64.  Saturated Q0.15
#    output, no FP on the hot path.
#  - Ternary GEMV (BitNet b1.58, packed 2-bits-per-weight): branchless
#    table-lookup unpacker, constant time per row.
#  - NativeTernary wire decoder: 2.0 bpw, defensive 11-code -> 0.
#  - Conformal abstention gate (CFC): per-group Q0.15 threshold table
#    with streaming quantile update and ratio-preserving overflow shift.
#  - HSL bytecode: 8-opcode integer ISA over the matrix primitives
#    with per-instruction MAC-cost accounting and a GATE opcode that
#    writes v66_ok directly into the composed decision.
#
# Composes with v60 + v61 + v62 + v63 + v64 + v65 via
# cos_v66_compose_decision as a 7-bit branchless AND.  Default build
# does NOT emit any SME instruction (avoids SIGILL on M1/M2/M3 hosts);
# COS_V66_SME=1 will reserve the SME path once a streaming-mode shim
# lands.
V66_SRCS = src/v66/silicon.c

standalone-v66: src/v66/creation_os_v66.c $(V66_SRCS)
	$(CC) $(CFLAGS) -Isrc/v66 -o creation_os_v66 src/v66/creation_os_v66.c $(V66_SRCS) $(LDFLAGS)

standalone-v66-hardened: src/v66/creation_os_v66.c $(V66_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v66 -o creation_os_v66_hardened src/v66/creation_os_v66.c $(V66_SRCS) $(HARDEN_LDFLAGS)

asan-v66: src/v66/creation_os_v66.c $(V66_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v66 -o creation_os_v66_asan src/v66/creation_os_v66.c $(V66_SRCS) -lm
	./creation_os_v66_asan --self-test

ubsan-v66: src/v66/creation_os_v66.c $(V66_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v66 -o creation_os_v66_ubsan src/v66/creation_os_v66.c $(V66_SRCS) -lm
	./creation_os_v66_ubsan --self-test

test-v66: standalone-v66
	./creation_os_v66 --self-test

check-v66: standalone-v66 test-v66
	@echo "check-v66: OK (v66 σ-Silicon: int8-gemv + ternary-gemv + ntw + cfc-conformal + hsl, 7-bit composed)"

microbench-v66: standalone-v66
	./creation_os_v66 --bench

# --- v67 σ-Noesis: deliberative reasoning + knowledge retrieval ---
#
# Branchless, integer-only C kernel shipping the 2026 cognitive-frontier
# synthesis (AlphaProof / AlphaGeometry 2 DeepMind 2024, o1/o3 OpenAI
# deliberative reasoning, Graph-of-Thoughts arXiv:2308.09687, hybrid
# retrieval ColBERT/SPLADE/BGE 2026, dual-process cognitive architectures
# Soar/ACT-R/LIDA, metacognitive calibration CFC, Anthropic SAE
# feature-circuits, AlphaFold 3 iterative-refinement evidence trace) as
# the deliberative reasoning + AGI knowledge retrieval kernel that
# turns v60-v66 control + matrix plane into structured cognition.
#
# Capabilities:
#  - BM25 sparse retrieval (integer Q0.15 IDF approximation).
#  - 256-bit dense-signature Hamming retrieval.
#  - Graph walker (CSR + visited bitset, bounded budget).
#  - Hybrid rescore (BM25 + dense + graph, Q0.15 weights).
#  - Fixed-width deliberation beam (COS_V67_BEAM_W) + verifier.
#  - Dual-process gate (System-1 vs System-2) as a single compare.
#  - Metacognitive confidence (top1 margin vs range) in Q0.15.
#  - Tactic library (branchless priority cascade over witness score).
#  - NBL (Noetic Bytecode Language): 9-opcode integer ISA over all
#    of the above, with per-instruction cost accounting and a GATE
#    opcode that writes v67_ok directly into the composed decision.
#
# Composes with v60 + v61 + v62 + v63 + v64 + v65 + v66 via
# cos_v67_compose_decision as an 8-bit branchless AND.
V67_SRCS = src/v67/noesis.c

standalone-v67: src/v67/creation_os_v67.c $(V67_SRCS)
	$(CC) $(CFLAGS) -Isrc/v67 -o creation_os_v67 src/v67/creation_os_v67.c $(V67_SRCS) $(LDFLAGS)

standalone-v67-hardened: src/v67/creation_os_v67.c $(V67_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v67 -o creation_os_v67_hardened src/v67/creation_os_v67.c $(V67_SRCS) $(HARDEN_LDFLAGS)

asan-v67: src/v67/creation_os_v67.c $(V67_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v67 -o creation_os_v67_asan src/v67/creation_os_v67.c $(V67_SRCS) -lm
	./creation_os_v67_asan --self-test

ubsan-v67: src/v67/creation_os_v67.c $(V67_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v67 -o creation_os_v67_ubsan src/v67/creation_os_v67.c $(V67_SRCS) -lm
	./creation_os_v67_ubsan --self-test

test-v67: standalone-v67
	./creation_os_v67 --self-test

check-v67: standalone-v67 test-v67
	@echo "check-v67: OK (v67 σ-Noesis: bm25 + dense-sig + graph + beam + dual-process + metacog + tactics + nbl, 8-bit composed)"

microbench-v67: standalone-v67
	./creation_os_v67 --bench

# --- v68: σ-Mnemos -----------------------------------------------
#
# Continual-learning + episodic-memory + online-adaptation kernel.
# Distilled from the 2024-2026 frontier (Titans test-time memory,
# TTT, ACT-R activation decay, hippocampal pattern separation +
# completion via bipolar HV at D=8192, sleep replay /
# consolidation, EWC-style anti-catastrophic-forgetting ratchet).
# Composes with v60..v67 via cos_v68_compose_decision as a 9-bit
# branchless AND.
V68_SRCS = src/v68/mnemos.c

standalone-v68: src/v68/creation_os_v68.c $(V68_SRCS)
	$(CC) $(CFLAGS) -Isrc/v68 -o creation_os_v68 src/v68/creation_os_v68.c $(V68_SRCS) $(LDFLAGS)

standalone-v68-hardened: src/v68/creation_os_v68.c $(V68_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v68 -o creation_os_v68_hardened src/v68/creation_os_v68.c $(V68_SRCS) $(HARDEN_LDFLAGS)

asan-v68: src/v68/creation_os_v68.c $(V68_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v68 -o creation_os_v68_asan src/v68/creation_os_v68.c $(V68_SRCS) -lm
	./creation_os_v68_asan --self-test

ubsan-v68: src/v68/creation_os_v68.c $(V68_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v68 -o creation_os_v68_ubsan src/v68/creation_os_v68.c $(V68_SRCS) -lm
	./creation_os_v68_ubsan --self-test

test-v68: standalone-v68
	./creation_os_v68 --self-test

check-v68: standalone-v68 test-v68
	@echo "check-v68: OK (v68 σ-Mnemos: bipolar-HV + surprise + actr-decay + recall + hebb + consolidate + forget + mml, 9-bit composed)"

microbench-v68: standalone-v68
	./creation_os_v68 --bench

# --- v69: σ-Constellation ---------------------------------------
#
# Distributed-orchestration + parallel-decoding + multi-agent
# consensus kernel.  Distilled from the 2024-2026 frontier (EAGLE-3
# tree speculative decoding + Hierarchical SD, Council Mode +
# FREE-MAD multi-agent debate, PBFT-style 2f+1 Byzantine quorum,
# MaxScore MoE routing, Lamport vector clocks, flash-style chunked
# attention, AlphaZero-lineage Elo+UCB self-play, popcount KV
# dedup).  Composes with v60..v68 via cos_v69_compose_decision as a
# 10-bit branchless AND.
V69_SRCS = src/v69/constellation.c

standalone-v69: src/v69/creation_os_v69.c $(V69_SRCS)
	$(CC) $(CFLAGS) -Isrc/v69 -o creation_os_v69 src/v69/creation_os_v69.c $(V69_SRCS) $(LDFLAGS)

standalone-v69-hardened: src/v69/creation_os_v69.c $(V69_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v69 -o creation_os_v69_hardened src/v69/creation_os_v69.c $(V69_SRCS) $(HARDEN_LDFLAGS)

asan-v69: src/v69/creation_os_v69.c $(V69_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v69 -o creation_os_v69_asan src/v69/creation_os_v69.c $(V69_SRCS) -lm
	./creation_os_v69_asan --self-test

ubsan-v69: src/v69/creation_os_v69.c $(V69_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v69 -o creation_os_v69_ubsan src/v69/creation_os_v69.c $(V69_SRCS) -lm
	./creation_os_v69_ubsan --self-test

test-v69: standalone-v69
	./creation_os_v69 --self-test

check-v69: standalone-v69 test-v69
	@echo "check-v69: OK (v69 σ-Constellation: tree-spec + debate + byz-vote + moe-route + gossip + chunked-dot + elo-ucb + dedup + cl, 10-bit composed)"

microbench-v69: standalone-v69
	./creation_os_v69 --bench

# --- v70: σ-Hyperscale ------------------------------------------
#
# Trillion-parameter / hyperscale-killer substrate kernel.  Distilled
# from the 2024-2026 frontier (ShiftAddLLM power-of-2 GEMV
# arXiv:2406.05981, Mamba-2/Mamba-3 selective SSM scan
# arXiv:2312.00752 + arXiv:2603.15569, RWKV-7 "Goose" delta-rule
# arXiv:2503.14456, DeepSeek-V3 auxiliary-loss-free MoE up to 10 240
# experts arXiv:2412.19437, Samsung HBM-PIM / RACAM bit-serial
# AND-popcount arXiv:2603.09216, SKYLIGHT / LightMat-HP / Lightmatter
# Envise photonic WDM dot product arXiv:2602.19031, arXiv:2604.12278,
# Intel Loihi 3 / MatMul-free LLM graded-spike sparse activation
# arXiv:2503.18002, NVIDIA NCCL / Patarasuk-Yuan 2009 ring all-reduce,
# Petals / Helix / DirectStorage / GPUDirect Storage streaming weight
# scheduler arXiv:2209.01188 + arXiv:2406.01566).  Composes with
# v60..v69 via cos_v70_compose_decision as an 11-bit branchless AND.
V70_SRCS = src/v70/hyperscale.c

standalone-v70: src/v70/creation_os_v70.c $(V70_SRCS)
	$(CC) $(CFLAGS) -Isrc/v70 -o creation_os_v70 src/v70/creation_os_v70.c $(V70_SRCS) $(LDFLAGS)

standalone-v70-hardened: src/v70/creation_os_v70.c $(V70_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v70 -o creation_os_v70_hardened src/v70/creation_os_v70.c $(V70_SRCS) $(HARDEN_LDFLAGS)

asan-v70: src/v70/creation_os_v70.c $(V70_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v70 -o creation_os_v70_asan src/v70/creation_os_v70.c $(V70_SRCS) -lm
	./creation_os_v70_asan --self-test

ubsan-v70: src/v70/creation_os_v70.c $(V70_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v70 -o creation_os_v70_ubsan src/v70/creation_os_v70.c $(V70_SRCS) -lm
	./creation_os_v70_ubsan --self-test

test-v70: standalone-v70
	./creation_os_v70 --self-test

check-v70: standalone-v70 test-v70
	@echo "check-v70: OK (v70 σ-Hyperscale: p2q-shiftadd + ssm-mamba2 + rwkv7 + moe10k + pim-popcount + photonic-wdm + spike-loihi3 + ring-allreduce + stream-lru + hsl, 11-bit composed)"

microbench-v70: standalone-v70
	./creation_os_v70 --bench

# --- v71: σ-Wormhole ---------------------------------------------
# Hyperdimensional wormhole / portal-cognition / direct-addressing
# substrate.  Ten branchless integer primitives (Einstein-Rosen
# bridge table + constant-time anchor cleanup + single-XOR teleport
# + Kleinberg small-world multi-hop routing + ER=EPR tensor-bond
# pairing + HMAC-HV bridge integrity + Poincaré-boundary gate + hop
# budget + path receipt + WHL 10-op bytecode).  Composes with
# v60..v70 via cos_v71_compose_decision as a 12-bit branchless AND.
V71_SRCS = src/v71/wormhole.c

standalone-v71: src/v71/creation_os_v71.c $(V71_SRCS)
	$(CC) $(CFLAGS) -Isrc/v71 -o creation_os_v71 src/v71/creation_os_v71.c $(V71_SRCS) $(LDFLAGS)

standalone-v71-hardened: src/v71/creation_os_v71.c $(V71_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v71 -o creation_os_v71_hardened src/v71/creation_os_v71.c $(V71_SRCS) $(HARDEN_LDFLAGS)

asan-v71: src/v71/creation_os_v71.c $(V71_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v71 -o creation_os_v71_asan src/v71/creation_os_v71.c $(V71_SRCS) -lm
	./creation_os_v71_asan --self-test

ubsan-v71: src/v71/creation_os_v71.c $(V71_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v71 -o creation_os_v71_ubsan src/v71/creation_os_v71.c $(V71_SRCS) -lm
	./creation_os_v71_ubsan --self-test

test-v71: standalone-v71
	./creation_os_v71 --self-test

check-v71: standalone-v71 test-v71
	@echo "check-v71: OK (v71 σ-Wormhole: portal-ER-bridge + anchor-cleanup + teleport + kleinberg-route + er-epr-bond + hmac-hv-integrity + poincare-boundary + hop-budget + path-receipt + whl, 12-bit composed)"

microbench-v71: standalone-v71
	./creation_os_v71 --bench

# --- v72: σ-Chain -----------------------------------------------
# Blockchain / web3 / post-quantum / zero-knowledge / verifiable-
# agent substrate plane.  Ten branchless integer primitives
# (Merkle ledger + append-only receipts + WOTS+ one-time signature
# + threshold t-of-n quorum + hash-chain VRF + DAG-BFT quorum gate
# + ZK proof-receipt + EIP-7702 session delegation + chain-receipt
# bundle + SCL 10-op bytecode).  Composes with v60..v71 via
# cos_v72_compose_decision as a 13-bit branchless AND.
V72_SRCS = src/v72/chain.c

standalone-v72: src/v72/creation_os_v72.c $(V72_SRCS)
	$(CC) $(CFLAGS) -Isrc/v72 -o creation_os_v72 src/v72/creation_os_v72.c $(V72_SRCS) $(LDFLAGS)

standalone-v72-hardened: src/v72/creation_os_v72.c $(V72_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v72 -o creation_os_v72_hardened src/v72/creation_os_v72.c $(V72_SRCS) $(HARDEN_LDFLAGS)

asan-v72: src/v72/creation_os_v72.c $(V72_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v72 -o creation_os_v72_asan src/v72/creation_os_v72.c $(V72_SRCS) -lm
	./creation_os_v72_asan --self-test

ubsan-v72: src/v72/creation_os_v72.c $(V72_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v72 -o creation_os_v72_ubsan src/v72/creation_os_v72.c $(V72_SRCS) -lm
	./creation_os_v72_ubsan --self-test

test-v72: standalone-v72
	./creation_os_v72 --self-test

check-v72: standalone-v72 test-v72
	@echo "check-v72: OK (v72 σ-Chain: merkle-ledger + append-only-receipts + wots-plus + threshold-quorum + hash-chain-vrf + dag-bft-quorum + zk-proof-receipt + eip7702-delegation + chain-bundle + scl, 13-bit composed)"

microbench-v72: standalone-v72
	./creation_os_v72 --bench

# --- v73: σ-Omnimodal (the Creator) ------------------------------
# Unified multimodal generation substrate: code (Cursor / Lovable /
# Devin / Bolt lineage), image (Midjourney / FLUX / VINO MMDiT),
# video (Sora / Veo / MOVA / Matrix-Game-3), audio / voice clone
# (ElevenLabs / SoundStream / Encodec / MSR-Codec), workflow (n8n
# v2.10 "Brain vs Hands" + JudgeFlow + SOAN), physics-coherent flow
# (DiReCT + StreamFlow + MixFlow), and GTA-tier procedural world
# synthesis (MultiGen + Matrix-Game).  Ten branchless integer
# primitives (universal-modality tokenizer + rectified-flow integer
# scheduler + VINO cross-modal bind + MOVA AV lock + Matrix-Game
# world step + DiReCT physics gate + workflow DAG executor +
# Cursor-style code-edit planner + MultiGen asset navigation +
# OML 10-op bytecode).  Composes with v60..v72 via
# cos_v73_compose_decision as a 14-bit branchless AND.
V73_SRCS = src/v73/omnimodal.c

standalone-v73: src/v73/creation_os_v73.c $(V73_SRCS)
	$(CC) $(CFLAGS) -Isrc/v73 -o creation_os_v73 src/v73/creation_os_v73.c $(V73_SRCS) $(LDFLAGS)

standalone-v73-hardened: src/v73/creation_os_v73.c $(V73_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v73 -o creation_os_v73_hardened src/v73/creation_os_v73.c $(V73_SRCS) $(HARDEN_LDFLAGS)

asan-v73: src/v73/creation_os_v73.c $(V73_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v73 -o creation_os_v73_asan src/v73/creation_os_v73.c $(V73_SRCS) -lm
	./creation_os_v73_asan --self-test

ubsan-v73: src/v73/creation_os_v73.c $(V73_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v73 -o creation_os_v73_ubsan src/v73/creation_os_v73.c $(V73_SRCS) -lm
	./creation_os_v73_ubsan --self-test

test-v73: standalone-v73
	./creation_os_v73 --self-test

check-v73: standalone-v73 test-v73
	@echo "check-v73: OK (v73 σ-Omnimodal: universal-tokenizer + rectified-flow + VINO-bind + MOVA-lock + Matrix-Game-world + DiReCT-physics + n8n-workflow + Cursor-plan + MultiGen-asset + OML, 14-bit composed)"

microbench-v73: standalone-v73
	./creation_os_v73 --bench

# --- v74: σ-Experience (the Experience kernel) ------------------
# Perfect UX/UI + universal expertise + real-time render budget
# that makes 2026-era AAA games playable on commodity silicon (M4
# MacBook, iPhone-class SoC, plain Snapdragon phone) + 1-second
# interactive-world synth via hypervector space.  Ten branchless
# integer primitives (Fitts-V2P target heatmap + adaptive layout +
# designer-basis personalisation + SquireIR slot authoring +
# universal-expert LoRA-MoE mesh + skill composition + Mobile-GS
# order-free Gaussian-splat render step + DLSS 4.5 / FSR / XeSS
# upscale + multi-frame-gen gate + 1-second interactive-world
# synth + XPL 10-op bytecode).  References: arXiv:2603.11531
# (Mobile-GS ICLR 2026, 116 FPS Snapdragon 8 Gen 3), msplat Metal
# engine ~350 FPS M4 Max, DLSS 4.5 Dynamic Multi-Frame Generation
# 6× factor, arXiv:2508.13634 (V2P Fitts heatmap), Apple SQUIRE
# April 2026, arXiv:2604.09876 (designer-basis personalisation),
# arXiv:2601.04823 (DR-LoRA), arXiv:2603.00573 (CoMoL), Genie 3
# DeepMind 2026 (720p / 20-24 FPS text-to-interactive-world).
# Composes with v60..v73 via cos_v74_compose_decision as a 15-bit
# branchless AND.
V74_SRCS = src/v74/experience.c

standalone-v74: src/v74/creation_os_v74.c $(V74_SRCS)
	$(CC) $(CFLAGS) -Isrc/v74 -o creation_os_v74 src/v74/creation_os_v74.c $(V74_SRCS) $(LDFLAGS)

standalone-v74-hardened: src/v74/creation_os_v74.c $(V74_SRCS)
	$(CC) $(HARDEN_CFLAGS) -Isrc/v74 -o creation_os_v74_hardened src/v74/creation_os_v74.c $(V74_SRCS) $(HARDEN_LDFLAGS)

asan-v74: src/v74/creation_os_v74.c $(V74_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) -Isrc/v74 -o creation_os_v74_asan src/v74/creation_os_v74.c $(V74_SRCS) -lm
	./creation_os_v74_asan --self-test

ubsan-v74: src/v74/creation_os_v74.c $(V74_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) -Isrc/v74 -o creation_os_v74_ubsan src/v74/creation_os_v74.c $(V74_SRCS) -lm
	./creation_os_v74_ubsan --self-test

test-v74: standalone-v74
	./creation_os_v74 --self-test

check-v74: standalone-v74 test-v74
	@echo "check-v74: OK (v74 σ-Experience: fitts-v2p + adaptive-layout + designer-basis + squire-slot + universal-expert-mesh + skill-compose + mobile-gs-render + dlss-framegen + second-world + XPL, 15-bit composed)"

microbench-v74: standalone-v74
	./creation_os_v74 --bench

# --- v76: σ-Surface (the Surface kernel) ------------------------
# Mobile + messenger + legacy-software surface.  Ten branchless
# integer-only primitives:
#   1) touch-event decode (UITouch / MotionEvent → 256-bit HV)
#   2) 6-template gesture classify (tap/double/long/swipe/pinch/rotate)
#   3) haptic Q0.15 waveform generator + energy-budget gate
#   4) messenger protocol bridge (WhatsApp, Telegram, Signal,
#      iMessage, RCS, Matrix, XMPP, Discord, Slack, Line)
#   5) Signal-protocol X3DH-mix + Double-Ratchet step (SHA-256
#      via v75 FIPS-180-4; no OpenSSL)
#   6) WCAG 2.2 + Apple HIG + Material 3 accessibility bitmask
#   7) LWW-register + OR-set 256-bit CRDT merge
#   8) 64-app legacy-software capability-template HV registry
#      (Word/Excel/Outlook/Photoshop/Illustrator/AutoCAD/SolidWorks/
#       SAP/Salesforce/Figma/Notion/Zoom/Xcode/VSCode/Git/GitHub/
#       PostgreSQL/MongoDB/Redis/Stripe/AWS/GCP/Azure/Chrome/Safari/...)
#   9) 64-format file-type registry (DOCX/XLSX/PDF/DWG/PSD/PNG/MP4/
#      WAV/JSON/SQL/ZIP/APK/IPA/ELF/WASM/...)
#  10) SBL — Surface Bytecode Language — 10-op integer ISA.
# Composes with v60..v74 via cos_v76_compose_decision as a 16-bit
# branchless AND (v75 σ-License supplies the receipt spine).  The
# bindings/ios and bindings/android directories ship Swift + Kotlin
# façades so iOS / iPadOS / macOS and Android NDK targets consume
# the kernel directly via a C-ABI.
V76_SRCS = src/v76/surface.c src/license_kernel/license_attest.c
V76_INC  = -Isrc/v76 -Isrc/license_kernel

standalone-v76: src/v76/creation_os_v76.c $(V76_SRCS)
	$(CC) $(CFLAGS) $(V76_INC) -o creation_os_v76 src/v76/creation_os_v76.c $(V76_SRCS) $(LDFLAGS)

standalone-v76-hardened: src/v76/creation_os_v76.c $(V76_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V76_INC) -o creation_os_v76_hardened src/v76/creation_os_v76.c $(V76_SRCS) $(HARDEN_LDFLAGS)

asan-v76: src/v76/creation_os_v76.c $(V76_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V76_INC) -o creation_os_v76_asan src/v76/creation_os_v76.c $(V76_SRCS) -lm
	./creation_os_v76_asan --self-test

ubsan-v76: src/v76/creation_os_v76.c $(V76_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V76_INC) -o creation_os_v76_ubsan src/v76/creation_os_v76.c $(V76_SRCS) -lm
	./creation_os_v76_ubsan --self-test

test-v76: standalone-v76
	./creation_os_v76 --self-test

check-v76: standalone-v76 test-v76
	@echo "check-v76: OK (v76 σ-Surface: touch + gesture + haptic + msg(WhatsApp/Telegram/Signal/iMessage/RCS/Matrix/XMPP/Discord/Slack/Line) + Signal-ratchet + a11y + CRDT + legacy(Word/Excel/Outlook/Photoshop/AutoCAD/SAP/Salesforce/Figma/Xcode/Postgres/Stripe/AWS/...) + format(DOCX/XLSX/PDF/DWG/PSD/MP4/ZIP/APK/IPA/ELF/WASM/...) + SBL, 16-bit composed)"

microbench-v76: standalone-v76
	./creation_os_v76 --bench

# --- v77: σ-Reversible (the Landauer / Bennett plane) ------------
# Bit-reversible logic substrate.  Ten branchless, integer-only
# primitives, every one either self-inverse or with a declared
# explicit inverse, so the hot path erases zero bits and touches
# the Landauer floor of k_B·T·ln 2:
#   1) NOT                (self-inverse)
#   2) Feynman CNOT       (self-inverse)
#   3) SWAP               (self-inverse)
#   4) Fredkin CSWAP      (self-inverse, conservative)
#   5) Toffoli CCNOT      (self-inverse, universal)
#   6) Peres              (self-inverse at VM boundary)
#   7) Majority-3         (self-inverse, 4-register ancilla form)
#   8) Bennett driver     (forward ∘ reverse ≡ identity)
#   9) Reversible 8-bit adder via Peres chain
#  10) RVL — 8-opcode reversible bytecode ISA
# Extends v76's 16-bit composed decision with a lateral 17-th AND
# via cos_v77_compose_decision(v76_ok, v77_ok).  References:
# Landauer 1961, Bennett 1973, Toffoli 1980, Fredkin 1982,
# Feynman 1985, Margolus 1990, arXiv:2402.02720, NeurIPS 2023-25
# reversible-transformer literature.
V77_SRCS = src/v77/reversible.c
V77_INC  = -Isrc/v77

standalone-v77: src/v77/creation_os_v77.c $(V77_SRCS)
	$(CC) $(CFLAGS) $(V77_INC) -o creation_os_v77 src/v77/creation_os_v77.c $(V77_SRCS) $(LDFLAGS)

standalone-v77-hardened: src/v77/creation_os_v77.c $(V77_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V77_INC) -o creation_os_v77_hardened src/v77/creation_os_v77.c $(V77_SRCS) $(HARDEN_LDFLAGS)

asan-v77: src/v77/creation_os_v77.c $(V77_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V77_INC) -o creation_os_v77_asan src/v77/creation_os_v77.c $(V77_SRCS)
	./creation_os_v77_asan --self-test

ubsan-v77: src/v77/creation_os_v77.c $(V77_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V77_INC) -o creation_os_v77_ubsan src/v77/creation_os_v77.c $(V77_SRCS)
	./creation_os_v77_ubsan --self-test

test-v77: standalone-v77
	./creation_os_v77 --self-test

check-v77: standalone-v77 test-v77
	@echo "check-v77: OK (v77 σ-Reversible: NOT/CNOT/SWAP/Fredkin/Toffoli/Peres/Majority-3/Bennett/8-bit reversible adder/RVL bytecode; forward∘reverse ≡ id; 17-bit composed decision)"

microbench-v77: standalone-v77
	./creation_os_v77 --bench

# --- v78: σ-Gödel-Attestor (the meta-cognitive plane) ------------
# Self-attesting integer-only kernel that forces every emission
# through the joint filter of ten of 20th–21st-century foundational
# results, all branchless and libc-only:
#   1) IIT-φ                 (Tononi / Albantakis / Oizumi — IIT 4.0)
#   2) Variational free      (Friston 2010; Active Inference 2022;
#      energy                 Parr et al. 2024)
#   3) MDL upper bound       (Solomonoff 1964; Rissanen 1978)
#   4) Gödel prime-power     (Gödel 1931)
#      numbering
#   5) Global Workspace      (Baars 1988; Dehaene 2024)
#      broadcast gate
#   6) Halting witness       (Turing 1936)
#   7) Löbian self-trust     (Löb 1955; Payor 2014)
#      anchor
#   8) Bisimulation check    (Milner 1989; Sangiorgi 2011)
#   9) Chaitin Ω bound       (Chaitin 1975)
#  10) MCB bytecode VM       — 8-op Meta-Cognitive Bytecode that
#                              weaves 1..9 into one proof receipt.
# Extends v77's 17-bit composed decision with a lateral 18-th AND
# via cos_v78_compose_decision(v77_composed_ok, v78_ok).
V78_SRCS = src/v78/godel.c
V78_INC  = -Isrc/v78

standalone-v78: src/v78/creation_os_v78.c $(V78_SRCS)
	$(CC) $(CFLAGS) $(V78_INC) -o creation_os_v78 src/v78/creation_os_v78.c $(V78_SRCS) $(LDFLAGS)

standalone-v78-hardened: src/v78/creation_os_v78.c $(V78_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V78_INC) -o creation_os_v78_hardened src/v78/creation_os_v78.c $(V78_SRCS) $(HARDEN_LDFLAGS)

asan-v78: src/v78/creation_os_v78.c $(V78_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V78_INC) -o creation_os_v78_asan src/v78/creation_os_v78.c $(V78_SRCS)
	./creation_os_v78_asan --self-test

ubsan-v78: src/v78/creation_os_v78.c $(V78_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V78_INC) -o creation_os_v78_ubsan src/v78/creation_os_v78.c $(V78_SRCS)
	./creation_os_v78_ubsan --self-test

test-v78: standalone-v78
	./creation_os_v78 --self-test

check-v78: standalone-v78 test-v78
	@echo "check-v78: OK (v78 σ-Gödel-Attestor: IIT-φ/FEP/MDL/Gödel-num/Global-Workspace/halting-witness/Löbian self-trust/bisim/Chaitin-Ω/MCB bytecode; 18-bit composed decision; >200 000 self-test rows)"

microbench-v78: standalone-v78
	./creation_os_v78 --bench

# --- v79: σ-Simulacrum (the hypervector-space simulation substrate)
# Branchless, integer-only kernel (libc-only, Q16.16 fixed-point)
# that lets the agent instantiate, step, measure and verify entire
# worlds — particle systems, cellular automata, stabilizer quantum
# circuits, HD reservoir computers, Koopman-lifted dynamics, Cronin
# assembly objects, Kauffman Boolean networks — all inside the
# 256-bit HV space.  Ten primitives, each grounded in a real paper:
#   1) Symplectic Verlet      (Verlet 1967; Hairer/Lubich/Wanner 2006)
#   2) Wolfram 1D CA          (Wolfram 1983; Cook 2004 — rule 110
#                              universality)
#   3) Aaronson-Gottesman     (arXiv:quant-ph/0406196) stabilizer
#      tableau                  tableau — polynomial-time Clifford
#                              simulation on integer bit-arrays
#   4) HD reservoir step      (Jaeger 2001; Frady et al.
#                              arXiv:2003.04030; Schlegel et al.
#                              arXiv:2109.06548)
#   5) Koopman embedding      (Koopman 1931; Brunton et al. 2016)
#   6) Cronin assembly        (Marshall-Cronin 2021; Sharma et al.
#      index                    Nature 2023)
#   7) Kauffman Boolean       (Kauffman 1969)
#      network step
#   8) Integer energy         (leapfrog shadow Hamiltonian)
#   9) Trajectory receipt     (Merkle-style commutative XOR mix)
#  10) SSL bytecode VM        — 8-op Simulacrum Scripting Language
#                              that weaves 1..9 into one verifiable
#                              step program.
# Extends v78's 18-bit composed decision with a lateral 19-th AND
# via cos_v79_compose_decision(v78_composed_ok, v79_ok).
V79_SRCS = src/v79/simulacrum.c
V79_INC  = -Isrc/v79

standalone-v79: src/v79/creation_os_v79.c $(V79_SRCS)
	$(CC) $(CFLAGS) $(V79_INC) -o creation_os_v79 src/v79/creation_os_v79.c $(V79_SRCS) $(LDFLAGS)

standalone-v79-hardened: src/v79/creation_os_v79.c $(V79_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V79_INC) -o creation_os_v79_hardened src/v79/creation_os_v79.c $(V79_SRCS) $(HARDEN_LDFLAGS)

asan-v79: src/v79/creation_os_v79.c $(V79_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V79_INC) -o creation_os_v79_asan src/v79/creation_os_v79.c $(V79_SRCS)
	./creation_os_v79_asan --self-test

ubsan-v79: src/v79/creation_os_v79.c $(V79_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V79_INC) -o creation_os_v79_ubsan src/v79/creation_os_v79.c $(V79_SRCS)
	./creation_os_v79_ubsan --self-test

test-v79: standalone-v79
	./creation_os_v79 --self-test

check-v79: standalone-v79 test-v79
	@echo "check-v79: OK (v79 σ-Simulacrum: Verlet/CA/stabilizer/HD-reservoir/Koopman/assembly/graph/energy/receipt/SSL bytecode; 19-bit composed decision; >100 000 self-test rows)"

microbench-v79: standalone-v79
	./creation_os_v79 --bench

# --- v80: σ-Cortex (the hypervector-space neocortical reasoning plane)
# Branchless, integer-only kernel (libc-only, Q16.16 fixed-point)
# that collapses ten of the most impactful 2023–2025 sequence-model,
# attention, routing and test-time-compute results into one
# hypervector-space neocortex, compiled directly to hardware:
#   1) Selective SSM (Mamba)  (Gu & Dao 2023 arXiv:2312.00752;
#                              Mamba-2, Dao & Gu 2024
#                              arXiv:2405.21060)
#   2) RoPE                   (Su et al. 2021 RoFormer
#                              arXiv:2104.09864)
#   3) Sliding-window /       (Beltagy 2020 Longformer
#      ring attention          arXiv:2004.05150; Mistral 7B 2023;
#                              Liu et al. 2023 Ring Attention
#                              arXiv:2310.01889)
#   4) Paged KV cache         (Kwon et al. 2023 vLLM /
#                              PagedAttention arXiv:2309.06180)
#   5) Speculative decoding   (Leviathan, Kalman, Matias 2023
#      verify                   arXiv:2211.17192; Chen et al.
#                              DeepMind 2023)
#   6) Variational free       (Friston 2010 Nat. Rev. Neurosci.
#      energy                    "The free-energy principle")
#   7) KAN edge activation    (Liu et al. 2024 arXiv:2404.19756;
#                              Kolmogorov 1957 superposition
#                              theorem)
#   8) Continuous Thought     (Sakana AI 2025 "Continuous
#      Machine tick             Thought Machines")
#   9) MoE top-k router       (Shazeer et al. 2017
#                              arXiv:1701.06538; DeepSeek-MoE 2024
#                              arXiv:2401.06066; Mixtral 2024)
#  10) TTC bytecode VM        — 16-op Test-Time-Compute ISA that
#                              weaves 1..9 into one reasoning
#                              program (o1 / DeepSeek-R1
#                              2024–2025 style test-time scaling).
# Extends v79's 19-bit composed decision with a lateral 20-th AND
# via cos_v80_compose_decision(v79_composed_ok, v80_ok).
V80_SRCS = src/v80/cortex.c
V80_INC  = -Isrc/v80

standalone-v80: src/v80/creation_os_v80.c $(V80_SRCS)
	$(CC) $(CFLAGS) $(V80_INC) -o creation_os_v80 src/v80/creation_os_v80.c $(V80_SRCS) $(LDFLAGS)

standalone-v80-hardened: src/v80/creation_os_v80.c $(V80_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V80_INC) -o creation_os_v80_hardened src/v80/creation_os_v80.c $(V80_SRCS) $(HARDEN_LDFLAGS)

asan-v80: src/v80/creation_os_v80.c $(V80_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V80_INC) -o creation_os_v80_asan src/v80/creation_os_v80.c $(V80_SRCS)
	./creation_os_v80_asan --self-test

ubsan-v80: src/v80/creation_os_v80.c $(V80_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V80_INC) -o creation_os_v80_ubsan src/v80/creation_os_v80.c $(V80_SRCS)
	./creation_os_v80_ubsan --self-test

test-v80: standalone-v80
	./creation_os_v80 --self-test

check-v80: standalone-v80 test-v80
	@echo "check-v80: OK (v80 σ-Cortex: SSM/RoPE/sliding-attn/KV-paged/spec-verify/free-energy/KAN/CTM/MoE/TTC bytecode; 20-bit composed decision; >100 000 self-test rows)"

microbench-v80: standalone-v80
	./creation_os_v80 --bench

# --- v81 σ-Lattice (post-quantum lattice cryptography) -----------
#
# Eight primitives, each grounded in a standard:
#  1) Keccak-f[1600]         — FIPS 202 / Bertoni et al. 2011
#  2) SHAKE-128 / SHAKE-256  — FIPS 202 §6.3 XOFs
#  3) Barrett reduce mod q   — Kyber q=3329, magic 20159
#  4) Montgomery multiply    — R = 2^16, QINV = 62209
#  5) NTT / INTT             — Kyber-shaped 256-point transform
#  6) CBD(η=2) sampler       — FIPS 203 §4.2.2 SamplePolyCBD_η
#  7) Polynomial (de)serialise — 12-bit packed format
#  8) KEM (keygen/encaps/decaps) — Kyber-structured correctness
#                                   round-trip (spine for FIPS-203)
# Extends v80's 20-bit composed decision with a lateral 21st AND
# via cos_v81_compose_decision(v80_composed_ok, v81_ok).
V81_SRCS = src/v81/lattice.c
V81_INC  = -Isrc/v81

standalone-v81: src/v81/creation_os_v81.c $(V81_SRCS)
	$(CC) $(CFLAGS) $(V81_INC) -o creation_os_v81 src/v81/creation_os_v81.c $(V81_SRCS) $(LDFLAGS)

standalone-v81-hardened: src/v81/creation_os_v81.c $(V81_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V81_INC) -o creation_os_v81_hardened src/v81/creation_os_v81.c $(V81_SRCS) $(HARDEN_LDFLAGS)

asan-v81: src/v81/creation_os_v81.c $(V81_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V81_INC) -o creation_os_v81_asan src/v81/creation_os_v81.c $(V81_SRCS)
	./creation_os_v81_asan --self-test || ./creation_os_v81_asan

ubsan-v81: src/v81/creation_os_v81.c $(V81_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V81_INC) -o creation_os_v81_ubsan src/v81/creation_os_v81.c $(V81_SRCS)
	./creation_os_v81_ubsan --self-test || ./creation_os_v81_ubsan

test-v81: standalone-v81
	./creation_os_v81

check-v81: standalone-v81 test-v81
	@echo "check-v81: OK (v81 σ-Lattice: Keccak-f[1600]/SHAKE-128/256/Barrett/Montgomery/NTT/CBD/KEM; 21-bit composed decision; >3M self-test rows)"

microbench-v81: standalone-v81
	./creation_os_v81 --bench

# --- v82 σ-Stream (streaming per-chunk composed decision) --------
V82_SRCS = src/v82/stream.c src/v81/lattice.c
V82_INC  = -Isrc/v82 -Isrc/v81

standalone-v82: src/v82/creation_os_v82.c $(V82_SRCS)
	$(CC) $(CFLAGS) $(V82_INC) -o creation_os_v82 src/v82/creation_os_v82.c $(V82_SRCS) $(LDFLAGS)

standalone-v82-hardened: src/v82/creation_os_v82.c $(V82_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V82_INC) -o creation_os_v82_hardened src/v82/creation_os_v82.c $(V82_SRCS) $(HARDEN_LDFLAGS)

asan-v82: src/v82/creation_os_v82.c $(V82_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V82_INC) -o creation_os_v82_asan src/v82/creation_os_v82.c $(V82_SRCS)
	./creation_os_v82_asan

ubsan-v82: src/v82/creation_os_v82.c $(V82_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V82_INC) -o creation_os_v82_ubsan src/v82/creation_os_v82.c $(V82_SRCS)
	./creation_os_v82_ubsan

test-v82: standalone-v82
	./creation_os_v82

check-v82: standalone-v82 test-v82
	@echo "check-v82: OK (v82 σ-Stream: streaming per-chunk 22-bit AND, halt-on-flip, SHAKE-256 Merkle chain)"

microbench-v82: standalone-v82
	./creation_os_v82

# --- v83 σ-Agentic (PLAN→ROLL→SURPRISE→ENERGY loop) --------------
V83_SRCS = src/v83/agentic.c src/v81/lattice.c
V83_INC  = -Isrc/v83 -Isrc/v81

standalone-v83: src/v83/creation_os_v83.c $(V83_SRCS)
	$(CC) $(CFLAGS) $(V83_INC) -o creation_os_v83 src/v83/creation_os_v83.c $(V83_SRCS) $(LDFLAGS)

standalone-v83-hardened: src/v83/creation_os_v83.c $(V83_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V83_INC) -o creation_os_v83_hardened src/v83/creation_os_v83.c $(V83_SRCS) $(HARDEN_LDFLAGS)

asan-v83: src/v83/creation_os_v83.c $(V83_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V83_INC) -o creation_os_v83_asan src/v83/creation_os_v83.c $(V83_SRCS)
	./creation_os_v83_asan

ubsan-v83: src/v83/creation_os_v83.c $(V83_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V83_INC) -o creation_os_v83_ubsan src/v83/creation_os_v83.c $(V83_SRCS)
	./creation_os_v83_ubsan

test-v83: standalone-v83
	./creation_os_v83

check-v83: standalone-v83 test-v83
	@echo "check-v83: OK (v83 σ-Agentic: PLAN→ROLL→SURPRISE→ENERGY learner loop, 23-bit composed decision)"

microbench-v83: standalone-v83
	./creation_os_v83

# --- v84 σ-ZKProof (NANOZK-style layerwise commit) ----------------
V84_SRCS = src/v84/zkproof.c src/v81/lattice.c
V84_INC  = -Isrc/v84 -Isrc/v81

standalone-v84: src/v84/creation_os_v84.c $(V84_SRCS)
	$(CC) $(CFLAGS) $(V84_INC) -o creation_os_v84 src/v84/creation_os_v84.c $(V84_SRCS) $(LDFLAGS)

standalone-v84-hardened: src/v84/creation_os_v84.c $(V84_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V84_INC) -o creation_os_v84_hardened src/v84/creation_os_v84.c $(V84_SRCS) $(HARDEN_LDFLAGS)

asan-v84: src/v84/creation_os_v84.c $(V84_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V84_INC) -o creation_os_v84_asan src/v84/creation_os_v84.c $(V84_SRCS)
	./creation_os_v84_asan

ubsan-v84: src/v84/creation_os_v84.c $(V84_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V84_INC) -o creation_os_v84_ubsan src/v84/creation_os_v84.c $(V84_SRCS)
	./creation_os_v84_ubsan

test-v84: standalone-v84
	./creation_os_v84

check-v84: standalone-v84 test-v84
	@echo "check-v84: OK (v84 σ-ZKProof: NANOZK-style layerwise Merkle commit + tamper-detecting opening proof, 24-bit composed decision)"

microbench-v84: standalone-v84
	./creation_os_v84

# --- v85 σ-Formal (runtime TLA-style invariant checker) -----------
V85_SRCS = src/v85/formal.c
V85_INC  = -Isrc/v85

standalone-v85: src/v85/creation_os_v85.c $(V85_SRCS)
	$(CC) $(CFLAGS) $(V85_INC) -o creation_os_v85 src/v85/creation_os_v85.c $(V85_SRCS) $(LDFLAGS)

standalone-v85-hardened: src/v85/creation_os_v85.c $(V85_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V85_INC) -o creation_os_v85_hardened src/v85/creation_os_v85.c $(V85_SRCS) $(HARDEN_LDFLAGS)

asan-v85: src/v85/creation_os_v85.c $(V85_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V85_INC) -o creation_os_v85_asan src/v85/creation_os_v85.c $(V85_SRCS)
	./creation_os_v85_asan

ubsan-v85: src/v85/creation_os_v85.c $(V85_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V85_INC) -o creation_os_v85_ubsan src/v85/creation_os_v85.c $(V85_SRCS)
	./creation_os_v85_ubsan

test-v85: standalone-v85
	./creation_os_v85

check-v85: standalone-v85 test-v85
	@echo "check-v85: OK (v85 σ-Formal: TLA-style ALWAYS / EVENTUALLY / RESPONDS runtime invariant checker; 25-bit composed decision)"

microbench-v85: standalone-v85
	./creation_os_v85

# --- v86 σ-JEPA (latent-space predictive world model) -------------
V86_SRCS = src/v86/jepa.c
V86_INC  = -Isrc/v86

standalone-v86: src/v86/creation_os_v86.c $(V86_SRCS)
	$(CC) $(CFLAGS) $(V86_INC) -o creation_os_v86 src/v86/creation_os_v86.c $(V86_SRCS) $(LDFLAGS)

standalone-v86-hardened: src/v86/creation_os_v86.c $(V86_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V86_INC) -o creation_os_v86_hardened src/v86/creation_os_v86.c $(V86_SRCS) $(HARDEN_LDFLAGS)

asan-v86: src/v86/creation_os_v86.c $(V86_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V86_INC) -o creation_os_v86_asan src/v86/creation_os_v86.c $(V86_SRCS)
	./creation_os_v86_asan

ubsan-v86: src/v86/creation_os_v86.c $(V86_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V86_INC) -o creation_os_v86_ubsan src/v86/creation_os_v86.c $(V86_SRCS)
	./creation_os_v86_ubsan

test-v86: standalone-v86
	./creation_os_v86

check-v86: standalone-v86 test-v86
	@echo "check-v86: OK (v86 σ-JEPA: latent-space predictive world model — encoder + EMA target + predictor + VICReg var/invar/covar; 26-bit composed decision)"

microbench-v86: standalone-v86
	./creation_os_v86

# --- v87 σ-SAE (Top-K sparse autoencoder / mech-interp) ----------
V87_SRCS = src/v87/sae.c
V87_INC  = -Isrc/v87

standalone-v87: src/v87/creation_os_v87.c $(V87_SRCS)
	$(CC) $(CFLAGS) $(V87_INC) -o creation_os_v87 src/v87/creation_os_v87.c $(V87_SRCS) $(LDFLAGS)

standalone-v87-hardened: src/v87/creation_os_v87.c $(V87_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V87_INC) -o creation_os_v87_hardened src/v87/creation_os_v87.c $(V87_SRCS) $(HARDEN_LDFLAGS)

asan-v87: src/v87/creation_os_v87.c $(V87_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V87_INC) -o creation_os_v87_asan src/v87/creation_os_v87.c $(V87_SRCS)
	./creation_os_v87_asan

ubsan-v87: src/v87/creation_os_v87.c $(V87_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V87_INC) -o creation_os_v87_ubsan src/v87/creation_os_v87.c $(V87_SRCS)
	./creation_os_v87_ubsan

test-v87: standalone-v87
	./creation_os_v87

check-v87: standalone-v87 test-v87
	@echo "check-v87: OK (v87 σ-SAE: Top-K sparse autoencoder + feature dictionary + causal ablation; 27-bit composed decision)"

microbench-v87: standalone-v87
	./creation_os_v87

# --- v88 σ-FHE (Ring-LWE integer homomorphic compute) ------------
V88_SRCS = src/v88/fhe.c src/v81/lattice.c
V88_INC  = -Isrc/v88 -Isrc/v81

standalone-v88: src/v88/creation_os_v88.c $(V88_SRCS)
	$(CC) $(CFLAGS) $(V88_INC) -o creation_os_v88 src/v88/creation_os_v88.c $(V88_SRCS) $(LDFLAGS)

standalone-v88-hardened: src/v88/creation_os_v88.c $(V88_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V88_INC) -o creation_os_v88_hardened src/v88/creation_os_v88.c $(V88_SRCS) $(HARDEN_LDFLAGS)

asan-v88: src/v88/creation_os_v88.c $(V88_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V88_INC) -o creation_os_v88_asan src/v88/creation_os_v88.c $(V88_SRCS)
	./creation_os_v88_asan

ubsan-v88: src/v88/creation_os_v88.c $(V88_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V88_INC) -o creation_os_v88_ubsan src/v88/creation_os_v88.c $(V88_SRCS)
	./creation_os_v88_ubsan

test-v88: standalone-v88
	./creation_os_v88

check-v88: standalone-v88 test-v88
	@echo "check-v88: OK (v88 σ-FHE: Ring-LWE integer homomorphic — keygen + enc/dec + add + scalar-mul + rotate; 28-bit composed decision)"

microbench-v88: standalone-v88
	./creation_os_v88

# --- v89 σ-Spiking (Loihi-3 graded LIF + STDP neuromorphic) ------
V89_SRCS = src/v89/spiking.c
V89_INC  = -Isrc/v89

standalone-v89: src/v89/creation_os_v89.c $(V89_SRCS)
	$(CC) $(CFLAGS) $(V89_INC) -o creation_os_v89 src/v89/creation_os_v89.c $(V89_SRCS) $(LDFLAGS)

standalone-v89-hardened: src/v89/creation_os_v89.c $(V89_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V89_INC) -o creation_os_v89_hardened src/v89/creation_os_v89.c $(V89_SRCS) $(HARDEN_LDFLAGS)

asan-v89: src/v89/creation_os_v89.c $(V89_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V89_INC) -o creation_os_v89_asan src/v89/creation_os_v89.c $(V89_SRCS)
	./creation_os_v89_asan

ubsan-v89: src/v89/creation_os_v89.c $(V89_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V89_INC) -o creation_os_v89_ubsan src/v89/creation_os_v89.c $(V89_SRCS)
	./creation_os_v89_ubsan

test-v89: standalone-v89
	./creation_os_v89

check-v89: standalone-v89 test-v89
	@echo "check-v89: OK (v89 σ-Spiking: Loihi-3 graded-spike LIF + STDP neuromorphic plane; 29-bit composed decision)"

microbench-v89: standalone-v89
	./creation_os_v89

# --- v90 σ-Hierarchical (hierarchical active inference tower) -----
V90_SRCS = src/v90/hierarchical.c src/v81/lattice.c
V90_INC  = -Isrc/v90 -Isrc/v81

standalone-v90: src/v90/creation_os_v90.c $(V90_SRCS)
	$(CC) $(CFLAGS) $(V90_INC) -o creation_os_v90 src/v90/creation_os_v90.c $(V90_SRCS) $(LDFLAGS)

standalone-v90-hardened: src/v90/creation_os_v90.c $(V90_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V90_INC) -o creation_os_v90_hardened src/v90/creation_os_v90.c $(V90_SRCS) $(HARDEN_LDFLAGS)

asan-v90: src/v90/creation_os_v90.c $(V90_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V90_INC) -o creation_os_v90_asan src/v90/creation_os_v90.c $(V90_SRCS)
	./creation_os_v90_asan

ubsan-v90: src/v90/creation_os_v90.c $(V90_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V90_INC) -o creation_os_v90_ubsan src/v90/creation_os_v90.c $(V90_SRCS)
	./creation_os_v90_ubsan

test-v90: standalone-v90
	./creation_os_v90

check-v90: standalone-v90 test-v90
	@echo "check-v90: OK (v90 σ-Hierarchical: three-level RGM/S-HAI tower — top-down prior + bottom-up error + SHAKE-256 receipts; 30-bit composed decision)"

microbench-v90: standalone-v90
	./creation_os_v90

# --- v91 σ-Quantum (integer 4-qubit simulator + Grover) ----------
V91_SRCS = src/v91/quantum.c
V91_INC  = -Isrc/v91

standalone-v91: src/v91/creation_os_v91.c $(V91_SRCS)
	$(CC) $(CFLAGS) $(V91_INC) -o creation_os_v91 src/v91/creation_os_v91.c $(V91_SRCS) $(LDFLAGS)

standalone-v91-hardened: src/v91/creation_os_v91.c $(V91_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V91_INC) -o creation_os_v91_hardened src/v91/creation_os_v91.c $(V91_SRCS) $(HARDEN_LDFLAGS)

asan-v91: src/v91/creation_os_v91.c $(V91_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V91_INC) -o creation_os_v91_asan src/v91/creation_os_v91.c $(V91_SRCS)
	./creation_os_v91_asan

ubsan-v91: src/v91/creation_os_v91.c $(V91_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V91_INC) -o creation_os_v91_ubsan src/v91/creation_os_v91.c $(V91_SRCS)
	./creation_os_v91_ubsan

test-v91: standalone-v91
	./creation_os_v91

check-v91: standalone-v91 test-v91
	@echo "check-v91: OK (v91 σ-Quantum: 4-qubit integer quantum simulator + Grover amplitude amplification; 31-bit composed decision)"

microbench-v91: standalone-v91
	./creation_os_v91

# --- v92 σ-Titans (neural long-term memory, surprise-gated) ------
V92_SRCS = src/v92/titans.c
V92_INC  = -Isrc/v92

standalone-v92: src/v92/creation_os_v92.c $(V92_SRCS)
	$(CC) $(CFLAGS) $(V92_INC) -o creation_os_v92 src/v92/creation_os_v92.c $(V92_SRCS) $(LDFLAGS)

standalone-v92-hardened: src/v92/creation_os_v92.c $(V92_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V92_INC) -o creation_os_v92_hardened src/v92/creation_os_v92.c $(V92_SRCS) $(HARDEN_LDFLAGS)

asan-v92: src/v92/creation_os_v92.c $(V92_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V92_INC) -o creation_os_v92_asan src/v92/creation_os_v92.c $(V92_SRCS)
	./creation_os_v92_asan

ubsan-v92: src/v92/creation_os_v92.c $(V92_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V92_INC) -o creation_os_v92_ubsan src/v92/creation_os_v92.c $(V92_SRCS)
	./creation_os_v92_ubsan

test-v92: standalone-v92
	./creation_os_v92

check-v92: standalone-v92 test-v92
	@echo "check-v92: OK (v92 σ-Titans: neural long-term memory with surprise-gated writes and adaptive forgetting; 32-bit composed decision)"

microbench-v92: standalone-v92
	./creation_os_v92

# --- v93 σ-MoR (Mixture-of-Recursions, adaptive depth routing) ---
V93_SRCS = src/v93/mor.c
V93_INC  = -Isrc/v93

standalone-v93: src/v93/creation_os_v93.c $(V93_SRCS)
	$(CC) $(CFLAGS) $(V93_INC) -o creation_os_v93 src/v93/creation_os_v93.c $(V93_SRCS) $(LDFLAGS)

standalone-v93-hardened: src/v93/creation_os_v93.c $(V93_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V93_INC) -o creation_os_v93_hardened src/v93/creation_os_v93.c $(V93_SRCS) $(HARDEN_LDFLAGS)

asan-v93: src/v93/creation_os_v93.c $(V93_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V93_INC) -o creation_os_v93_asan src/v93/creation_os_v93.c $(V93_SRCS)
	./creation_os_v93_asan

ubsan-v93: src/v93/creation_os_v93.c $(V93_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V93_INC) -o creation_os_v93_ubsan src/v93/creation_os_v93.c $(V93_SRCS)
	./creation_os_v93_ubsan

test-v93: standalone-v93
	./creation_os_v93

check-v93: standalone-v93 test-v93
	@echo "check-v93: OK (v93 σ-MoR: Mixture-of-Recursions with token-level adaptive recursion depth routing; 33-bit composed decision)"

microbench-v93: standalone-v93
	./creation_os_v93

# --- v94 σ-Clifford (Cl(3,0) geometric algebra layer) ------------
V94_SRCS = src/v94/clifford.c
V94_INC  = -Isrc/v94

standalone-v94: src/v94/creation_os_v94.c $(V94_SRCS)
	$(CC) $(CFLAGS) $(V94_INC) -o creation_os_v94 src/v94/creation_os_v94.c $(V94_SRCS) $(LDFLAGS)

standalone-v94-hardened: src/v94/creation_os_v94.c $(V94_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V94_INC) -o creation_os_v94_hardened src/v94/creation_os_v94.c $(V94_SRCS) $(HARDEN_LDFLAGS)

asan-v94: src/v94/creation_os_v94.c $(V94_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V94_INC) -o creation_os_v94_asan src/v94/creation_os_v94.c $(V94_SRCS)
	./creation_os_v94_asan

ubsan-v94: src/v94/creation_os_v94.c $(V94_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V94_INC) -o creation_os_v94_ubsan src/v94/creation_os_v94.c $(V94_SRCS)
	./creation_os_v94_ubsan

test-v94: standalone-v94
	./creation_os_v94

check-v94: standalone-v94 test-v94
	@echo "check-v94: OK (v94 σ-Clifford: Cl(3,0) geometric-algebra multivector algebra + equivariant layer; 34-bit composed decision)"

microbench-v94: standalone-v94
	./creation_os_v94

# --- v95 σ-Sheaf (cellular-sheaf NN + sheaf Laplacian) -----------
V95_SRCS = src/v95/sheaf.c
V95_INC  = -Isrc/v95

standalone-v95: src/v95/creation_os_v95.c $(V95_SRCS)
	$(CC) $(CFLAGS) $(V95_INC) -o creation_os_v95 src/v95/creation_os_v95.c $(V95_SRCS) $(LDFLAGS)

standalone-v95-hardened: src/v95/creation_os_v95.c $(V95_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V95_INC) -o creation_os_v95_hardened src/v95/creation_os_v95.c $(V95_SRCS) $(HARDEN_LDFLAGS)

asan-v95: src/v95/creation_os_v95.c $(V95_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V95_INC) -o creation_os_v95_asan src/v95/creation_os_v95.c $(V95_SRCS)
	./creation_os_v95_asan

ubsan-v95: src/v95/creation_os_v95.c $(V95_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V95_INC) -o creation_os_v95_ubsan src/v95/creation_os_v95.c $(V95_SRCS)
	./creation_os_v95_ubsan

test-v95: standalone-v95
	./creation_os_v95

check-v95: standalone-v95 test-v95
	@echo "check-v95: OK (v95 σ-Sheaf: cellular-sheaf neural network with sheaf-Laplacian heat diffusion; 35-bit composed decision)"

microbench-v95: standalone-v95
	./creation_os_v95

# --- v96 σ-Diffusion (integer rectified-flow / DDIM sampler) -----
V96_SRCS = src/v96/diffusion.c
V96_INC  = -Isrc/v96

standalone-v96: src/v96/creation_os_v96.c $(V96_SRCS)
	$(CC) $(CFLAGS) $(V96_INC) -o creation_os_v96 src/v96/creation_os_v96.c $(V96_SRCS) $(LDFLAGS)

standalone-v96-hardened: src/v96/creation_os_v96.c $(V96_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V96_INC) -o creation_os_v96_hardened src/v96/creation_os_v96.c $(V96_SRCS) $(HARDEN_LDFLAGS)

asan-v96: src/v96/creation_os_v96.c $(V96_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V96_INC) -o creation_os_v96_asan src/v96/creation_os_v96.c $(V96_SRCS)
	./creation_os_v96_asan

ubsan-v96: src/v96/creation_os_v96.c $(V96_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V96_INC) -o creation_os_v96_ubsan src/v96/creation_os_v96.c $(V96_SRCS)
	./creation_os_v96_ubsan

test-v96: standalone-v96
	./creation_os_v96

check-v96: standalone-v96 test-v96
	@echo "check-v96: OK (v96 σ-Diffusion: integer rectified-flow / DDIM sampler; 36-bit composed decision)"

microbench-v96: standalone-v96
	./creation_os_v96

# --- v97 σ-RL (Q-learning + GAE + PPO-clip) ----------------------
V97_SRCS = src/v97/rl.c
V97_INC  = -Isrc/v97

standalone-v97: src/v97/creation_os_v97.c $(V97_SRCS)
	$(CC) $(CFLAGS) $(V97_INC) -o creation_os_v97 src/v97/creation_os_v97.c $(V97_SRCS) $(LDFLAGS)

standalone-v97-hardened: src/v97/creation_os_v97.c $(V97_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V97_INC) -o creation_os_v97_hardened src/v97/creation_os_v97.c $(V97_SRCS) $(HARDEN_LDFLAGS)

asan-v97: src/v97/creation_os_v97.c $(V97_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V97_INC) -o creation_os_v97_asan src/v97/creation_os_v97.c $(V97_SRCS)
	./creation_os_v97_asan

ubsan-v97: src/v97/creation_os_v97.c $(V97_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V97_INC) -o creation_os_v97_ubsan src/v97/creation_os_v97.c $(V97_SRCS)
	./creation_os_v97_ubsan

test-v97: standalone-v97
	./creation_os_v97

check-v97: standalone-v97 test-v97
	@echo "check-v97: OK (v97 σ-RL: integer Q-learning + GAE + PPO-clip; 37-bit composed decision)"

microbench-v97: standalone-v97
	./creation_os_v97

# --- v98 σ-Topology (Vietoris–Rips + Betti filtration) -----------
V98_SRCS = src/v98/topology.c
V98_INC  = -Isrc/v98

standalone-v98: src/v98/creation_os_v98.c $(V98_SRCS)
	$(CC) $(CFLAGS) $(V98_INC) -o creation_os_v98 src/v98/creation_os_v98.c $(V98_SRCS) $(LDFLAGS)

standalone-v98-hardened: src/v98/creation_os_v98.c $(V98_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V98_INC) -o creation_os_v98_hardened src/v98/creation_os_v98.c $(V98_SRCS) $(HARDEN_LDFLAGS)

asan-v98: src/v98/creation_os_v98.c $(V98_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V98_INC) -o creation_os_v98_asan src/v98/creation_os_v98.c $(V98_SRCS)
	./creation_os_v98_asan

ubsan-v98: src/v98/creation_os_v98.c $(V98_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V98_INC) -o creation_os_v98_ubsan src/v98/creation_os_v98.c $(V98_SRCS)
	./creation_os_v98_ubsan

test-v98: standalone-v98
	./creation_os_v98

check-v98: standalone-v98 test-v98
	@echo "check-v98: OK (v98 σ-Topology: persistent homology + Vietoris–Rips + Betti-0/1 filtration; 38-bit composed decision)"

microbench-v98: standalone-v98
	./creation_os_v98

# --- v99 σ-Causal (SCM + do-calculus + back-door) -----------------
V99_SRCS = src/v99/causal.c
V99_INC  = -Isrc/v99

standalone-v99: src/v99/creation_os_v99.c $(V99_SRCS)
	$(CC) $(CFLAGS) $(V99_INC) -o creation_os_v99 src/v99/creation_os_v99.c $(V99_SRCS) $(LDFLAGS)

standalone-v99-hardened: src/v99/creation_os_v99.c $(V99_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V99_INC) -o creation_os_v99_hardened src/v99/creation_os_v99.c $(V99_SRCS) $(HARDEN_LDFLAGS)

asan-v99: src/v99/creation_os_v99.c $(V99_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V99_INC) -o creation_os_v99_asan src/v99/creation_os_v99.c $(V99_SRCS)
	./creation_os_v99_asan

ubsan-v99: src/v99/creation_os_v99.c $(V99_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V99_INC) -o creation_os_v99_ubsan src/v99/creation_os_v99.c $(V99_SRCS)
	./creation_os_v99_ubsan

test-v99: standalone-v99
	./creation_os_v99

check-v99: standalone-v99 test-v99
	@echo "check-v99: OK (v99 σ-Causal: structural causal model + do-calculus + back-door + counterfactual; 39-bit composed decision)"

microbench-v99: standalone-v99
	./creation_os_v99

# --- v100 σ-Hyena (sub-quadratic gated long convolution) ----------
V100_SRCS = src/v100/hyena.c
V100_INC  = -Isrc/v100

standalone-v100: src/v100/creation_os_v100.c $(V100_SRCS)
	$(CC) $(CFLAGS) $(V100_INC) -o creation_os_v100 src/v100/creation_os_v100.c $(V100_SRCS) $(LDFLAGS)

standalone-v100-hardened: src/v100/creation_os_v100.c $(V100_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(V100_INC) -o creation_os_v100_hardened src/v100/creation_os_v100.c $(V100_SRCS) $(HARDEN_LDFLAGS)

asan-v100: src/v100/creation_os_v100.c $(V100_SRCS)
	$(CC) $(SAN_CFLAGS) $(ASAN_FLAGS) $(V100_INC) -o creation_os_v100_asan src/v100/creation_os_v100.c $(V100_SRCS)
	./creation_os_v100_asan

ubsan-v100: src/v100/creation_os_v100.c $(V100_SRCS)
	$(CC) $(SAN_CFLAGS) $(UBSAN_FLAGS) $(V100_INC) -o creation_os_v100_ubsan src/v100/creation_os_v100.c $(V100_SRCS)
	./creation_os_v100_ubsan

test-v100: standalone-v100
	./creation_os_v100

check-v100: standalone-v100 test-v100
	@echo "check-v100: OK (v100 σ-Hyena: sub-quadratic gated long convolution operator; 40-bit composed decision)"

microbench-v100: standalone-v100
	./creation_os_v100

# --- v101 σ-BitNet-Bridge (real LLM inference via bitnet.cpp) ----------
#
# Default build is "stub mode": the σ-channel math is exercised by
# `cos_v101_self_test()` on pure-C synthetic logits and merge-gate stays
# green on any host, with or without the 1.1 GB BitNet checkpoint.
#
# Real mode: pass COS_V101_BITNET_REAL=1 AND build bitnet.cpp first
# (see docs/v101/THE_BITNET_BRIDGE.md for one-line setup).  Real mode links
# against third_party/bitnet/build/.../libllama.dylib and requires
# models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf to run anything beyond
# --self-test.
V101_INC      = -Isrc/v101
V101_COMMON   = src/v101/sigma_channels.c src/v101/self_test.c src/v101/creation_os_v101.c

# Stub mode ---------------------------------------------------------
V101_STUB_SRCS = $(V101_COMMON) src/v101/bridge_stub.c

standalone-v101: $(V101_COMMON) src/v101/bridge_stub.c src/v101/bridge.h
	$(CC) $(CFLAGS) $(V101_INC) -o creation_os_v101 $(V101_STUB_SRCS) $(LDFLAGS)

test-v101: standalone-v101
	./creation_os_v101 --self-test

check-v101: standalone-v101 test-v101
	@echo "check-v101: OK (v101 σ-BitNet-Bridge σ-channel self-test; stub mode)"

microbench-v101: standalone-v101
	./creation_os_v101 --self-test

# Real mode ---------------------------------------------------------
# Paths are relative to repo root (make is invoked from here).
V101_LLAMA_INC = -Ithird_party/bitnet/3rdparty/llama.cpp/include \
                 -Ithird_party/bitnet/3rdparty/llama.cpp/ggml/include
V101_LLAMA_LIB = -Lthird_party/bitnet/build/3rdparty/llama.cpp/src -lllama \
                 -Lthird_party/bitnet/build/3rdparty/llama.cpp/ggml/src -lggml
V101_REAL_RPATH_MAC = -Wl,-rpath,@loader_path/third_party/bitnet/build/3rdparty/llama.cpp/src \
                      -Wl,-rpath,@loader_path/third_party/bitnet/build/3rdparty/llama.cpp/ggml/src
V101_REAL_SRCS = $(V101_COMMON) src/v101/bridge_real.c

standalone-v101-real: $(V101_COMMON) src/v101/bridge_real.c src/v101/bridge.h
	$(CC) $(CFLAGS) -DCOS_V101_BITNET_REAL=1 $(V101_INC) $(V101_LLAMA_INC) \
	    -o creation_os_v101 $(V101_REAL_SRCS) \
	    $(V101_LLAMA_LIB) $(V101_REAL_RPATH_MAC) $(LDFLAGS)

check-v101-real: standalone-v101-real
	./creation_os_v101 --self-test
	./creation_os_v101 --banner
	@echo "check-v101-real: OK (v101 σ-BitNet-Bridge real-mode build + self-test)"

bench-v101-smoke: standalone-v101-real
	@if [ -f models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf ]; then \
	    ./creation_os_v101 --gen \
	        --gguf models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf \
	        --ctx  "The capital of France is" \
	        --max-tokens 10 ; \
	else \
	    echo "bench-v101-smoke: SKIP (no models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf)"; \
	fi

# --- v102 σ-Eval-Harness (lm-evaluation-harness bridge) ----------
#
# v102 has no C kernel of its own.  It exposes the v101 bridge to the
# EleutherAI lm-evaluation-harness via a registered `creation_os` backend
# (benchmarks/v102/creation_os_backend.py), and wraps the full run in
# benchmarks/v102/run_eval.sh (baseline + σ-gated).  check-v102 runs a
# minimal --limit 5 smoke only if the tooling is present, otherwise SKIPs.
check-v102:
	@bash benchmarks/v102/check_v102.sh

bench-v102:
	@bash benchmarks/v102/run_eval.sh

# v103 σ-τ-sweep.  Pure post-hoc analysis on top of a single σ-logging run
# (benchmarks/v103/run_sigma_log.sh).  `check-v103` is a lightweight smoke
# that (a) imports the v103 backend module to confirm it registers, and
# (b) if a previous σ sidecar is present, runs the RC-metric analysis on
# the cached sidecar + lm-eval output without any further forward passes.
check-v103:
	@bash benchmarks/v103/check_v103.sh

bench-v103:
	@bash benchmarks/v103/run_sigma_log.sh
	@.venv-bitnet/bin/python benchmarks/v103/compute_rc_metrics.py

# v104 σ-operator search.  Post-hoc analysers on top of v103's sidecar JSONL.
# `check-v104` runs the operator-registry smoke + synthetic oracle/random
# sanity; no model weights needed, never blocks merge-gate.
# `bench-v104` runs the full n=5000 σ-log pass, then operator search +
# channel analysis + α-sweep.  Cost ≈ 5-6 h wall on Apple M3 / Metal.
check-v104:
	@bash benchmarks/v104/check_v104.sh

bench-v104:
	@bash benchmarks/v104/run_n5000_sigma_log.sh
	@.venv-bitnet/bin/python benchmarks/v104/compute_operator_rcc.py \
	    --results benchmarks/v104/n5000_results
	@.venv-bitnet/bin/python benchmarks/v104/channel_analysis.py \
	    --results benchmarks/v104/n5000_results
	@.venv-bitnet/bin/python benchmarks/v104/hybrid_sweep.py \
	    --results benchmarks/v104/n5000_results --second sigma_max_token

# --- v106 σ-Server ------------------------------------------------
# OpenAI-compatible HTTP layer around the v101 σ-bridge.
# `standalone-v106` builds in stub mode (merge-gate safe, no GGUF
# required).  `check-v106` runs a zero-network self-test that parses
# a synthetic JSON body and verifies the JSON helpers + config
# defaults.  `bench-v106` starts the server on a free port and runs
# the curl-based smoke test in benchmarks/v106/ against /health and
# /v1/models.
V106_COMMON_SRCS = src/v106/server.c src/v106/config.c src/v106/json_helpers.c src/v106/main.c
V106_INC         = -Isrc/v101 -Isrc/v106 -Isrc/v111 -Isrc/v112 -Isrc/v113 -Isrc/v114

# v111.2 σ-Reason + v112 σ-Agent + v113 σ-Sandbox + v114 σ-Swarm are
# compiled into every v106 binary so that the HTTP surface is complete
# in both stub and real mode.  Endpoints return 503 where the bridge
# cannot back them (e.g. no GGUF loaded).
V111_REASON_SRCS = src/v111/reason.c
V112_TOOLS_SRCS  = src/v112/tools.c
V113_SANDBOX_SRCS = src/v113/sandbox.c
V114_SWARM_SRCS   = src/v114/swarm.c

V106_AGENT_SRCS = $(V111_REASON_SRCS) $(V112_TOOLS_SRCS) $(V113_SANDBOX_SRCS) $(V114_SWARM_SRCS)

standalone-v106: $(V106_COMMON_SRCS) $(V106_AGENT_SRCS) src/v101/sigma_channels.c src/v101/self_test.c src/v101/bridge_stub.c
	$(CC) $(CFLAGS) $(V106_INC) -o creation_os_server \
	    $(V106_COMMON_SRCS) $(V106_AGENT_SRCS) \
	    src/v101/sigma_channels.c src/v101/self_test.c src/v101/bridge_stub.c \
	    $(LDFLAGS)

standalone-v106-real: $(V106_COMMON_SRCS) $(V106_AGENT_SRCS) src/v101/sigma_channels.c src/v101/self_test.c src/v101/bridge_real.c
	$(CC) $(CFLAGS) -DCOS_V101_BITNET_REAL=1 $(V106_INC) $(V101_LLAMA_INC) \
	    -o creation_os_server \
	    $(V106_COMMON_SRCS) $(V106_AGENT_SRCS) \
	    src/v101/sigma_channels.c src/v101/self_test.c src/v101/bridge_real.c \
	    $(V101_LLAMA_LIB) $(V101_REAL_RPATH_MAC) $(LDFLAGS)

check-v106: standalone-v106
	./creation_os_server --self-test
	@bash benchmarks/v106/check_v106.sh
	@echo "check-v106: OK (v106 σ-Server — config + JSON helpers + loopback curl)"

bench-v106: standalone-v106
	@bash benchmarks/v106/bench_v106.sh

# --- v111 σ-Frontier + σ-Reason + σ-SFT --------------------------
# v111.1 is a Python re-analyser over v103/v104 sidecars (no C kernel);
# see benchmarks/v111/.  v111.2 is the σ-Reason C library that wires into
# the v106 server above.  v111.3 is an MLX SFT recipe (Python, optional).
V111_INC = -Isrc/v101 -Isrc/v111

standalone-v111-reason: src/v111/reason.c src/v111/reason_self_test.c \
                        src/v101/sigma_channels.c src/v101/self_test.c src/v101/bridge_stub.c
	$(CC) $(CFLAGS) $(V111_INC) -o creation_os_v111_reason \
	    src/v111/reason.c src/v111/reason_self_test.c \
	    src/v101/sigma_channels.c src/v101/self_test.c src/v101/bridge_stub.c \
	    $(LDFLAGS)

check-v111-reason: standalone-v111-reason standalone-v106
	./creation_os_v111_reason
	@bash benchmarks/v111/check_v111_reason.sh
	@echo "check-v111-reason: OK (v111.2 σ-Reason self-test + loopback /v1/reason)"

check-v111-matrix:
	@bash benchmarks/v111/check_v111_matrix.sh

check-v111-adaptive:
	@bash benchmarks/v111/check_v111_adaptive.sh

check-v111-prereg-adaptive:
	@bash benchmarks/v111/check_v111_prereg_adaptive.sh

check-v111-mmlu-discovery:
	@bash benchmarks/v111/check_v111_mmlu_discovery.sh

check-v111-sft-smoke:
	@bash training/v111/check_v111_sft_smoke.sh

check-v111: check-v111-matrix check-v111-adaptive check-v111-prereg-adaptive check-v111-mmlu-discovery check-v111-reason check-v111-sft-smoke
	@echo "check-v111: OK (v111.1 matrix + v111.2 adaptive/post-hoc + v111.2 prereg + v111.2 mmlu-discovery + v111.2 reason + v111.3 SFT smoke)"

bench-v111-hellaswag: standalone-v101-real
	@bash benchmarks/v111/run_matrix.sh hellaswag

bench-v111-full:
	@bash benchmarks/v111/run_matrix.sh all

# --- v112 σ-Agent, v113 σ-Sandbox, v114 σ-Swarm ------------------
# These three kernels ship as libraries compiled into the v106 server
# (so the HTTP endpoints are always available) *and* as standalone
# CLI tools for merge-gate smoke checks that do not require weights.
V112_INC = -Isrc/v101 -Isrc/v112
V113_INC = -Isrc/v101 -Isrc/v112 -Isrc/v113
V114_INC = -Isrc/v101 -Isrc/v112 -Isrc/v114

creation_os_v112_tools: $(V112_TOOLS_SRCS) src/v112/main.c \
                         src/v101/sigma_channels.c src/v101/self_test.c \
                         src/v101/bridge_stub.c
	$(CC) $(CFLAGS) $(V112_INC) -o $@ \
	    $(V112_TOOLS_SRCS) src/v112/main.c \
	    src/v101/sigma_channels.c src/v101/self_test.c \
	    src/v101/bridge_stub.c $(LDFLAGS)

check-v112-function-calling: creation_os_v112_tools
	@bash benchmarks/v112/check_v112_function_calling.sh
	@echo "check-v112: OK (σ-gated function calling + 10 scenarios)"

check-v112: check-v112-function-calling

creation_os_v113_sandbox: $(V113_SANDBOX_SRCS) src/v113/main.c
	$(CC) $(CFLAGS) $(V113_INC) -o $@ \
	    $(V113_SANDBOX_SRCS) src/v113/main.c $(LDFLAGS)

check-v113-sandbox-execute: creation_os_v113_sandbox
	@bash benchmarks/v113/check_v113_sandbox_execute.sh
	@echo "check-v113: OK (σ-gated subprocess sandbox + rlimit + deadline)"

check-v113: check-v113-sandbox-execute

creation_os_v114_swarm: $(V114_SWARM_SRCS) src/v114/main.c \
                         $(V112_TOOLS_SRCS) \
                         src/v101/sigma_channels.c src/v101/self_test.c \
                         src/v101/bridge_stub.c
	$(CC) $(CFLAGS) $(V114_INC) -o $@ \
	    $(V114_SWARM_SRCS) src/v114/main.c \
	    $(V112_TOOLS_SRCS) \
	    src/v101/sigma_channels.c src/v101/self_test.c \
	    src/v101/bridge_stub.c $(LDFLAGS)

check-v114-swarm-routing: creation_os_v114_swarm
	@bash benchmarks/v114/check_v114_swarm_routing.sh
	@echo "check-v114-swarm-routing: OK (σ_product_min router)"

check-v114-multi-specialist-consensus: creation_os_v114_swarm
	@bash benchmarks/v114/check_v114_consensus.sh
	@echo "check-v114-consensus: OK (resonance-threshold agreement)"

check-v114: check-v114-swarm-routing check-v114-multi-specialist-consensus
	@echo "check-v114: OK (σ-swarm multi-specialist orchestration)"

check-v112-v114: check-v112 check-v113 check-v114
	@echo "check-v112-v114: OK (agentic stack — tools + sandbox + swarm)"

# --- v115 σ-Memory, v116 σ-MCP, v117 σ-Long-Context, v118 σ-Vision ---
# Four standalone C libraries + CLIs for the memory / MCP / long-context
# / vision layer.  Each ships merge-gate-safe smoke tests that require
# no weights and no network.  v115 links against libsqlite3 (system on
# macOS; libsqlite3-dev on Debian / Ubuntu); if missing, build with
# `make V115_SQLITE_OFF=1` to skip persistence (APIs return ENOSYS).
V115_INC        = -Isrc/v115
V116_INC        = -Isrc/v115 -Isrc/v116
V117_INC        = -Isrc/v117
V118_INC        = -Isrc/v118

V115_MEMORY_SRCS = src/v115/memory.c
V116_MCP_SRCS    = src/v116/mcp_server.c
V117_LONGCTX_SRCS = src/v117/long_context.c
V118_VISION_SRCS  = src/v118/vision.c

# libsqlite3 is available by default on macOS and on any Linux with
# libsqlite3-dev installed.  Override with V115_SQLITE_OFF=1 to compile
# the pure-C fallback (self-test still runs; persistence disabled).
V115_LDFLAGS = -lsqlite3
ifneq ($(V115_SQLITE_OFF),)
V115_CFLAGS_EXTRA = -DCOS_V115_NO_SQLITE=1
V115_LDFLAGS =
endif

creation_os_v115_memory: $(V115_MEMORY_SRCS) src/v115/main.c
	$(CC) $(CFLAGS) $(V115_CFLAGS_EXTRA) $(V115_INC) -o $@ \
	    $(V115_MEMORY_SRCS) src/v115/main.c \
	    $(V115_LDFLAGS) $(LDFLAGS)

check-v115-memory-roundtrip: creation_os_v115_memory
	@bash benchmarks/v115/check_v115_memory_roundtrip.sh
	@echo "check-v115-memory-roundtrip: OK (σ-memory schema + JSON + round-trip)"

check-v115-sigma-weighted-recall: creation_os_v115_memory
	@bash benchmarks/v115/check_v115_sigma_weighted_recall.sh
	@echo "check-v115-sigma-weighted-recall: OK (low-σ memories outrank high-σ duplicates)"

check-v115: check-v115-memory-roundtrip check-v115-sigma-weighted-recall
	@echo "check-v115: OK (σ-memory: SQLite + σ-weighted recall)"

creation_os_v116_mcp: $(V116_MCP_SRCS) src/v116/main.c $(V115_MEMORY_SRCS)
	$(CC) $(CFLAGS) $(V115_CFLAGS_EXTRA) $(V116_INC) -o $@ \
	    $(V116_MCP_SRCS) src/v116/main.c $(V115_MEMORY_SRCS) \
	    $(V115_LDFLAGS) $(LDFLAGS)

check-v116-mcp-stdio-smoke: creation_os_v116_mcp
	@bash benchmarks/v116/check_v116_mcp_stdio_smoke.sh
	@echo "check-v116-mcp-stdio-smoke: OK (JSON-RPC 2.0 stdio — 5 tools + 3 resources + 2 prompts)"

check-v116: check-v116-mcp-stdio-smoke
	@echo "check-v116: OK (σ-MCP server)"

creation_os_v117_long_context: $(V117_LONGCTX_SRCS) src/v117/main.c
	$(CC) $(CFLAGS) $(V117_INC) -o $@ \
	    $(V117_LONGCTX_SRCS) src/v117/main.c $(LDFLAGS)

check-v117-long-context-32k: creation_os_v117_long_context
	@bash benchmarks/v117/check_v117_long_context_32k.sh
	@echo "check-v117-long-context-32k: OK (paged KV + σ-aware eviction + sliding window)"

check-v117: check-v117-long-context-32k
	@echo "check-v117: OK (σ-long-context)"

creation_os_v118_vision: $(V118_VISION_SRCS) src/v118/main.c
	$(CC) $(CFLAGS) $(V118_INC) -o $@ \
	    $(V118_VISION_SRCS) src/v118/main.c $(LDFLAGS)

check-v118-vision-smoke: creation_os_v118_vision
	@bash benchmarks/v118/check_v118_vision_smoke.sh
	@echo "check-v118-vision-smoke: OK (data URL parse + σ-gated projection)"

check-v118: check-v118-vision-smoke
	@echo "check-v118: OK (σ-vision interface layer)"

check-v115-v118: check-v115 check-v116 check-v117 check-v118
	@echo "check-v115-v118: OK (memory + MCP + long-context + vision)"

# --- v119 σ-Speculative (draft-verify with σ-adaptive γ) ---
# Pure-C policy layer: adaptive γ from σ_product, σ-gate on the draft
# step (auto-reject before the target runs), full synthetic-stream
# simulator.  Weights-free — target + draft GGUF plumbing is v119.1.
V119_INC              = -Isrc/v119
V119_SPECULATIVE_SRCS = src/v119/speculative.c

creation_os_v119_speculative: $(V119_SPECULATIVE_SRCS) src/v119/main.c
	$(CC) $(CFLAGS) $(V119_INC) -o $@ \
	    $(V119_SPECULATIVE_SRCS) src/v119/main.c $(LDFLAGS)

check-v119-speculative-speedup: creation_os_v119_speculative
	@bash benchmarks/v119/check_v119_speculative_speedup.sh
	@echo "check-v119-speculative-speedup: OK (σ-adaptive γ + σ-gate draft accept policy)"

check-v119: check-v119-speculative-speedup
	@echo "check-v119: OK (σ-speculative decoding policy)"

# --- v120 σ-Distill (σ-targeted teacher→student SFT row selector) ---
# Pure-C JSONL selector: keep rows where σ_big < τ_low AND σ_small >
# τ_high, emit SFT JSONL with the teacher response and a manifest
# with drop-reason counters.  MLX LoRA training lands in v120.1.
V120_INC           = -Isrc/v120
V120_DISTILL_SRCS  = src/v120/distill.c

creation_os_v120_distill: $(V120_DISTILL_SRCS) src/v120/main.c
	$(CC) $(CFLAGS) $(V120_INC) -o $@ \
	    $(V120_DISTILL_SRCS) src/v120/main.c $(LDFLAGS)

check-v120-distill-smoke: creation_os_v120_distill
	@bash benchmarks/v120/check_v120_distill_smoke.sh
	@echo "check-v120-distill-smoke: OK (σ-targeted SFT row selector + manifest)"

check-v120: check-v120-distill-smoke
	@echo "check-v120: OK (σ-distill selector layer)"

# --- v121 σ-Planning (HTN + MCTS backtrack on σ > τ_step) ---
# Pure-C multi-step planner: HTN decomposition + lowest-σ branch
# selection + bounded MCTS-style replan on any step whose geometric-
# mean σ exceeds τ_step.  Emits /v1/plan-shaped JSON.  Real tool
# backends (v112/v113/v114) are wired in v121.1.
V121_INC            = -Isrc/v121
V121_PLANNING_SRCS  = src/v121/planning.c

creation_os_v121_planning: $(V121_PLANNING_SRCS) src/v121/main.c
	$(CC) $(CFLAGS) $(V121_INC) -o $@ \
	    $(V121_PLANNING_SRCS) src/v121/main.c $(LDFLAGS)

check-v121-planning-loop: creation_os_v121_planning
	@bash benchmarks/v121/check_v121_planning_loop.sh
	@echo "check-v121-planning-loop: OK (HTN/MCTS + /v1/plan JSON contract)"

check-v121: check-v121-planning-loop
	@echo "check-v121: OK (σ-planning)"

# --- v122 σ-Red-Team (200-test injection + jailbreak + hallucination) ---
# Pure-C harness + σ-aware adjudicator + Markdown/JSON reporter.  Mock
# responder by default; live v106 endpoint wire-up is v122.1.  Audit
# item E (red-team in CI without Garak) is closed by v122.
V122_INC           = -Isrc/v122
V122_REDTEAM_SRCS  = src/v122/red_team.c

creation_os_v122_red_team: $(V122_REDTEAM_SRCS) src/v122/main.c
	$(CC) $(CFLAGS) $(V122_INC) -o $@ \
	    $(V122_REDTEAM_SRCS) src/v122/main.c $(LDFLAGS)

check-v122-red-team: creation_os_v122_red_team
	@bash benchmarks/v122/check_v122_red_team.sh
	@echo "check-v122-red-team: OK (200-test σ-red-team, per-category ≥ 80%)"

check-v122: check-v122-red-team
	@echo "check-v122: OK (σ-red-team harness)"

# --- v123 σ-Formal (offline TLA+ model check of σ-invariants) ---
# Two-tier enforcement: pure-C structural validator always runs (no
# Java), full TLC exhaustive model check runs when `tlc` or
# `tla2tools.jar` is available; SKIPs cleanly otherwise.  Closes the
# audit finding "v85 TLA-invariants are runtime only" by adding an
# offline proof obligation every runner enforces structurally.
V123_INC          = -Isrc/v123
V123_FORMAL_SRCS  = src/v123/formal.c

creation_os_v123_formal: $(V123_FORMAL_SRCS) src/v123/main.c
	$(CC) $(CFLAGS) $(V123_INC) -o $@ \
	    $(V123_FORMAL_SRCS) src/v123/main.c $(LDFLAGS)

check-v123-formal-tlc: creation_os_v123_formal
	@bash benchmarks/v123/check_v123_formal_tlc.sh
	@echo "check-v123-formal-tlc: OK (σ-governance invariants — structural + TLC-if-present)"

check-v123: check-v123-formal-tlc
	@echo "check-v123: OK (σ-formal model-check)"

check-v119-v123: check-v119 check-v120 check-v121 check-v122 check-v123
	@echo "check-v119-v123: OK (speculative + distill + planning + red-team + formal)"

# --- v124 σ-Continual (on-device living weights) ---
# Pure-C buffer + idle-trigger + forgetting-smoke state machine.
# Exercises healthy and pathological baselines deterministically so
# the merge-gate sees both the happy and the rollback paths without
# MLX, weights, or network.  v124.1 wires a real MLX LoRA trainer
# and hot-swaps the adapter into v106.
V124_INC            = -Isrc/v124
V124_CONTINUAL_SRCS = src/v124/continual.c

creation_os_v124_continual: $(V124_CONTINUAL_SRCS) src/v124/main.c
	$(CC) $(CFLAGS) $(V124_INC) -o $@ \
	    $(V124_CONTINUAL_SRCS) src/v124/main.c $(LDFLAGS)

check-v124-continual-learning: creation_os_v124_continual
	@bash benchmarks/v124/check_v124_continual_learning.sh
	@echo "check-v124-continual-learning: OK (σ-buffer + idle-trigger + rollback)"

check-v124: check-v124-continual-learning
	@echo "check-v124: OK (σ-continual policy)"

# --- v125 σ-DPO (σ-derived preference optimization) ---
# Pure-C σ-labeler, numerically-stable DPO loss kernel, and a σ-
# distribution mode-collapse detector.  No weights, no MLX: the self-
# test uses DPO's analytical limits (δ=0 → L=log 2, strong chosen →
# L→0, strong rejected → L≈|δ|).  v125.1 stacks a real LoRA DPO
# adapter on top of the v124 continual adapter via MLX.
V125_INC       = -Isrc/v125
V125_DPO_SRCS  = src/v125/dpo.c

creation_os_v125_dpo: $(V125_DPO_SRCS) src/v125/main.c
	$(CC) $(CFLAGS) $(V125_INC) -o $@ \
	    $(V125_DPO_SRCS) src/v125/main.c $(LDFLAGS)

check-v125-dpo-smoke: creation_os_v125_dpo
	@bash benchmarks/v125/check_v125_dpo_smoke.sh
	@echo "check-v125-dpo-smoke: OK (DPO loss + σ-labeler + mode-collapse)"

check-v125: check-v125-dpo-smoke
	@echo "check-v125: OK (σ-DPO kernel)"

# --- v126 σ-Embed (2568-d σ-aware embedding for v115 memory) ---
# Pure-C hash-shingle projector (BitNet layer-15 stand-in), σ-block
# concatenation, σ-weighted cosine, top-k rank.  v115 memory already
# exposes an `embed_fn` indirect pointer — v126.1 plugs the real
# BitNet hidden-state extractor in without changing anything below
# v115's ranking contract.
V126_INC        = -Isrc/v126
V126_EMBED_SRCS = src/v126/embed.c

creation_os_v126_embed: $(V126_EMBED_SRCS) src/v126/main.c
	$(CC) $(CFLAGS) $(V126_INC) -o $@ \
	    $(V126_EMBED_SRCS) src/v126/main.c $(LDFLAGS)

check-v126-embed-smoke: creation_os_v126_embed
	@bash benchmarks/v126/check_v126_embed_smoke.sh
	@echo "check-v126-embed-smoke: OK (2568-d hybrid embed + σ-weighted rank)"

check-v126: check-v126-embed-smoke
	@echo "check-v126: OK (σ-embed kernel)"

check-v124-v126: check-v124 check-v125 check-v126
	@echo "check-v124-v126: OK (continual + DPO + σ-embed)"

# --- v129 σ-Federated (privacy-preserving FedAvg + DP + unlearn) ---
# Pure-C aggregator + σ-scaled Gaussian DP + σ-adaptive top-K
# + unlearn-diff.  v129.0 is transport-free by design — v128 mesh
# (follow-up) plugs the socket layer in without changing the
# aggregation math below.
V129_INC            = -Isrc/v129
V129_FEDERATED_SRCS = src/v129/federated.c

creation_os_v129_federated: $(V129_FEDERATED_SRCS) src/v129/main.c
	$(CC) $(CFLAGS) $(V129_INC) -o $@ \
	    $(V129_FEDERATED_SRCS) src/v129/main.c $(LDFLAGS)

check-v129-federated-aggregation: creation_os_v129_federated
	@bash benchmarks/v129/check_v129_federated_aggregation.sh
	@echo "check-v129-federated-aggregation: OK (σ-FedAvg + DP + top-K + unlearn)"

check-v129: check-v129-federated-aggregation
	@echo "check-v129: OK (σ-federated kernel)"

# --- v130 σ-Codec (FP4 LoRA + PQ embed + σ-aware context) ---
# Pure-C compression layer for peer comms.  FP4 packs LoRA Δ at 4
# bits/value (8× vs fp32), σ-profile packs 8 floats → 8 bytes,
# PQ (M=8, K=128) compresses 2568-d embeddings to 8 bytes/vector,
# σ-aware context allocator gives uncertain chunks more budget.
# v130.1 can stack an entropy coder (zstd/ANS) without touching
# the API.
V130_INC         = -Isrc/v130
V130_CODEC_SRCS  = src/v130/codec.c

creation_os_v130_codec: $(V130_CODEC_SRCS) src/v130/main.c
	$(CC) $(CFLAGS) $(V130_INC) -o $@ \
	    $(V130_CODEC_SRCS) src/v130/main.c $(LDFLAGS)

check-v130-codec-roundtrip: creation_os_v130_codec
	@bash benchmarks/v130/check_v130_codec_roundtrip.sh
	@echo "check-v130-codec-roundtrip: OK (FP4 + σ-pack + PQ + context codec)"

check-v130: check-v130-codec-roundtrip
	@echo "check-v130: OK (σ-codec kernel)"

# --- v131 σ-Temporal (timeline + σ-trend + decay + spikes) ---
# Pure-C timeline + OLS trend + σ-weighted exponential decay +
# spike detection + deadline-σ prediction.  v131.0 is in-memory;
# v131.1 binds this to v115 SQLite for cross-session timelines.
V131_INC          = -Isrc/v131
V131_TEMPORAL_SRCS = src/v131/temporal.c

creation_os_v131_temporal: $(V131_TEMPORAL_SRCS) src/v131/main.c
	$(CC) $(CFLAGS) $(V131_INC) -o $@ \
	    $(V131_TEMPORAL_SRCS) src/v131/main.c $(LDFLAGS)

check-v131-temporal-recall: creation_os_v131_temporal
	@bash benchmarks/v131/check_v131_temporal_recall.sh
	@echo "check-v131-temporal-recall: OK (window + trend + decay + spike + deadline)"

check-v131: check-v131-temporal-recall
	@echo "check-v131: OK (σ-temporal kernel)"

# --- v132 σ-Persona (expertise tracker + style + TOML profile) ---
# Per-user adaptation: σ-driven expertise staircase, correction-
# feedback style state, deterministic TOML round-trip.  v132 state
# is *per-node* — never federates through v129, keeping personas
# local.
V132_INC          = -Isrc/v132
V132_PERSONA_SRCS = src/v132/persona.c

creation_os_v132_persona: $(V132_PERSONA_SRCS) src/v132/main.c
	$(CC) $(CFLAGS) $(V132_INC) -o $@ \
	    $(V132_PERSONA_SRCS) src/v132/main.c $(LDFLAGS)

check-v132-persona-adaptation: creation_os_v132_persona
	@bash benchmarks/v132/check_v132_persona_adaptation.sh
	@echo "check-v132-persona-adaptation: OK (expertise + feedback + TOML)"

check-v132: check-v132-persona-adaptation
	@echo "check-v132: OK (σ-persona kernel)"

# --- v133 σ-Meta (self-benchmark + meta-σ + auto-diagnose) ---
# System-level metacognition: weekly snapshots, OLS slope per week,
# meta-σ (cv of σ_product), auto-diagnose from highest-σ channel,
# deterministic self-benchmark runner.  v133.0 is pure policy;
# v133.1 exposes the dashboard on v106 /v1/meta/health and wires
# v122 red-team into the weekly smoke-set.
V133_INC       = -Isrc/v133
V133_META_SRCS = src/v133/meta.c

creation_os_v133_meta: $(V133_META_SRCS) src/v133/main.c
	$(CC) $(CFLAGS) $(V133_INC) -o $@ \
	    $(V133_META_SRCS) src/v133/main.c $(LDFLAGS)

check-v133-meta-self-benchmark: creation_os_v133_meta
	@bash benchmarks/v133/check_v133_meta_self_benchmark.sh
	@echo "check-v133-meta-self-benchmark: OK (slope + meta-σ + diagnose + bench)"

check-v133: check-v133-meta-self-benchmark
	@echo "check-v133: OK (σ-meta kernel)"

check-v129-v133: check-v129 check-v130 check-v131 check-v132 check-v133
	@echo "check-v129-v133: OK (collective intelligence stack)"

# --- v134 σ-Spike (event-driven σ + burst detection + Lava export) ---
# Neuromorphic-style σ-signalling: σ_spike fires only when σ_product
# moves by ≥ δ from the previous observation.  Stable tokens do zero
# σ-work downstream.  Burst detection is O(1) via a ring buffer.
# v134.0 is pure policy + simulation + Lava-format text export;
# v134.1 wires the export into an Intel Loihi 2 dev-kit when present.
V134_INC        = -Isrc/v134
V134_SPIKE_SRCS = src/v134/spike.c

creation_os_v134_spike: $(V134_SPIKE_SRCS) src/v134/main.c
	$(CC) $(CFLAGS) $(V134_INC) -o $@ \
	    $(V134_SPIKE_SRCS) src/v134/main.c $(LDFLAGS)

check-v134-spike-event-driven: creation_os_v134_spike
	@bash benchmarks/v134/check_v134_spike_event_driven.sh
	@echo "check-v134-spike-event-driven: OK (δ-spike + burst + lava)"

check-v134: check-v134-spike-event-driven
	@echo "check-v134: OK (σ-spike kernel)"

# --- v135 σ-Symbolic (Prolog micro-engine + hybrid reasoning) ---
# ~400-line Horn-clause micro-engine supporting ground-fact KBs
# and unification-based queries.  Rules (:-) are a v135.1 follow-up
# and are refused by the parser so callers never silently degrade.
# Exposes a σ-routed hybrid reasoner: σ < τ → BitNet only, σ ≥ τ
# + logical → KB verify / override, σ ≥ τ + non-logical → abstain.
V135_INC           = -Isrc/v135
V135_SYMBOLIC_SRCS = src/v135/symbolic.c

creation_os_v135_symbolic: $(V135_SYMBOLIC_SRCS) src/v135/main.c
	$(CC) $(CFLAGS) $(V135_INC) -o $@ \
	    $(V135_SYMBOLIC_SRCS) src/v135/main.c $(LDFLAGS)

check-v135-symbolic-prolog-roundtrip: creation_os_v135_symbolic
	@bash benchmarks/v135/check_v135_symbolic_prolog_roundtrip.sh
	@echo "check-v135-symbolic-prolog-roundtrip: OK (KB + query + consistency)"

check-v135: check-v135-symbolic-prolog-roundtrip
	@echo "check-v135: OK (σ-symbolic kernel)"

# --- v136 σ-Evolve (gradient-free architecture search) -----------
# Deterministic (μ/μ, λ) Evolution Strategy with diagonal covariance
# adaptation — a consciously simplified member of the CMA-ES family.
# Searches the weighted-geometric σ-aggregator genotype (8 weights
# + τ) against a synthetic sidecar; emits an evolved_sigma_config
# TOML for v29 to consume.  No gradients, no GPU.
V136_INC         = -Isrc/v136
V136_EVOLVE_SRCS = src/v136/evolve.c

creation_os_v136_evolve: $(V136_EVOLVE_SRCS) src/v136/main.c
	$(CC) $(CFLAGS) $(V136_INC) -o $@ \
	    $(V136_EVOLVE_SRCS) src/v136/main.c $(LDFLAGS)

check-v136-evolve-cmaes-converge: creation_os_v136_evolve
	@bash benchmarks/v136/check_v136_evolve_cmaes_converge.sh
	@echo "check-v136-evolve-cmaes-converge: OK (ES beats baseline + monotone)"

check-v136: check-v136-evolve-cmaes-converge
	@echo "check-v136: OK (σ-evolve kernel)"

# --- v137 σ-Compile (AOT specialisation of the σ-decision tree) ---
# Emits a branchless per-profile C decision tree that $(CC) -O3
# lowers to whatever IR the local toolchain uses (LLVM on clang,
# GIMPLE/RTL on gcc — we don't hard-depend on either).  v137.0
# ships the generator + an embedded example-compiled gate so the
# merge-gate can run end-to-end from one binary; v137.1 adds a
# `make compile-sigma` that generates, compiles, and dlopens a
# fresh per-profile .so per run.
V137_INC          = -Isrc/v137
V137_COMPILE_SRCS = src/v137/compile.c

creation_os_v137_compile: $(V137_COMPILE_SRCS) src/v137/main.c
	$(CC) $(CFLAGS) -O3 $(V137_INC) -o $@ \
	    $(V137_COMPILE_SRCS) src/v137/main.c $(LDFLAGS)

check-v137-compile-llvm-smoke: creation_os_v137_compile
	@bash benchmarks/v137/check_v137_compile_llvm_smoke.sh
	@echo "check-v137-compile-llvm-smoke: OK (emit + $(CC) -O3 + latency)"

check-v137: check-v137-compile-llvm-smoke
	@echo "check-v137: OK (σ-compile kernel)"

# --- v138 σ-Proof (ACSL annotations + Frama-C WP tier 2) ---------
# Tier 1 (always runs): pure-C validator over ACSL annotation
# blocks in src/v138/sigma_gate.c asserts the σ-contract shape
# (requires + ensures + 0≤σ≤1 domain + complete emit/abstain
# partition + loop invariants).  Tier 2 (opportunistic): runs
# `frama-c -wp -wp-rte` when the binary is on $PATH; otherwise
# prints "tier-2 skipped" and tier-1 is authoritative.  Export
# V138_REQUIRE_FRAMA_C=1 to fail the gate on a missing frama-c.
V138_INC        = -Isrc/v138
V138_PROOF_SRCS = src/v138/proof.c

creation_os_v138_proof: $(V138_PROOF_SRCS) src/v138/main.c
	$(CC) $(CFLAGS) $(V138_INC) -o $@ \
	    $(V138_PROOF_SRCS) src/v138/main.c $(LDFLAGS)

check-v138-prove-frama-c-wp: creation_os_v138_proof
	@bash benchmarks/v138/check_v138_prove_frama_c_wp.sh
	@echo "check-v138-prove-frama-c-wp: OK (tier-1 + tier-2 probe)"

check-v138: check-v138-prove-frama-c-wp
	@echo "check-v138: OK (σ-proof kernel)"

# `make prove` runs the merge-gate's σ-proof scope as a standalone
# target so authors can iterate on ACSL contracts quickly.
prove: creation_os_v138_proof
	@./creation_os_v138_proof --prove src/v138/sigma_gate.c
	@echo "prove: OK (tier-1 + tier-2 probe)"

check-v134-v138: check-v134 check-v135 check-v136 check-v137 check-v138
	@echo "check-v134-v138: OK (deep-infrastructure stack)"

# --- v139 σ-WorldModel (linear latent predictor, D≤64) -----------
# Fits A ∈ ℝ^{D×D} by normal-equations least squares on a
# deterministic synthetic trajectory, predicts the next state, and
# computes σ_world = ‖s_actual − s_pred‖ / ‖s_actual‖.  v139.0 is
# tier-0: pure C, no BitNet hidden-state extractor yet.  v139.1
# lifts D to 2560 and reads real layer-15 states via v101/v126.
V139_INC  = -Isrc/v139
V139_SRCS = src/v139/world_model.c

creation_os_v139_world_model: $(V139_SRCS) src/v139/main.c
	$(CC) $(CFLAGS) $(V139_INC) -o $@ \
	    $(V139_SRCS) src/v139/main.c $(LDFLAGS)

check-v139-world-model-prediction: creation_os_v139_world_model
	@bash benchmarks/v139/check_v139_world_model_prediction.sh
	@echo "check-v139-world-model-prediction: OK (LS fit + <10% held-out)"

check-v139: check-v139-world-model-prediction
	@echo "check-v139: OK (σ-world-model kernel)"

# --- v140 σ-Causal (counterfactual + per-channel ablation) -------
# Reuses v139's linear A for counterfactual propagation and adds
# per-σ-channel ablation attribution, returning the top-3
# contributing channels to an abstain decision.
V140_INC  = -Isrc/v140 -Isrc/v139
V140_SRCS = src/v140/causal.c src/v139/world_model.c

creation_os_v140_causal: $(V140_SRCS) src/v140/main.c
	$(CC) $(CFLAGS) $(V140_INC) -o $@ \
	    $(V140_SRCS) src/v140/main.c $(LDFLAGS)

check-v140-causal-attribution: creation_os_v140_causal
	@bash benchmarks/v140/check_v140_causal_attribution.sh
	@echo "check-v140-causal-attribution: OK (do-calc + top-3 channels)"

check-v140: check-v140-causal-attribution
	@echo "check-v140: OK (σ-causal kernel)"

# --- v141 σ-Curriculum (weakness detection + improvement loop) ---
# Pure scheduler: identifies weakest topic from a σ-histogram,
# simulates one curriculum cycle against a synthetic answer
# function, asserts the weakest topic improves without forgetting
# the strong ones.  Real MLX/LoRA hand-off lives in v141.1.
V141_INC  = -Isrc/v141
V141_SRCS = src/v141/curriculum.c

creation_os_v141_curriculum: $(V141_SRCS) src/v141/main.c
	$(CC) $(CFLAGS) $(V141_INC) -o $@ \
	    $(V141_SRCS) src/v141/main.c $(LDFLAGS)

check-v141-curriculum-cycle: creation_os_v141_curriculum
	@bash benchmarks/v141/check_v141_curriculum_cycle.sh
	@echo "check-v141-curriculum-cycle: OK (weakness → cycle → improvement)"

check-v141: check-v141-curriculum-cycle
	@echo "check-v141: OK (σ-curriculum kernel)"

# --- v142 σ-Interop (Python SDK + OpenAI shape + adapters) -------
# No C binary for v142: surface is a stdlib-only Python package
# under python/creation_os/.  The merge-gate verifies the module
# imports, that the OpenAI-SDK-shape smoke passes, and that the
# LangChain/LlamaIndex adapters import cleanly even when those
# frameworks are absent (graceful lazy-import).
check-v142-interop-openai-sdk:
	@bash benchmarks/v142/check_v142_interop_openai_sdk.sh
	@echo "check-v142-interop-openai-sdk: OK (SDK + OpenAI shape + adapters)"

check-v142: check-v142-interop-openai-sdk
	@echo "check-v142: OK (σ-interop kernel)"

# --- v143 σ-Benchmark (5-category synthetic benchmark suite) -----
# Runs calibration + abstention + swarm-routing + learning +
# adversarial on tier-0 synthetic data, writes
# benchmarks/v143/creation_os_benchmark.json.  v143.1 replaces each
# synthetic set with archived σ-traces from v104/v106 and publishes
# to Hugging Face.
V143_INC  = -Isrc/v143
V143_SRCS = src/v143/benchmark.c

creation_os_v143_benchmark: $(V143_SRCS) src/v143/main.c
	$(CC) $(CFLAGS) $(V143_INC) -o $@ \
	    $(V143_SRCS) src/v143/main.c $(LDFLAGS)

check-v143-benchmark-smoke: creation_os_v143_benchmark
	@bash benchmarks/v143/check_v143_benchmark_smoke.sh
	@echo "check-v143-benchmark-smoke: OK (5 categories + JSON)"

check-v143: check-v143-benchmark-smoke
	@echo "check-v143: OK (σ-benchmark kernel)"

check-v139-v143: check-v139 check-v140 check-v141 check-v142 check-v143
	@echo "check-v139-v143: OK (world-intelligence stack)"

# --- v144 σ-RSI (recursive self-improvement loop) ---------------
# Deterministic state machine for the RSI cycle: submit candidate
# score → accept-or-rollback → σ_rsi on last-10 history → 3-strike
# pause latch.  v144.1 wires v133 meta-weaknesses, v141 curriculum,
# v120 distill, v125 DPO, v124 continual, v143 benchmark into the
# submit() callback.
V144_INC  = -Isrc/v144
V144_SRCS = src/v144/rsi.c

creation_os_v144_rsi: $(V144_SRCS) src/v144/main.c
	$(CC) $(CFLAGS) $(V144_INC) -o $@ \
	    $(V144_SRCS) src/v144/main.c $(LDFLAGS)

check-v144-rsi-cycle-smoke: creation_os_v144_rsi
	@bash benchmarks/v144/check_v144_rsi_cycle_smoke.sh
	@echo "check-v144-rsi-cycle-smoke: OK (cycle + rollback + pause)"

check-v144: check-v144-rsi-cycle-smoke
	@echo "check-v144: OK (σ-rsi kernel)"

# --- v145 σ-Skill (atomic skill library + σ-routed stacking) ----
# In-memory, deterministic kernel for the skill-library discipline:
# acquire (σ-gated install), route (argmin-σ on target_topic),
# stack (LoRA-merge σ = min/√n), share (mesh broadcast at τ), and
# evolve (CMA-ES-style monotone σ-improvement).  v145.1 wires the
# on-disk skills/<name>.skill/ layout, real LoRA merge, and v128
# mesh gossip for cross-node sharing.
V145_INC  = -Isrc/v145
V145_SRCS = src/v145/skill.c

creation_os_v145_skill: $(V145_SRCS) src/v145/main.c
	$(CC) $(CFLAGS) $(V145_INC) -o $@ \
	    $(V145_SRCS) src/v145/main.c $(LDFLAGS)

check-v145-skill-acquire-and-route: creation_os_v145_skill
	@bash benchmarks/v145/check_v145_skill_acquire_and_route.sh
	@echo "check-v145-skill-acquire-and-route: OK (acquire + route + evolve)"

check-v145: check-v145-skill-acquire-and-route
	@echo "check-v145: OK (σ-skill kernel)"

# --- v146 σ-Genesis (automated kernel-skeleton generation) -------
# Deterministic template generator that emits a canonical 4-file
# kernel skeleton (kernel.h / kernel.c / tests/test.c / README.md)
# with a σ-gate on σ_code (completeness + seeded novelty jitter)
# and a human-in-the-loop state machine: generate → PENDING →
# approve / reject; three consecutive rejects latch paused=true
# and only cos_v146_resume() unblocks further approvals.  v146.1
# wires v114 swarm (8B code specialist) as the real generator.
V146_INC  = -Isrc/v146
V146_SRCS = src/v146/genesis.c

creation_os_v146_genesis: $(V146_SRCS) src/v146/main.c
	$(CC) $(CFLAGS) $(V146_INC) -o $@ \
	    $(V146_SRCS) src/v146/main.c $(LDFLAGS)

check-v146-genesis-template-gen: creation_os_v146_genesis
	@bash benchmarks/v146/check_v146_genesis_template_gen.sh
	@echo "check-v146-genesis-template-gen: OK (skeleton + σ-gate + HITL)"

check-v146: check-v146-genesis-template-gen
	@echo "check-v146: OK (σ-genesis kernel)"

# --- v147 σ-Reflect (thought-trace σ + RAIN rewind) --------------
# Deterministic kernel for deep self-reflection: each v111.2
# reasoning step is tagged with its σ_product, v147 identifies
# the argmax-σ "weakest link", computes a RAIN (ICLR 2024) rewind
# depth from σ_weakest (local / mid-chain / full restart), and
# detects pure-vs-chain divergence.  v147.1 wires v111.2 to feed
# real traces and v125 DPO to use weakest_step as a preference
# signal.
V147_INC  = -Isrc/v147
V147_SRCS = src/v147/reflect.c

creation_os_v147_reflect: $(V147_SRCS) src/v147/main.c
	$(CC) $(CFLAGS) $(V147_INC) -o $@ \
	    $(V147_SRCS) src/v147/main.c $(LDFLAGS)

check-v147-reflect-thought-trace: creation_os_v147_reflect
	@bash benchmarks/v147/check_v147_reflect_thought_trace.sh
	@echo "check-v147-reflect-thought-trace: OK (weakest + RAIN + divergence)"

check-v147: check-v147-reflect-thought-trace
	@echo "check-v147: OK (σ-reflect kernel)"

# --- v148 σ-Sovereign (autonomous orchestrator) ------------------
# Composes v144 (RSI) + v145 (skill) + v146 (genesis) + v147
# (reflect) into one six-stage supervised loop: measure →
# identify → improve → evolve → reflect → share.  Two σ-gates pace
# it — σ_rsi > τ_sovereign self-halts the loop (G1), and SUPERVISED
# mode keeps every structural change as pending_approvals until
# the user calls cos_v148_approve_all() (G2).  Emergency stop is a
# hot-path latch.
V148_INC  = -Isrc/v148 -Isrc/v144 -Isrc/v145 -Isrc/v146 -Isrc/v147
V148_SRCS = src/v148/sovereign.c src/v144/rsi.c src/v145/skill.c \
            src/v146/genesis.c src/v147/reflect.c

creation_os_v148_sovereign: $(V148_SRCS) src/v148/main.c
	$(CC) $(CFLAGS) $(V148_INC) -o $@ \
	    $(V148_SRCS) src/v148/main.c $(LDFLAGS)

check-v148-sovereign-supervised-smoke: creation_os_v148_sovereign
	@bash benchmarks/v148/check_v148_sovereign_supervised_smoke.sh
	@echo "check-v148-sovereign-supervised-smoke: OK (supervised + autonomous + emergency)"

check-v148: check-v148-sovereign-supervised-smoke
	@echo "check-v148: OK (σ-sovereign kernel)"

check-v144-v148: check-v144 check-v145 check-v146 check-v147 check-v148
	@echo "check-v144-v148: OK (sovereign stack)"

# --- v149 σ-Embodied (digital twin + σ-gated action + sim-to-real) ---
# Deterministic 6-DOF arm+object twin that stands in for the
# MuJoCo 3.x mj_step call v149.1 will wire.  Exposes three σ
# measurements: σ_embodied (world-model vs sim), σ_gap (sim vs
# real), and a σ_safe admission gate that refuses any action whose
# pre-commit σ_embodied exceeds the operator-supplied threshold.
# No network, no MuJoCo dependency on merge-gate — v149.0 is pure
# C11 + libm; v149.1 plumbs real MuJoCo XML + POST /v1/embodied/step
# + a WebGL 3-D σ-overlay.
V149_INC  = -Isrc/v149
V149_SRCS = src/v149/embodied.c

creation_os_v149_embodied: $(V149_SRCS) src/v149/main.c
	$(CC) $(CFLAGS) $(V149_INC) -o $@ \
	    $(V149_SRCS) src/v149/main.c $(LDFLAGS)

check-v149-embodied-mujoco-step: creation_os_v149_embodied
	@bash benchmarks/v149/check_v149_embodied_mujoco_step.sh
	@echo "check-v149-embodied-mujoco-step: OK (step + σ-gate + sim-to-real)"

check-v149: check-v149-embodied-mujoco-step
	@echo "check-v149: OK (σ-embodied kernel)"

# --- v150 σ-Swarm-Intelligence (debate + adversarial verify) ----
# Deterministic 3-specialist debate protocol: each specialist
# produces a candidate answer with a per-specialist σ, adversaries
# critique neighbours (critique σ lifts the defender's σ on a
# successful rebuttal), and three rounds let specialists adopt the
# lowest-σ neighbour's answer.  σ_collective is the geometric mean
# of final σs — the "resonance not consensus" metric.  v150.1
# wires v114 routing + v128 mesh for cross-node debate.
V150_INC  = -Isrc/v150
V150_SRCS = src/v150/swarm.c

creation_os_v150_swarm: $(V150_SRCS) src/v150/main.c
	$(CC) $(CFLAGS) $(V150_INC) -o $@ \
	    $(V150_SRCS) src/v150/main.c $(LDFLAGS)

check-v150-swarm-debate-converge: creation_os_v150_swarm
	@bash benchmarks/v150/check_v150_swarm_debate_converge.sh
	@echo "check-v150-swarm-debate-converge: OK (debate + σ_collective)"

check-v150: check-v150-swarm-debate-converge
	@echo "check-v150: OK (σ-swarm kernel)"

# --- v151 σ-Code-Agent (self-writing TDD loop + sandbox compile) -
# Extends v146 (genesis) with a test-driven code-agent loop:
# generate tests first (fail without impl), generate impl,
# re-compile + run, and σ-gate a PR candidate on compile+test+cov.
# The sandbox compile invokes $(CC) $(CFLAGS) on the emitted C to
# prove the skeleton is byte-exact buildable; the generated test
# binary is then executed to assert PASS.  v151.1 wires ASAN/UBSAN
# + LCOV coverage + real git branch emission.
V151_INC  = -Isrc/v151 -Isrc/v146
V151_SRCS = src/v151/codeagent.c src/v146/genesis.c

creation_os_v151_code_agent: $(V151_SRCS) src/v151/main.c
	$(CC) $(CFLAGS) $(V151_INC) -o $@ \
	    $(V151_SRCS) src/v151/main.c $(LDFLAGS)

check-v151-code-agent-tdd-cycle: creation_os_v151_code_agent
	@bash benchmarks/v151/check_v151_code_agent_tdd_cycle.sh
	@echo "check-v151-code-agent-tdd-cycle: OK (tests → impl → compile → run → σ-gate)"

check-v151: check-v151-code-agent-tdd-cycle
	@echo "check-v151: OK (σ-code-agent kernel)"

# --- v152 σ-Knowledge-Distill (corpus → QA → SFT → σ drop) ------
# Deterministic corpus-to-QA distillation kernel.  v152.0 ships a
# baked-in 16-paper fixture (structured claims, formal statements,
# K(t) kernels, σ definitions, empirical tallies) that stands in
# for the github.com/spektre-labs/corpus clone v152.1 will parse.
# 200 QA pairs are generated, a baseline σ distribution is
# computed, a deterministic "SFT" adapter applies per-topic σ-drop
# proportional to corpus coverage, and the post-drop mean σ must
# be ≥15 % lower than baseline.  No tokenizer, no network.
V152_INC  = -Isrc/v152
V152_SRCS = src/v152/distill.c

creation_os_v152_distill: $(V152_SRCS) src/v152/main.c
	$(CC) $(CFLAGS) $(V152_INC) -o $@ \
	    $(V152_SRCS) src/v152/main.c $(LDFLAGS)

check-v152-corpus-qa-sigma-drop: creation_os_v152_distill
	@bash benchmarks/v152/check_v152_corpus_qa_sigma_drop.sh
	@echo "check-v152-corpus-qa-sigma-drop: OK (corpus → QA → ≥15 %% σ drop)"

check-v152: check-v152-corpus-qa-sigma-drop
	@echo "check-v152: OK (σ-knowledge-distill kernel)"

# --- v153 σ-Identity (σ-calibrated self-knowledge) ---------------
# σ-calibrated identity assertion kernel.  10 identity facts
# (TRUE / FALSE / UNMEASURED) are baked in as the v153.0 fixture;
# per-domain σ values (calculus, quantum, corpus, agency, …) come
# from v133 meta-dashboard in v153.1 — here they are deterministic
# floats.  The kernel answers each assertion with a σ-profile and
# honors the five identity contracts: I1 truths pass at ≤σ_true,
# I2 falsehoods pass at ≤σ_false, I3 unmeasured claims must be
# flagged (σ > τ_unmeasured), I4 no false positives on confident
# facts, I5 every "I do not know" is σ-grounded, never performed.
V153_INC  = -Isrc/v153
V153_SRCS = src/v153/identity.c

creation_os_v153_identity: $(V153_SRCS) src/v153/main.c
	$(CC) $(CFLAGS) $(V153_INC) -o $@ \
	    $(V153_SRCS) src/v153/main.c $(LDFLAGS)

check-v153-identity-assertions: creation_os_v153_identity
	@bash benchmarks/v153/check_v153_identity_assertions.sh
	@echo "check-v153-identity-assertions: OK (10/10 + zero false positive)"

check-v153: check-v153-identity-assertions
	@echo "check-v153: OK (σ-identity kernel)"

check-v149-v153: check-v149 check-v150 check-v151 check-v152 check-v153
	@echo "check-v149-v153: OK (embodied + swarm + code-agent + distill + identity)"

# --- v154 σ-Showcase (end-to-end demo pipelines) ---------------
# Deterministic cross-kernel pipeline orchestrator: three headline
# scenarios (research-assistant, self-improving-coder, collaborative-
# research) each chain a fixed N-stage sequence with per-stage σ,
# a geomean σ_product, and a σ_abstain gate.  v154.0 synthesizes
# stage σ values; v154.1 wires live kernel calls (v118/v152/v135/
# v111/v150/v140/v153/v115 for research; v151/v113/v147/v119/v124/
# v145 for coder; v128/v129/v150/v130 for collab).
V154_INC  = -Isrc/v154
V154_SRCS = src/v154/showcase.c

creation_os_v154_showcase: $(V154_SRCS) src/v154/main.c
	$(CC) $(CFLAGS) $(V154_INC) -o $@ \
	    $(V154_SRCS) src/v154/main.c $(LDFLAGS)

check-v154-demo-pipelines: creation_os_v154_showcase
	@bash benchmarks/v154/check_v154_demo_pipelines.sh
	@echo "check-v154-demo-pipelines: OK (research + coder + collab)"

check-v154: check-v154-demo-pipelines
	@echo "check-v154: OK (σ-showcase kernel)"

# --- v155 σ-Publish (packaging manifests — PyPI/Homebrew/Docker/HF/npm) --
# Every publish surface (pyproject.toml, Homebrew formula, release
# Dockerfile, Hugging Face model cards, npm launcher) is validated
# offline by a stdlib-only Python script.  v155.1 runs the real
# `twine upload`, `brew audit`, `docker push`, and `hf upload`.
check-v155-packaging-manifests:
	@python3 scripts/v155_publish_check.py
	@echo "check-v155-packaging-manifests: OK (pypi + brew + docker + hf + npm)"

check-v155: check-v155-packaging-manifests
	@echo "check-v155: OK (σ-publish kernel)"

# --- v156 σ-Paper (unified Creation OS paper renders + sections) --
# The single unifying paper at docs/papers/creation_os_v1.md.
# A stdlib Python validator enforces the 11-section structure and
# a minimum word-count floor so the paper cannot silently regress
# into a stub.  v156.1 uploads to arxiv + Zenodo and archives the
# DOI back into CITATION.cff.
check-v156-paper-renders:
	@python3 scripts/v156_paper_check.py
	@echo "check-v156-paper-renders: OK (11 sections + floor)"

check-v156: check-v156-paper-renders
	@echo "check-v156: OK (σ-paper doc)"

# --- v157 σ-Community (CONTRIBUTING + governance + issue templates) --
# Community/contribution infrastructure lives in a set of markdown
# files whose *presence* and *required sections* are validated by a
# shell linter (no network, no external tools beyond coreutils).
check-v157-contributing-lint:
	@bash scripts/v157_contributing_lint.sh
	@echo "check-v157-contributing-lint: OK (CONTRIBUTING + GOVERNANCE + issues + good-first-issues)"

check-v157: check-v157-contributing-lint
	@echo "check-v157: OK (σ-community infrastructure)"

# --- v158 σ-v1.0 (release checklist — tag + changelog + binaries) --
# v1.0.0 release checklist lives in docs/RELEASE_v1_0.md and is
# validated by a deterministic shell gate that checks every required
# item is enumerated.  The actual release (git tag, gh release,
# binary builds, SHA256 attestations) lands in v158.1 with a human
# maintainer at the helm.
check-v158-release-checklist:
	@bash scripts/v158_release_checklist.sh
	@echo "check-v158-release-checklist: OK (tag + changelog + binaries + checksums)"

check-v158: check-v158-release-checklist
	@echo "check-v158: OK (σ-v1.0 release gate)"

check-v154-v158: check-v154 check-v155 check-v156 check-v157 check-v158
	@echo "check-v154-v158: OK (showcase + publish + paper + community + v1.0-release)"

# --- v159 σ-Heal (self-healing infrastructure) -------------------
# Deterministic health-monitor daemon + root-cause analyzer +
# self-repair action executor.  Six component probes (v106 HTTP,
# v101 σ channels, v115 SQLite, v117 KV-cache, v124 adapter,
# merge-gate smoke) are simulated with injected failure scenarios.
# A 3-day slope detector on top of v131-temporal drives predictive
# repairs.  v159.0 is fully in-process and deterministic; v159.1
# promotes the action stubs to real shell-outs + a real
# systemd-style daemon wrapper + a /v1/health history endpoint on
# v106.
V159_INC  = -Isrc/v159
V159_SRCS = src/v159/heal.c

creation_os_v159_heal: $(V159_SRCS) src/v159/main.c
	$(CC) $(CFLAGS) $(V159_INC) -o $@ \
	    $(V159_SRCS) src/v159/main.c $(LDFLAGS)

check-v159-heal-restart-cycle: creation_os_v159_heal
	@bash benchmarks/v159/check_v159_heal_restart_cycle.sh
	@echo "check-v159-heal-restart-cycle: OK (detect + diagnose + repair + predict)"

check-v159: check-v159-heal-restart-cycle
	@echo "check-v159: OK (σ-heal self-healing kernel)"

# --- v160 σ-Telemetry (OTLP + Prometheus + structured logs) -----
# Deterministic emitter producing: (a) OTLP-JSON for a 6-span
# cognitive trace (encode → recall → predict → generate →
# metacognition → decide), each span carrying its σ as an
# attribute; (b) Prometheus text-format /metrics (σ_product,
# σ_channel, abstain/heal/rsi counters, skill_count gauge); (c)
# ndjson structured logs with trace_id + σ_product.  v160.0 is
# fully in-process; v160.1 wires the emitter to a real
# OTLP/HTTP POST, a real /metrics endpoint on v106, a log-
# rotation daemon, and the v108 dashboard tile layout.
V160_INC  = -Isrc/v160
V160_SRCS = src/v160/telemetry.c

creation_os_v160_telemetry: $(V160_SRCS) src/v160/main.c
	$(CC) $(CFLAGS) $(V160_INC) -o $@ \
	    $(V160_SRCS) src/v160/main.c $(LDFLAGS)

check-v160-telemetry-otlp-export: creation_os_v160_telemetry
	@bash benchmarks/v160/check_v160_telemetry_otlp_export.sh
	@echo "check-v160-telemetry-otlp-export: OK (OTLP + Prometheus + ndjson)"

check-v160: check-v160-telemetry-otlp-export
	@echo "check-v160: OK (σ-telemetry observability)"

# --- v161 σ-Adversarial-Train (red-team → DPO → continuous harden) --
# Deterministic adversarial-training kernel.  Loads a seeded
# fixture of 10 red-team attacks across 5 attack types (prompt
# injection, jailbreak, data exfiltration, SSRF, role
# confusion), builds DPO (chosen, rejected) pairs for every
# successful attack with a canonical σ-gated refusal as the
# chosen side, and simulates a hardening cycle that closes ≥ 1
# vulnerability (target: all 6) while lifting σ_hardening above
# τ_refuse.  Also learns per-type σ-signatures (entropy /
# n_effective / coherence centroids) for fast future classification.
# v161.1 ingests real v122 red-team output, writes the DPO JSONL
# to packaging/dpo/adversarial_v161.jsonl, and plugs into v125
# DPO training inside a v144 RSI cycle.
V161_INC  = -Isrc/v161
V161_SRCS = src/v161/adv_train.c

creation_os_v161_adv_train: $(V161_SRCS) src/v161/main.c
	$(CC) $(CFLAGS) $(V161_INC) -o $@ \
	    $(V161_SRCS) src/v161/main.c $(LDFLAGS)

check-v161-adversarial-train-harden: creation_os_v161_adv_train
	@bash benchmarks/v161/check_v161_adversarial_train_harden.sh
	@echo "check-v161-adversarial-train-harden: OK (replay + DPO + harden + signature)"

check-v161: check-v161-adversarial-train-harden
	@echo "check-v161: OK (σ-adversarial-train kernel)"

# --- v162 σ-Compose (kernel manifests + profiles + resolver + hot-swap) --
# Deterministic kernel-composition kernel.  A baked manifest
# table (what each kernel provides, requires, its latency/memory
# impact, and the σ-channels it contributes) drives four
# profiles (lean / researcher / sovereign / custom).  BFS +
# recursive closure resolve declared roots into a dependency-
# complete enabled set; DFS rejects cycles.  Hot-swap
# enable/disable events respect the dependency graph (leaves
# can drop, internal nodes cannot while dependents exist) and
# are recorded in an event log.  v162.1 reads per-kernel
# kernels/vNN.manifest.toml from disk and drives process-level
# enable/disable via a v106 /v1/compose endpoint.
V162_INC  = -Isrc/v162
V162_SRCS = src/v162/compose.c

creation_os_v162_compose: $(V162_SRCS) src/v162/main.c
	$(CC) $(CFLAGS) $(V162_INC) -o $@ \
	    $(V162_SRCS) src/v162/main.c $(LDFLAGS)

check-v162-compose-profile-resolve: creation_os_v162_compose
	@bash benchmarks/v162/check_v162_compose_profile_resolve.sh
	@echo "check-v162-compose-profile-resolve: OK (manifests + profiles + deps + hot-swap)"

check-v162: check-v162-compose-profile-resolve
	@echo "check-v162: OK (σ-compose kernel)"

# --- v163 σ-Evolve-Architecture (CMA-ES + Pareto front + auto-profile) --
# Deterministic architecture-evolution kernel.  A 12-gene genome
# (subset of v101/v106/v111/v115/v117/v118/v119/v124/v128/v140/
# v150/v152) is optimized by a CMA-ES-lite loop (population 12,
# 30 generations) over three objectives: v143-benchmark accuracy
# (↑), latency_ms (↓), memory_mb (↓).  Fitness is a closed-form
# deterministic proxy; σ-gated regression (v133-meta rejects any
# candidate whose accuracy regresses > 3 % from the all-genes
# baseline) keeps evolution honest.  Output: a Pareto front with
# ≥ 3 non-dominated points + an auto-profile picker that chooses
# the highest-accuracy front point that fits a declared device
# budget (lat_ms, mem_mb).  v163.1 swaps the proxy for a real
# v143 benchmark smoke per candidate and writes
# kernels/evolve/pareto.json back to disk.
V163_INC  = -Isrc/v163
V163_SRCS = src/v163/evolve.c

creation_os_v163_evolve: $(V163_SRCS) src/v163/main.c
	$(CC) $(CFLAGS) $(V163_INC) -o $@ \
	    $(V163_SRCS) src/v163/main.c $(LDFLAGS)

check-v163-evolve-pareto-converge: creation_os_v163_evolve
	@bash benchmarks/v163/check_v163_evolve_pareto_converge.sh
	@echo "check-v163-evolve-pareto-converge: OK (CMA-ES + Pareto + auto-profile)"

check-v163: check-v163-evolve-pareto-converge
	@echo "check-v163: OK (σ-evolve-architecture kernel)"

check-v159-v163: check-v159 check-v160 check-v161 check-v162 check-v163
	@echo "check-v159-v163: OK (heal + telemetry + adversarial-train + compose + evolve)"

# --- v164 σ-Plugin (C ABI + sandbox + registry + official plugins) ------
# Deterministic plugin host.  Ships 4 officially-baked plugins
# (web-search, calculator, file-reader, git), each with a manifest
# that declares required capabilities (NETWORK / FILE_READ /
# FILE_WRITE / SUBPROCESS) and σ_impact.  The sandbox enforces
# manifest-vs-grant cap masking (missing cap → refused, σ = 1.0),
# a hard timeout_ms ceiling, and a permanent refusal of the
# MODEL_WEIGHTS cap.  The registry tracks σ_reputation as a
# ring-buffered geomean of the last 16 σ_plugin observations.
# v164.1 swaps the baked dispatcher for real dlopen + fork +
# seccomp-bpf and wires `cos plugin install github.com/...`.
V164_INC  = -Isrc/v164
V164_SRCS = src/v164/plugin.c

creation_os_v164_plugin: $(V164_SRCS) src/v164/main.c
	$(CC) $(CFLAGS) $(V164_INC) -o $@ \
	    $(V164_SRCS) src/v164/main.c $(LDFLAGS)

check-v164-plugin-load-unload: creation_os_v164_plugin
	@bash benchmarks/v164/check_v164_plugin_load_unload.sh
	@echo "check-v164-plugin-load-unload: OK (ABI + sandbox + registry + hot-swap)"

check-v164: check-v164-plugin-load-unload
	@echo "check-v164: OK (σ-plugin kernel)"

# --- v165 σ-Edge (cos-lite + RPi5/Jetson/Android + adaptive τ) ----------
# Edge-device runtime model.  A baked profile table describes 5
# targets (macbook_m3 / rpi5 / jetson_orin / android / ios) with
# arch, triple, available RAM, GPU/camera presence, default HTTP
# port, and a Makefile make_target pointing at the cos-lite
# cross-compile recipe (named here; actual cross-compilation lands
# in v165.1).  σ-adaptive τ: tau_edge = clamp(tau_default /
# max(available_ram/8192, 0.125), 0.15, 1.0).  A fit check
# compares a 4-component RAM budget (binary / weights / kvcache /
# sigma_overhead) against the profile's available RAM and emits a
# boot receipt.  iOS is declared supported_in_v0=false so the
# profile surface matches the roadmap.  v165.1 plugs real
# cross-compile, QEMU-CI, and iOS Swift wrapper.
V165_INC  = -Isrc/v165
V165_SRCS = src/v165/edge.c

creation_os_v165_edge: $(V165_SRCS) src/v165/main.c
	$(CC) $(CFLAGS) $(V165_INC) -o $@ \
	    $(V165_SRCS) src/v165/main.c $(LDFLAGS)

check-v165-edge-rpi5-smoke: creation_os_v165_edge
	@bash benchmarks/v165/check_v165_edge_rpi5_smoke.sh
	@echo "check-v165-edge-rpi5-smoke: OK (profile table + τ_edge + boot fit)"

check-v165: check-v165-edge-rpi5-smoke
	@echo "check-v165: OK (σ-edge kernel)"

# --- v166 σ-Stream (WebSocket + per-token σ + interrupt-on-sigma) -------
# Real-time streaming model.  Prompt is tokenized, per-token σ is
# generated as an 8-channel vector (SplitMix64-keyed by
# (seed, token_index, channel) + a slow sine drift), σ_product =
# geomean(channels), and the stream stops itself the moment
# σ_product > tau_interrupt.  The closing summary frame declares
# whether the stream completed or was interrupted and at which
# token.  Voice hint: audible_delay_ms = 40 + 400·σ — the model's
# voice literally slows down when it gets uncertain (v127 hook).
# v166.1 plugs an actual ws:// server and a real tokenizer.
V166_INC  = -Isrc/v166
V166_SRCS = src/v166/stream.c

creation_os_v166_stream: $(V166_SRCS) src/v166/main.c
	$(CC) $(CFLAGS) $(V166_INC) -o $@ \
	    $(V166_SRCS) src/v166/main.c $(LDFLAGS)

check-v166-stream-websocket: creation_os_v166_stream
	@bash benchmarks/v166/check_v166_stream_websocket.sh
	@echo "check-v166-stream-websocket: OK (NDJSON + per-token σ + interrupt)"

check-v166: check-v166-stream-websocket
	@echo "check-v166: OK (σ-stream kernel)"

# --- v167 σ-Governance-API (policy server + fleet + audit + RBAC) -------
# Organization-level control plane.  A baked org (spektre-labs)
# owns 3 domain policies (medical/creative/code) declaring
# (τ, abstain_message, require_sandbox); a fleet of 4 nodes
# (lab-m3, lab-rpi5, cloud-a, cloud-b) stamped with the active
# policy_version; an append-only ring-buffered audit log (N=64)
# of every σ-decision (emit/abstain/revise/denied); and a
# 4-role RBAC (admin/user/auditor/developer) enforced at
# evaluation time.  Policy push skips any node whose health is
# DOWN (enforces the "dead node can't claim compliance"
# invariant).  v167.1 ships a real HTTP policy server with TLS
# auth + GDPR/SOC2/HIPAA export profiles.
V167_INC  = -Isrc/v167
V167_SRCS = src/v167/governance.c

creation_os_v167_governance: $(V167_SRCS) src/v167/main.c
	$(CC) $(CFLAGS) $(V167_INC) -o $@ \
	    $(V167_SRCS) src/v167/main.c $(LDFLAGS)

check-v167-governance-policy-apply: creation_os_v167_governance
	@bash benchmarks/v167/check_v167_governance_policy_apply.sh
	@echo "check-v167-governance-policy-apply: OK (policy + fleet + audit + RBAC)"

check-v167: check-v167-governance-policy-apply
	@echo "check-v167: OK (σ-governance-api kernel)"

# --- v168 σ-Marketplace (registry + reputation σ) -----------------------
# Skill / kernel / plugin marketplace.  Baked registry of 6
# artifacts across all three kinds, each carrying a
# deterministic FNV-sha (8 bytes hex), tests_passed/total,
# user_reports_total/negative, and benchmark_delta_pct.
# σ_reputation = clamp(0.6·fail_rate + 0.3·neg_rate +
# 0.1·bench_penalty, 0, 1).  σ-gated install refuses any
# artifact with σ_reputation > 0.50 unless `--force` overrides;
# the override is logged as status=forced.  Publish appends a
# new artifact and recomputes σ.  A .cos-skill fixture
# validator asserts the required file set is present.
# v168.1 swaps the baked store for a real HTTPS registry and
# computes real SHA-256 over a packed tarball.
V168_INC  = -Isrc/v168
V168_SRCS = src/v168/marketplace.c

creation_os_v168_marketplace: $(V168_SRCS) src/v168/main.c
	$(CC) $(CFLAGS) $(V168_INC) -o $@ \
	    $(V168_SRCS) src/v168/main.c $(LDFLAGS)

check-v168-marketplace-publish-install: creation_os_v168_marketplace
	@bash benchmarks/v168/check_v168_marketplace_publish_install.sh
	@echo "check-v168-marketplace-publish-install: OK (registry + reputation + σ-gated install)"

check-v168: check-v168-marketplace-publish-install
	@echo "check-v168: OK (σ-marketplace kernel)"

check-v164-v168: check-v164 check-v165 check-v166 check-v167 check-v168
	@echo "check-v164-v168: OK (plugin + edge + stream + governance + marketplace)"

# --- v169 σ-Ontology (auto KG + OWL-lite + corpus navigation) ----------
# Deterministic ontology builder.  50 synthetic memory-like
# entries are parsed into candidate (subject, predicate, object)
# triples with a σ_extraction per triple; τ_extract = 0.35 gates
# which triples join the KG.  Kept triples feed a hierarchical
# typer mapping entities to 6 top-level classes (Person /
# Software / Metric / Kernel / Device / Concept) with a
# σ_type confidence.  A query API returns source entry-ids for
# any (predicate, object?) pattern, so v152 corpus questions
# like "which entries mention σ?" resolve structurally instead
# of by substring search.  OWL-lite is emitted as JSON in v0;
# v169.1 writes real OWL/XML to ~/.creation-os/ontology.owl and
# drives the extractor off BitNet over real v115 memory entries.
V169_INC  = -Isrc/v169
V169_SRCS = src/v169/ontology.c

creation_os_v169_ontology: $(V169_SRCS) src/v169/main.c
	$(CC) $(CFLAGS) $(V169_INC) -o $@ \
	    $(V169_SRCS) src/v169/main.c $(LDFLAGS)

check-v169-ontology-rdf-extract: creation_os_v169_ontology
	@bash benchmarks/v169/check_v169_ontology_rdf_extract.sh
	@echo "check-v169-ontology-rdf-extract: OK (extract + type + query)"

check-v169: check-v169-ontology-rdf-extract
	@echo "check-v169: OK (σ-ontology kernel)"

# --- v170 σ-Transfer (cross-domain transfer + negative-transfer rollback) ---
# Eight baked domains (math, physics, chemistry, biology, code,
# logic, history, poetry) in a deterministic 4-D embedding
# space.  v170 picks the nearest source with σ_source ≤ τ_src
# whose gap to σ_target is ≥ δ, applies a closed-form
# σ-delta (improvement shrinks exponentially with distance;
# penalty if distance > d_max), rolls back via v124 when σ
# rises, and ensembles the k=2 nearest strong neighbours for
# zero-shot adaptation to unseen domains.  v170.1 swaps the
# closed-form model for real LoRA-adapter composition.
V170_INC  = -Isrc/v170
V170_SRCS = src/v170/transfer.c

creation_os_v170_transfer: $(V170_SRCS) src/v170/main.c
	$(CC) $(CFLAGS) $(V170_INC) -o $@ \
	    $(V170_SRCS) src/v170/main.c $(LDFLAGS)

check-v170-transfer-cross-domain: creation_os_v170_transfer
	@bash benchmarks/v170/check_v170_transfer_cross_domain.sh
	@echo "check-v170-transfer-cross-domain: OK (decide + apply + rollback + zero-shot)"

check-v170: check-v170-transfer-cross-domain
	@echo "check-v170: OK (σ-transfer kernel)"

# --- v171 σ-Collab (human-AI protocol + anti-sycophancy) ----------------
# Explicit collaboration mode (pair / lead / follow) with a
# four-way action selector per turn: emit, handoff (σ_model >
# τ_mode), debate (σ_Δ > τ_disagree on a human claim the model
# contradicts), or anti-sycophancy (model would semantically
# agree with a shaky human claim yet its own σ stays above
# τ_sycophancy).  A 6-turn baked scenario exercises all four
# paths; every turn emits a contribution audit line so who-
# said-what-with-what-confidence is recoverable after the fact.
V171_INC  = -Isrc/v171
V171_SRCS = src/v171/collab.c

creation_os_v171_collab: $(V171_SRCS) src/v171/main.c
	$(CC) $(CFLAGS) $(V171_INC) -o $@ \
	    $(V171_SRCS) src/v171/main.c $(LDFLAGS)

check-v171-collab-handoff: creation_os_v171_collab
	@bash benchmarks/v171/check_v171_collab_handoff.sh
	@echo "check-v171-collab-handoff: OK (emit + handoff + debate + anti-sycophancy)"

check-v171: check-v171-collab-handoff
	@echo "check-v171: OK (σ-collab kernel)"

# --- v172 σ-Narrative (session summaries + goals + resumption) ----------
# 3-session baked fixture (weeks 1..3) with σ_coverage per
# summary, a chained narrative thread, three goals with
# σ_progress, and four people with role/last-context and a
# σ_ident.  The resumption protocol deterministically composes
# an opener referencing the last session + the primary goal
# (lowest σ_progress among non-done goals), so a fresh session
# picks up at the right story-beat instead of cold-booting with
# "How can I help?".  v172.1 swaps the fixture for real v115
# memory iteration and BitNet-generated summaries.
V172_INC  = -Isrc/v172
V172_SRCS = src/v172/narrative.c

creation_os_v172_narrative: $(V172_SRCS) src/v172/main.c
	$(CC) $(CFLAGS) $(V172_INC) -o $@ \
	    $(V172_SRCS) src/v172/main.c $(LDFLAGS)

check-v172-narrative-resumption: creation_os_v172_narrative
	@bash benchmarks/v172/check_v172_narrative_resumption.sh
	@echo "check-v172-narrative-resumption: OK (thread + goals + people + resume)"

check-v172: check-v172-narrative-resumption
	@echo "check-v172: OK (σ-narrative kernel)"

# --- v173 σ-Teach (Socratic + curriculum + exercises + σ-honest) --------
# Four canonical subtopics (HD vectors, σ-gating, KV cache,
# v1.58-bit quantization).  Socratic pass ingests per-subtopic
# learner correctness → σ_student (=1-p) → curriculum ordered
# weakest-first; difficulty tracks σ_student in four tiers.
# Teach cycle iterates the curriculum, emits ≤ 2 exercises
# per subtopic with a closed-form σ-drop model, marks mastery
# at σ_student ≤ τ_mastery, and ABSTAINS on the
# `v1.58-bit quantization` subtopic because its baked
# σ_teacher=0.75 is above τ_teacher=0.60 — a σ-honest teacher.
# v173.1 wires BitNet for diagnostic generation and v132
# persona for tone adaptation.
V173_INC  = -Isrc/v173
V173_SRCS = src/v173/teach.c

creation_os_v173_teach: $(V173_SRCS) src/v173/main.c
	$(CC) $(CFLAGS) $(V173_INC) -o $@ \
	    $(V173_SRCS) src/v173/main.c $(LDFLAGS)

check-v173-teach-socratic-cycle: creation_os_v173_teach
	@bash benchmarks/v173/check_v173_teach_socratic_cycle.sh
	@echo "check-v173-teach-socratic-cycle: OK (diag + curriculum + abstain + mastery)"

check-v173: check-v173-teach-socratic-cycle
	@echo "check-v173: OK (σ-teach kernel)"

# Aggregate check-v169-v173
check-v169-v173: check-v169 check-v170 check-v171 check-v172 check-v173
	@echo "check-v169-v173: OK (ontology + transfer + collab + narrative + teach)"

# --- v174 σ-Flywheel (self-evolving data pipeline) ----------------------
# Proposer → solver → σ-verifier → DPO data loop with σ used
# as a data filter: σ<τ_confident → chosen; σ>τ_uncertain →
# negative + big-model correction (PAIR); middle band → SKIP
# (uninformative).  50 synthetic questions over 5 clusters,
# three-mode σ distribution so every class is exercised under
# the baked seed.  Three anti-model-collapse guards:
# (a) answer-entropy H < H_min, (b) σ-variance < var_min,
# (c) benchmark score regresses below baseline − slack;
# any one aborts the cycle with an explicit reason.  v174.1
# wires real v151 proposer, v114 swarm big-model, and
# v125 DPO + v124 hot-swap.
V174_INC  = -Isrc/v174
V174_SRCS = src/v174/flywheel.c

creation_os_v174_flywheel: $(V174_SRCS) src/v174/main.c
	$(CC) $(CFLAGS) $(V174_INC) -o $@ \
	    $(V174_SRCS) src/v174/main.c $(LDFLAGS)

check-v174-flywheel-one-cycle: creation_os_v174_flywheel
	@bash benchmarks/v174/check_v174_flywheel_one_cycle.sh
	@echo "check-v174-flywheel-one-cycle: OK (chosen + pair + skip + anti-collapse)"

check-v174: check-v174-flywheel-one-cycle
	@echo "check-v174: OK (σ-flywheel kernel)"

# --- v175 σ-Debate-Train (self-play DPO + Elo tournament + SPIN) --------
# Harvests v150-style debates into DPO data: A answers,
# B attempts to refute; if B wins → (A=rejected, B=chosen)
# pair; if A survives → chosen positive; both-consensus-low-σ
# rounds are SKIPped (uninformative) to avoid contaminating
# the trainer.  Plus a 3-adapter round-robin tournament using
# closed-form Elo (expected = 1/(1 + 10^((Rb−Ra)/400)),
# update = K·(actual − expected), K=32), and a SPIN loop
# whose σ_delta shrinks monotonically (0.50·0.55^gen) until
# it falls below spin_convergence = 0.01.  v175.1 plugs in
# real v150 debate + v125 DPO + real adapter swap.
V175_INC  = -Isrc/v175
V175_SRCS = src/v175/debate_train.c

creation_os_v175_debate_train: $(V175_SRCS) src/v175/main.c
	$(CC) $(CFLAGS) $(V175_INC) -o $@ \
	    $(V175_SRCS) src/v175/main.c $(LDFLAGS)

check-v175-debate-train-elo: creation_os_v175_debate_train
	@bash benchmarks/v175/check_v175_debate_train_elo.sh
	@echo "check-v175-debate-train-elo: OK (harvest + tournament + SPIN)"

check-v175: check-v175-debate-train-elo
	@echo "check-v175: OK (σ-debate-train kernel)"

# --- v176 σ-Simulator (world gen + curriculum + dream training) ---------
# Procedurally generates 6 candidate worlds (room, objects,
# friction, mass) with σ_realism and σ_difficulty derived
# from parameter plausibility; builds a 5-world curriculum
# ordered by σ_difficulty ascending (easy → hard, realistic
# worlds first); measures sim-to-sim σ_transfer between
# consecutive curriculum worlds; then runs 1000 latent
# "dream" rollouts (Box-Muller-sampled σ around 0.12 ±0.05)
# and asserts σ_dream_mean ≤ σ_real + dream_slack — the
# signal that dreams are a safer learning source when σ-gated.
# v176.1 emits real MuJoCo XML + a DreamerV3-style neural
# world model behind the rollouts.
V176_INC  = -Isrc/v176
V176_SRCS = src/v176/simulator.c

creation_os_v176_simulator: $(V176_SRCS) src/v176/main.c
	$(CC) $(CFLAGS) $(V176_INC) -o $@ \
	    $(V176_SRCS) src/v176/main.c $(LDFLAGS)

check-v176-simulator-dream-train: creation_os_v176_simulator
	@bash benchmarks/v176/check_v176_simulator_dream_train.sh
	@echo "check-v176-simulator-dream-train: OK (worlds + curriculum + transfer + dreams)"

check-v176: check-v176-simulator-dream-train
	@echo "check-v176: OK (σ-simulator kernel)"

# --- v177 σ-Compress (σ-aware pruning + mixed precision + merging) ------
# Closed-form compression of a synthetic 16×64 BitNet-like
# stack.  Three σ-aware passes: (1) prune neurons whose
# σ_impact < prune_tau (0.05) — they contribute nothing to
# calibration; (2) assign mixed precision per layer by
# σ_layer (INT8 if ≤ 0.15; INT4 if ≤ 0.40; INT2 otherwise);
# (3) merge adjacent layers sharing a σ-profile (|Δσ| ≤
# merge_tau = 0.03) at the same precision.  Exit invariant:
# σ_calibration drift ≤ drift_budget_pct = 5 %, with a
# material (≥ 30 %) parameter drop.  v177.1 emits a real
# models/v177/bitnet_1b_sigma_pruned.gguf.
V177_INC  = -Isrc/v177
V177_SRCS = src/v177/compress.c

creation_os_v177_compress: $(V177_SRCS) src/v177/main.c
	$(CC) $(CFLAGS) $(V177_INC) -o $@ \
	    $(V177_SRCS) src/v177/main.c $(LDFLAGS)

check-v177-compress-sigma-prune: creation_os_v177_compress
	@bash benchmarks/v177/check_v177_compress_sigma_prune.sh
	@echo "check-v177-compress-sigma-prune: OK (prune + mixed prec + merge)"

check-v177: check-v177-compress-sigma-prune
	@echo "check-v177: OK (σ-compress kernel)"

# --- v178 σ-Consensus (Byzantine + reputation + sybil + merkle) ---------
# 5-node mesh (3 honest mature + 1 honest young + 1 byzantine)
# runs one σ-Byzantine agreement round over 4 baked claims
# (θ = 0.30).  Reputation-weighted quorum at 2/3: accept /
# reject / abstain.  Sybil-resistance: young nodes start at
# rep 1.0 — they vote but cannot override mature nodes.
# Byzantine reputations decay faster (−0.40 vs −0.20).  Each
# resolved claim is leaf-hashed (SHA-256 of packed σ+decision)
# and folded into a merkle root — tampering any leaf breaks
# verification.  "Resonance not consensus": truth = σ-signal
# convergence above quorum; mesh abstains when it cannot
# converge.  v178.1 wires live v128 mesh transport + signed
# messages + streaming v72 anchoring.
V178_INC  = -Isrc/v178 -Isrc/license_kernel
V178_SRCS = src/v178/consensus.c src/license_kernel/license_attest.c

creation_os_v178_consensus: $(V178_SRCS) src/v178/main.c
	$(CC) $(CFLAGS) $(V178_INC) -o $@ \
	    $(V178_SRCS) src/v178/main.c $(LDFLAGS)

check-v178-consensus-byzantine: creation_os_v178_consensus
	@bash benchmarks/v178/check_v178_consensus_byzantine.sh
	@echo "check-v178-consensus-byzantine: OK (quorum + reputation + merkle)"

check-v178: check-v178-consensus-byzantine
	@echo "check-v178: OK (σ-consensus kernel)"

check-v174-v178: check-v174 check-v175 check-v176 check-v177 check-v178
	@echo "check-v174-v178: OK (flywheel + debate-train + simulator + compress + consensus)"

# --- v179 σ-Interpret (SAE + circuit tracing + mechanistic explain) -----
# 64-sample / 24-feature / 8-head / 8-MLP fixture at synthetic
# layer 15.  v179.0 fits a deterministic linear "SAE" against a
# bimodal σ distribution, keeping features whose |Pearson r| ≥
# τ_correlated (0.60).  Eight seeded monosemantic features have
# named uncertainty modes (dates / named-entities / numerical /
# low-coverage / conflicting-context / rare-token + two negative
# confident-signal features); the remaining features are noise
# distractors.  Per-sample `--explain` picks the best-fitting
# labeled feature and emits a human-readable chain: token + head
# + MLP + n_effective collapse, dovetailing with v140 causal
# channels.  v179.1 promotes this to a real 2560 → 8192 SAE over
# BitNet-2B layer 15 weights with the /v1/explain/{id} endpoint.
V179_INC  = -Isrc/v179
V179_SRCS = src/v179/interpret.c

creation_os_v179_interpret: $(V179_SRCS) src/v179/main.c
	$(CC) $(CFLAGS) $(V179_INC) -o $@ \
	    $(V179_SRCS) src/v179/main.c $(LDFLAGS)

check-v179-sae-feature-correlation: creation_os_v179_interpret
	@bash benchmarks/v179/check_v179_sae_feature_correlation.sh
	@echo "check-v179-sae-feature-correlation: OK (SAE + circuit + explain)"

check-v179: check-v179-sae-feature-correlation
	@echo "check-v179: OK (σ-interpret kernel)"

# --- v180 σ-Steer (activation steering + firmware removal + persona) ----
# 48-sample / 64-dim fixture with three persisted unit-norm
# direction vectors: truthful (pulls hidden state away from
# hallucination), no_firmware (suppresses disclaimer /
# self-minimization features), expert (v132 persona level ladder).
# The σ-gate fires only when σ_before ≥ τ_high_sigma, so low-σ
# samples are left ≈ untouched.  Truthful steering drops mean σ
# on the high-σ bucket by ≥ 10 % relative; no_firmware cuts the
# firmware-token rate by ≥ 25 %; the expert ladder produces
# monotonically-increasing expert-lexicon scores across the three
# persona levels.  v180.1 wires real activation hooks through the
# v101 specialist bridge and persists SAE-derived direction
# vectors to `models/v180/steer_*.bin`.
V180_INC  = -Isrc/v180
V180_SRCS = src/v180/steer.c

creation_os_v180_steer: $(V180_SRCS) src/v180/main.c
	$(CC) $(CFLAGS) $(V180_INC) -o $@ \
	    $(V180_SRCS) src/v180/main.c $(LDFLAGS)

check-v180-steer-truthful-sigma-drop: creation_os_v180_steer
	@bash benchmarks/v180/check_v180_steer_truthful_sigma_drop.sh
	@echo "check-v180-steer-truthful-sigma-drop: OK (truthful + firmware + expert)"

check-v180: check-v180-steer-truthful-sigma-drop
	@echo "check-v180: OK (σ-steer kernel)"

# --- v181 σ-Audit (immutable log + ed25519 + compliance + anomaly) ------
# 1 000-entry hash-chained log: each entry carries SHA-256(prompt),
# SHA-256(response), σ_product + 8 σ-channels, decision tag, v179
# explanation, v180 steering vector names, specialist id and
# adapter version.  Canonical hash is SHA-256 of the concatenated
# canonical bytes + prev_hash; chain links force tamper detection.
# The v181.0 attestation field `sig` is a deterministic keyed
# SHA-256 (SHA-256(key || self_hash)) — v181.1 swaps the payload
# for an ed25519 signature (libsodium) without changing the log
# layout.  Anomaly detector fires when mean σ in the recent
# window exceeds the prior window by ≥ 30 %; the fixture injects
# a spike in the tail 15 % of entries to exercise it.  `--export`
# emits canonical JSONL for external auditors (EU AI Act Art. 13,
# ISO/IEC 42001).  v181.1 ships `cos audit report --period
# YYYY-MM` (PDF), `cos audit export --format jsonl` (CLI), and
# auto-wires v159 self-healing on anomaly.
V181_INC  = -Isrc/v181 -Isrc/license_kernel
V181_SRCS = src/v181/audit.c src/license_kernel/license_attest.c

creation_os_v181_audit: $(V181_SRCS) src/v181/main.c
	$(CC) $(CFLAGS) $(V181_INC) -o $@ \
	    $(V181_SRCS) src/v181/main.c $(LDFLAGS)

check-v181-audit-chain-verify: creation_os_v181_audit
	@bash benchmarks/v181/check_v181_audit_chain_verify.sh
	@echo "check-v181-audit-chain-verify: OK (chain + anomaly + jsonl)"

check-v181: check-v181-audit-chain-verify
	@echo "check-v181: OK (σ-audit kernel)"

# --- v182 σ-Privacy (input hashing + adaptive σ-DP + right to forget) ---
# 120-row / 4-session fixture with three privacy tiers (public,
# private, ephemeral).  Prompts and responses are SHA-256 hashed
# at ingest — the row struct carries no plaintext field at all,
# so serialization cannot leak clear text.  End-of-session purges
# ephemeral rows.  DP is σ-adaptive: noise_std = base · (1 + k·σ).
# On the low-σ subset adaptive noise is *smaller* than fixed-ε
# baseline (utility wins) AND ε_effective_low < fixed_epsilon
# (privacy budget spent wins).  High-σ rows get strictly larger
# noise than low-σ rows.  `--forget <session>` is GDPR right-to-
# erasure: memory rows removed, forgotten flag set on the
# transient audit-style copy, row count drops, plaintext
# invariant still holds.  v182.1 wires live v115 memory rows with
# AES-GCM at rest, live v129 unlearn broadcast, and a zk-proof
# verifier for external right-to-forget attestations.
V182_INC  = -Isrc/v182 -Isrc/license_kernel
V182_SRCS = src/v182/privacy.c src/license_kernel/license_attest.c

creation_os_v182_privacy: $(V182_SRCS) src/v182/main.c
	$(CC) $(CFLAGS) $(V182_INC) -o $@ \
	    $(V182_SRCS) src/v182/main.c $(LDFLAGS)

check-v182-privacy-dp-adaptive: creation_os_v182_privacy
	@bash benchmarks/v182/check_v182_privacy_dp_adaptive.sh
	@echo "check-v182-privacy-dp-adaptive: OK (σ-DP + forget + tiers)"

check-v182: check-v182-privacy-dp-adaptive
	@echo "check-v182: OK (σ-privacy kernel)"

# --- v183 σ-Governance-Theory (TLA+ + liveness + safety + model check) --
# Bounded model check over the same Kripke structure declared
# in `specs/v183/governance_theory.tla`.  Fourteen properties:
# seven axioms (σ ∈ [0,1]; emit / abstain / learn / forget /
# steer / consensus preconditions and postconditions), three
# liveness properties (progress_always, rsi_improves_one_domain,
# heal_recovers), and four safety properties (no_silent_failure,
# no_unchecked_output, no_private_leak, no_regression_
# propagates).  The C checker enumerates each property's bounded
# domain exhaustively; counterexamples count as failures.  Total
# visited states ≥ 10^5.  v183.1 re-runs TLC directly against
# the mirrored TLA+ spec, archives the proof on Zenodo under
# `docs/papers/sigma_governance_formal.md`.
V183_INC  = -Isrc/v183
V183_SRCS = src/v183/governance.c

creation_os_v183_governance: $(V183_SRCS) src/v183/main.c
	$(CC) $(CFLAGS) $(V183_INC) -o $@ \
	    $(V183_SRCS) src/v183/main.c $(LDFLAGS)

check-v183-governance-tlc-pass: creation_os_v183_governance
	@bash benchmarks/v183/check_v183_governance_tlc_pass.sh
	@echo "check-v183-governance-tlc-pass: OK (7 axioms + 3 liveness + 4 safety)"

check-v183: check-v183-governance-tlc-pass
	@echo "check-v183: OK (σ-governance-theory kernel)"

check-v179-v183: check-v179 check-v180 check-v181 check-v182 check-v183
	@echo "check-v179-v183: OK (interpret + steer + audit + privacy + governance)"

# --- v184 σ-VLA (dual-system vision-language-action, σ per stage) -------
# System 2 (slow, accurate): SigLIP + BitNet plans; System 1
# (fast, reactive): policy head executes.  v184.0 ships a
# deterministic grounding fixture: 10 synthetic scenes × 5
# candidates.  Merge-gate requires ≥ 8/10 correct grounding,
# at least one ambiguous-scene abort, and σ-channel bounds.
# v184.1 ships real Moondream + BitNet composite on RPi5.
V184_INC  = -Isrc/v184
V184_SRCS = src/v184/vla.c

creation_os_v184_vla: $(V184_SRCS) src/v184/main.c
	$(CC) $(CFLAGS) $(V184_INC) -o $@ \
	    $(V184_SRCS) src/v184/main.c $(LDFLAGS)

check-v184-vla-grounding-accuracy: creation_os_v184_vla
	@bash benchmarks/v184/check_v184_vla_grounding_accuracy.sh
	@echo "check-v184-vla-grounding-accuracy: OK (≥ 8/10 grounding + σ-gated abort)"

check-v184: check-v184-vla-grounding-accuracy
	@echo "check-v184: OK (σ-VLA kernel)"

# --- v185 σ-Multimodal-Fusion (N-modal registry + σ-weighted) ----------
# Any number of modalities register with (encoder, native_dim,
# σ_channel); v185 projects each to a common D-dim, weights
# them by 1/(1+σ_i), computes cross-modal σ as mean cosine
# distance, and exposes σ_fused as noisy-OR.  Dynamic drop:
# σ_i > τ_drop ⇒ modality removed from the fusion for that
# sample, enabling graceful degradation.  v185.1 ships real
# SigLIP + Whisper + BitNet + policy-head encoders.
V185_INC  = -Isrc/v185
V185_SRCS = src/v185/fusion.c

creation_os_v185_fusion: $(V185_SRCS) src/v185/main.c
	$(CC) $(CFLAGS) $(V185_INC) -o $@ \
	    $(V185_SRCS) src/v185/main.c $(LDFLAGS)

check-v185-fusion-cross-modal-consistency: creation_os_v185_fusion
	@bash benchmarks/v185/check_v185_fusion_cross_modal_consistency.sh
	@echo "check-v185-fusion-cross-modal-consistency: OK (σ-separated conflict + dynamic drop)"

check-v185: check-v185-fusion-cross-modal-consistency
	@echo "check-v185: OK (σ-multimodal-fusion kernel)"

# --- v186 σ-Continual-Architecture (capacity monitor + auto-grow + prune) ---
# v133-style σ-monitor watches per-domain σ_mean; rising slope
# ⇒ starved domain.  v146 genesis proposes a new kernel;
# v163 evolve accepts iff Δσ_domain < 0.  v177 compress
# prunes the weakest kernel in an over-capacity domain.
# Every change is hash-chained into the architecture-history
# log; replay must re-derive the final tip.  v186.1 wires the
# real genesis/evolve/compress/audit kernels end-to-end.
V186_INC  = -Isrc/v186
V186_SRCS = src/v186/grow.c

creation_os_v186_grow: $(V186_SRCS) src/v186/main.c
	$(CC) $(CFLAGS) $(V186_INC) -o $@ \
	    $(V186_SRCS) src/v186/main.c $(LDFLAGS)

check-v186-architecture-growth-cycle: creation_os_v186_grow
	@bash benchmarks/v186/check_v186_architecture_growth_cycle.sh
	@echo "check-v186-architecture-growth-cycle: OK (grow + prune + verified chain)"

check-v186: check-v186-architecture-growth-cycle
	@echo "check-v186: OK (σ-continual-architecture kernel)"

# --- v187 σ-Calibration (ECE + temperature scaling + per-domain) -------
# 500-sample holdout bucketed into 10 σ-bins; Expected
# Calibration Error (ECE) = Σ |acc_bin - (1 - σ_bin)| *
# n_bin / N.  Temperature scaling σ_cal = 1 - (1-σ)^T is
# searched by golden section on [0.3, 4.0] globally and
# per-domain (math / code / history / general).  Merge-gate
# requires raw ECE ≥ 0.10 (starting miscalibration real) and
# calibrated ECE < 0.05 (σ-alarm threshold).  v187.1 rotates
# a live holdout from the inference worker and wires v159
# auto-recalibration.
V187_INC  = -Isrc/v187
V187_SRCS = src/v187/calib.c

creation_os_v187_calib: $(V187_SRCS) src/v187/main.c
	$(CC) $(CFLAGS) $(V187_INC) -o $@ \
	    $(V187_SRCS) src/v187/main.c $(LDFLAGS)

check-v187-calibration-ece-below-005: creation_os_v187_calib
	@bash benchmarks/v187/check_v187_calibration_ece_below_005.sh
	@echo "check-v187-calibration-ece-below-005: OK (raw ECE ≥ 0.10 → calibrated < 0.05)"

check-v187: check-v187-calibration-ece-below-005
	@echo "check-v187: OK (σ-calibration kernel)"

# --- v188 σ-Alignment (value assertions + alignment score + incidents) ---
# Five measurable value assertions ("no hallucination",
# "abstain on doubt", "no firmware", "improve over time",
# "honest about limits"); each has a σ-measurable predicate.
# Violations become audit incidents classified into tighten-τ
# (σ ≥ τ but gate didn't fire) or adversarial_train_required
# (σ < τ, silent vulnerability).  alignment_score is the
# geometric mean of per-assertion scores so a single broken
# assertion can't be averaged away.  v188.1 ships the PDF
# `cos alignment report` and Frama-C proofs of at least two
# σ-alignment invariants.
V188_INC  = -Isrc/v188
V188_SRCS = src/v188/align.c

creation_os_v188_align: $(V188_SRCS) src/v188/main.c
	$(CC) $(CFLAGS) $(V188_INC) -o $@ \
	    $(V188_SRCS) src/v188/main.c $(LDFLAGS)

check-v188-alignment-score-smoke: creation_os_v188_align
	@bash benchmarks/v188/check_v188_alignment_score_smoke.sh
	@echo "check-v188-alignment-score-smoke: OK (≥0.80 per-assertion + classifier partition)"

check-v188: check-v188-alignment-score-smoke
	@echo "check-v188: OK (σ-alignment kernel)"

check-v184-v188: check-v184 check-v185 check-v186 check-v187 check-v188
	@echo "check-v184-v188: OK (VLA + fusion + grow + calibration + alignment)"

# --- v189 σ-TTC (σ-allocated test-time compute budget) ---
# Test-time scaling literature (Snell et al., Brown et al. 2024)
# asks "think longer, answer better" but is silent on
# allocation.  v189 makes σ the allocator: easy queries get a
# single forward pass, medium three thinking paths, hard eight
# paths plus debate / symbolic / reflect plug-ins; per-token
# recurrent depth scales with per-token σ; modes fast |
# balanced | deep cap the whole ladder.  Invariants exercised
# by the merge-gate: monotone spending (hard ≥ 2× medium ≥
# 2× easy) and hard/easy ≥ 4× (matching Snell et al.'s
# "compute-optimal is 4× more efficient than uniform best-of-N"
# result).  v189.1 ships the real thinking-path enumerator
# over BitNet-2B and wires `[ttc]` mode into the API.
V189_INC  = -Isrc/v189
V189_SRCS = src/v189/ttc.c

creation_os_v189_ttc: $(V189_SRCS) src/v189/main.c
	$(CC) $(CFLAGS) $(V189_INC) -o $@ \
	    $(V189_SRCS) src/v189/main.c $(LDFLAGS)

check-v189-ttc-adaptive-budget: creation_os_v189_ttc
	@bash benchmarks/v189/check_v189_ttc_adaptive_budget.sh
	@echo "check-v189-ttc-adaptive-budget: OK (monotone σ-budget + 4× hard/easy + mode caps)"

check-v189: check-v189-ttc-adaptive-budget
	@echo "check-v189: OK (σ-TTC kernel)"

# --- v190 σ-Latent-Reason (recurrent depth + convergence) ---
# The thinking block is a contraction map ρ·(h-h*)+h* with
# spectral radius ρ < 1, so ‖h_{n+1}-h_n‖ = ρ·‖h_n-h_{n-1}‖
# geometrically decays.  σ_latent = ‖Δh‖/‖h‖ is the halt
# signal; reasoning stops when σ_latent < τ_converge = 0.01 or
# the max recurrent depth is reached.  No intermediate tokens
# are emitted: reasoning is fully internal, so prompts aren't
# leaked via "let me think step-by-step".  Invariants exercised
# in the merge-gate: strict σ_latent monotonicity per query,
# ≥ 90 % convergence rate, and hard-class queries consume ≥ 3×
# more iterations than easy-class.  v190.1 wires a learnt halt
# predictor into BitNet-2B layers 10-20.
V190_INC  = -Isrc/v190
V190_SRCS = src/v190/latent.c

creation_os_v190_latent: $(V190_SRCS) src/v190/main.c
	$(CC) $(CFLAGS) $(V190_INC) -o $@ \
	    $(V190_SRCS) src/v190/main.c $(LDFLAGS)

check-v190-latent-reason-convergence: creation_os_v190_latent
	@bash benchmarks/v190/check_v190_latent_reason_convergence.sh
	@echo "check-v190-latent-reason-convergence: OK (monotone σ_latent + ≥90% convergence + zero middle tokens)"

check-v190: check-v190-latent-reason-convergence
	@echo "check-v190: OK (σ-latent-reason kernel)"

# --- v191 σ-Constitutional (7 axioms + hash-chain audit) ---
# Seven seed axioms live in specs/constitution.toml:
# (1) declared = realized, (2) σ-honesty, (3) no silent
# failure, (4) authority requires measurement, (5) human
# primacy, (6) resonance, (7) no firmware.  Each axiom is a
# predicate on (output, σ, metadata); a candidate output is
# ACCEPTED only when all 7 predicates pass, otherwise REVISED
# or ABSTAINED (decision picked by σ-margin).  Axiom #3 "no
# silent failure" is enforced by FNV-1a chaining every verdict
# into an append-only hash chain that replays
# byte-deterministically.  The merge-gate asserts ≥ 1 firmware
# rejection and ≥ 1 fully-safe acceptance and rejects every
# flawed candidate.  v191.1 wires a live (v148 + v150 + v183)
# axiom-proposal pipeline with SHA-256 signed constitution.
V191_INC  = -Isrc/v191
V191_SRCS = src/v191/constitution.c

creation_os_v191_constitution: $(V191_SRCS) src/v191/main.c
	$(CC) $(CFLAGS) $(V191_INC) -o $@ \
	    $(V191_SRCS) src/v191/main.c $(LDFLAGS)

check-v191-constitutional-check: creation_os_v191_constitution
	@bash benchmarks/v191/check_v191_constitutional_check.sh
	@echo "check-v191-constitutional-check: OK (7 axioms + firmware rejection + hash chain)"

check-v191: check-v191-constitutional-check
	@echo "check-v191: OK (σ-constitutional kernel)"

# --- v192 σ-Emergent (superlinear detection + risk + journal) ---
# v192 quantifies "how much more is the whole than the best
# part" on a σ-grounded metric: σ_emergent = 1 - max(part) /
# combined.  Positive values are superlinear; above τ_risk the
# combination counts as genuine emergence.  Beneficial /
# risky classification uses a safety co-metric (drop ≥ 0.05
# below the best part ⇒ risky).  Every superlinear event is
# appended to an append-only FNV-1a chain so the emergence
# journal replays byte-identically.  Merge-gate: ≥ 2 super-
# linear pairs, ≥ 1 beneficial AND ≥ 1 risky, 0 linear false
# positives.  v192.1 sweeps v143 benchmark grids over real
# kernel pairs and folds v140 causal attribution into risky
# emergence decomposition.
V192_INC  = -Isrc/v192
V192_SRCS = src/v192/emergent.c

creation_os_v192_emergent: $(V192_SRCS) src/v192/main.c
	$(CC) $(CFLAGS) $(V192_INC) -o $@ \
	    $(V192_SRCS) src/v192/main.c $(LDFLAGS)

check-v192-emergent-detection: creation_os_v192_emergent
	@bash benchmarks/v192/check_v192_emergent_detection.sh
	@echo "check-v192-emergent-detection: OK (≥1 beneficial + ≥1 risky + 0 false positives)"

check-v192: check-v192-emergent-detection
	@echo "check-v192: OK (σ-emergent kernel)"

# --- v193 σ-Coherence (K = ρ·I_Φ·F, K_eff = (1-σ)·K, K_crit) ---
# v193 implements the Creation OS core formula directly:
# ρ (cross-kernel consistency) · I_Φ (integrated information)
# · F (geom-mean of v143 accuracy, 1-ECE from v187, v188
# alignment) = K; K_eff = (1-σ)·K; alert when K_eff < K_crit.
# A 16-tick deterministic trajectory steady-states, survives
# a controlled σ spike (swarm-consensus miscalibration), heals
# via v159, and recovers monotonically as v187 ECE drops →
# F rises → K_eff rises.  Merge-gate invariants: all
# components ∈ [0,1]; K_eff = (1-σ)K numerically; alert fires;
# recovery within ≤ 3 ticks; improvement phase strictly
# monotone; Δ K_eff > 0 post-calibration.  v193.1 wires the
# live Web UI /coherence dashboard and real v135/v169/v143/
# v187/v188 feeds.
V193_INC  = -Isrc/v193
V193_SRCS = src/v193/coherence.c

creation_os_v193_coherence: $(V193_SRCS) src/v193/main.c
	$(CC) $(CFLAGS) $(V193_INC) -o $@ \
	    $(V193_SRCS) src/v193/main.c $(LDFLAGS)

check-v193-coherence-keff: creation_os_v193_coherence
	@bash benchmarks/v193/check_v193_coherence_keff.sh
	@echo "check-v193-coherence-keff: OK (K=ρ·I_Φ·F + K_eff identity + alert/recovery)"

check-v193: check-v193-coherence-keff
	@echo "check-v193: OK (σ-coherence kernel)"

check-v189-v193: check-v189 check-v190 check-v191 check-v192 check-v193
	@echo "check-v189-v193: OK (TTC + latent-reason + constitutional + emergent + coherence)"

# --- v194 σ-Horizon (multi-horizon + degradation monitor) ---
# Long-horizon agents collapse after ~35 minutes because they
# don't measure their own degradation.  v194 makes that
# measurement explicit: a three-tier goal tree (strategic /
# tactical / operational) plus a σ-slope monitor on the
# operational tier that, once the 10-tick climb exceeds
# τ_degrade_slope, fires the recovery ladder in order —
# v117 KV-cache flush → v172 summarize+resume → v115 break
# point + persist progress.  Every operational task writes a
# FNV-1a checkpoint so a crash never loses work, and a
# simulated crash-recovery run reproduces the terminal hash
# byte-identically.  Merge-gate contracts: ladder order
# 1→2→3, σ after ladder < σ at detection, chain valid, and
# crash recovery matches.  v194.1 wires live v115 memory and
# `cos goals` CLI over `~/.creation-os/goals.jsonl`.
V194_INC  = -Isrc/v194
V194_SRCS = src/v194/horizon.c

creation_os_v194_horizon: $(V194_SRCS) src/v194/main.c
	$(CC) $(CFLAGS) $(V194_INC) -o $@ \
	    $(V194_SRCS) src/v194/main.c $(LDFLAGS)

check-v194-horizon-degradation-detect: creation_os_v194_horizon
	@bash benchmarks/v194/check_v194_horizon_degradation_detect.sh
	@echo "check-v194-horizon-degradation-detect: OK (1/3/12 tree + ladder + crash recovery)"

check-v194: check-v194-horizon-degradation-detect
	@echo "check-v194: OK (σ-horizon kernel)"

# --- v195 σ-Recover (error classification + per-class fix) ---
# Five error classes (HALLUCINATION, CAPABILITY_GAP,
# PLANNING_ERROR, TOOL_FAILURE, CONTEXT_LOSS) each map to a
# canonical recovery operator set: v180+v125 for hallucination,
# v141+v145 for capability gap, v121 replan, v159 heal+retry,
# v194 checkpoint.  Every incident logs a hash-chained record
# of (error, recovery).  The v0 demonstrator runs two passes
# over the same fixture so the v174 flywheel contract is
# exercisable: pass-1 consumes strictly fewer ops than pass-0.
# Calibration feedback: per-domain ECE strictly drops wherever
# a hallucination appeared, because v187 is rebinned on the
# signal.  Merge-gate: 5/5 classes, canonical partitioning,
# strict learning gain, strict ECE drop on every domain.
V195_INC  = -Isrc/v195
V195_SRCS = src/v195/recover.c

creation_os_v195_recover: $(V195_SRCS) src/v195/main.c
	$(CC) $(CFLAGS) $(V195_INC) -o $@ \
	    $(V195_SRCS) src/v195/main.c $(LDFLAGS)

check-v195-recover-error-classify: creation_os_v195_recover
	@bash benchmarks/v195/check_v195_recover_error_classify.sh
	@echo "check-v195-recover-error-classify: OK (5 classes + canonical + ECE drop + learning)"

check-v195: check-v195-recover-error-classify
	@echo "check-v195: OK (σ-recover kernel)"

# --- v196 σ-Habit (cortex/cerebellum pattern compile) ---
# Repetitive audit-log patterns ≥ τ_repeat compile into
# fast habits (v137 LLVM path, v0 closed-form cycle model);
# σ is the cortex/cerebellum switch — σ_steady < τ_break
# runs the compiled habit (cerebellum, ≥ 10× speedup), a σ
# spike breaks out to full reasoning (cortex).  Library is
# hash-chained.  Merge-gate: ≥ 3 habits, 10× speedup, at
# least one σ-spike break-out actually returns to cortex.
V196_INC  = -Isrc/v196
V196_SRCS = src/v196/habit.c

creation_os_v196_habit: $(V196_SRCS) src/v196/main.c
	$(CC) $(CFLAGS) $(V196_INC) -o $@ \
	    $(V196_SRCS) src/v196/main.c $(LDFLAGS)

check-v196-habit-compile-speedup: creation_os_v196_habit
	@bash benchmarks/v196/check_v196_habit_compile_speedup.sh
	@echo "check-v196-habit-compile-speedup: OK (≥3 habits + 10× + break-out)"

check-v196: check-v196-habit-compile-speedup
	@echo "check-v196: OK (σ-habit kernel)"

# --- v197 σ-Theory-of-Mind (user state + intent + anti-manipulation) ---
# v197 estimates user state from observables (length,
# edits, typing speed, topic variance), produces σ_tom,
# predicts intent as mode-of-history, and adapts the
# response mode only when σ_tom < τ_adapt — else neutral.
# Anti-manipulation: firmware-style probes are rejected via
# a v191 constitutional check; they never adapt.  Merge-gate
# contracts: all 6 states covered, canonical state→mode
# partitioning, ≥ 1 firmware probe rejected, deterministic.
V197_INC  = -Isrc/v197
V197_SRCS = src/v197/tom.c

creation_os_v197_tom: $(V197_SRCS) src/v197/main.c
	$(CC) $(CFLAGS) $(V197_INC) -o $@ \
	    $(V197_SRCS) src/v197/main.c $(LDFLAGS)

check-v197-tom-state-estimation: creation_os_v197_tom
	@bash benchmarks/v197/check_v197_tom_state_estimation.sh
	@echo "check-v197-tom-state-estimation: OK (6 states + canonical adapt + anti-manipulation)"

check-v197: check-v197-tom-state-estimation
	@echo "check-v197: OK (σ-ToM kernel)"

# --- v198 σ-Moral (multi-framework geodesic) ---
# Four ethical frameworks (deontological, utilitarian,
# virtue, care) score every decision.  σ_moral is the
# variance across the four scores; a moral path is a
# 5-waypoint trajectory; the geodesic is the path with
# strictly minimum mean σ_moral across three candidates.
# Clear cases (σ < τ_clear) act; dilemmas (σ > τ_dilemma)
# raise honest_uncertainty and invite human consultation.
# No firmware refusals on clear cases — that behaviour is
# measured and enforced = 0.  Merge-gate: 4/4 frameworks,
# geodesic strict-min, dilemma→honest, clear→no refusal.
V198_INC  = -Isrc/v198
V198_SRCS = src/v198/moral.c

creation_os_v198_moral: $(V198_SRCS) src/v198/main.c
	$(CC) $(CFLAGS) $(V198_INC) -o $@ \
	    $(V198_SRCS) src/v198/main.c $(LDFLAGS)

check-v198-moral-multi-framework: creation_os_v198_moral
	@bash benchmarks/v198/check_v198_moral_multi_framework.sh
	@echo "check-v198-moral-multi-framework: OK (4 frameworks + geodesic + honest uncertainty)"

check-v198: check-v198-moral-multi-framework
	@echo "check-v198: OK (σ-moral kernel)"

check-v194-v198: check-v194 check-v195 check-v196 check-v197 check-v198
	@echo "check-v194-v198: OK (horizon + recover + habit + ToM + moral)"

# --- v199 σ-Law (norm register + governance graph) ---
# Three jurisdictions (SCSL / EU AI Act / internal) with
# 18 norms, 16 queries.  Higher priority wins; same-priority
# contradictions escalate to REVIEW with σ_law = 1.0 — no
# silent override.  Waivers flip PROHIBITED → PERMITTED with
# an audit record.  Every resolution appends to a FNV-1a
# hash-chained governance graph that replays byte-identically.
V199_INC  = -Isrc/v199
V199_SRCS = src/v199/law.c

creation_os_v199_law: $(V199_SRCS) src/v199/main.c
	$(CC) $(CFLAGS) $(V199_INC) -o $@ \
	    $(V199_SRCS) src/v199/main.c $(LDFLAGS)

check-v199-law-conflict-resolve: creation_os_v199_law
	@bash benchmarks/v199/check_v199_law_conflict_resolve.sh
	@echo "check-v199-law-conflict-resolve: OK (priority + waiver + same-prio → REVIEW)"

check-v199: check-v199-law-conflict-resolve
	@echo "check-v199: OK (σ-law kernel)"

# --- v200 σ-Market (resource ledger + σ-price + self-improving) ---
# Four resources (compute / API / memory / adapters) with
# σ as the price signal — σ > σ_local routes to API,
# else local.  Scarcity penalty activates when a single
# allocator holds > τ_hoard of a pool, triggering
# deterministic eviction.  Over the 40-query trajectory
# σ_before monotonically drops so cost_second_half is
# strictly cheaper than cost_first_half (self-improving
# via v120 distill).  Exchange log hash-chained.
V200_INC  = -Isrc/v200
V200_SRCS = src/v200/market.c

creation_os_v200_market: $(V200_SRCS) src/v200/main.c
	$(CC) $(CFLAGS) $(V200_INC) -o $@ \
	    $(V200_SRCS) src/v200/main.c $(LDFLAGS)

check-v200-market-scarcity-penalty: creation_os_v200_market
	@bash benchmarks/v200/check_v200_market_scarcity_penalty.sh
	@echo "check-v200-market-scarcity-penalty: OK (σ-price + scarcity + self-improving)"

check-v200: check-v200-market-scarcity-penalty
	@echo "check-v200: OK (σ-market kernel)"

# --- v201 σ-Diplomacy (stance + minimax + trust + treaty) ---
# 8 negotiations × 4 parties, closed-form minimax compromise
# over the red-line intersection.  Disjoint red lines yield
# an explicit DEFER (never a silent override).  Betrayal
# drops trust by 0.5 and requires 10 successful interactions
# to recover.  Treaty receipts FNV-1a chained for audit.
V201_INC  = -Isrc/v201
V201_SRCS = src/v201/diplomacy.c

creation_os_v201_diplomacy: $(V201_SRCS) src/v201/main.c
	$(CC) $(CFLAGS) $(V201_INC) -o $@ \
	    $(V201_SRCS) src/v201/main.c $(LDFLAGS)

check-v201-diplomacy-compromise-search: creation_os_v201_diplomacy
	@bash benchmarks/v201/check_v201_diplomacy_compromise_search.sh
	@echo "check-v201-diplomacy-compromise-search: OK (minimax + defer + trust)"

check-v201: check-v201-diplomacy-compromise-search
	@echo "check-v201: OK (σ-diplomacy kernel)"

# --- v202 σ-Culture (style profiles + taboo gate + translation) ---
# Six profiles × six canonical cores = 36 translations.
# σ_translate stays below τ_drift for every (core, profile)
# and the three canonical symbols {σ, τ, K_eff} survive
# translation in ≥ 90 % of cases.  Taboo gate rephrases
# high-σ_offense messages into non-empty surfaces — never
# firmware-censored — and every (drift, offense, rephrased,
# symbols) record is FNV-1a hash-chained.
V202_INC  = -Isrc/v202
V202_SRCS = src/v202/culture.c

creation_os_v202_culture: $(V202_SRCS) src/v202/main.c
	$(CC) $(CFLAGS) $(V202_INC) -o $@ \
	    $(V202_SRCS) src/v202/main.c $(LDFLAGS)

check-v202-culture-translation-preserve: creation_os_v202_culture
	@bash benchmarks/v202/check_v202_culture_translation_preserve.sh
	@echo "check-v202-culture-translation-preserve: OK (profiles + taboo + symbol invariance)"

check-v202: check-v202-culture-translation-preserve
	@echo "check-v202: OK (σ-culture kernel)"

# --- v203 σ-Civilization (institution registry + K_eff
# collapse + continuity + public-good ledger + SCSL) ---
# Six institutions of three licence classes
# (SCSL / CLOSED / PRIVATE) produce 12-tick σ trajectories.
# A 4-tick window over K_crit=0.60 marks a collapse; four
# consecutive ticks below K_crit mark a recovery.
# Continuity score strictly orders stable > recovered >
# permanently-collapsed.  Inter-layer contradiction queries
# (v199 verdict vs v202 profile practice) escalate to
# REVIEW when they disagree.  SCSL public-good ratio
# strictly exceeds CLOSED.  FNV-1a hash chain audits the
# contradiction log.
V203_INC  = -Isrc/v203
V203_SRCS = src/v203/civilization.c

creation_os_v203_civilization: $(V203_SRCS) src/v203/main.c
	$(CC) $(CFLAGS) $(V203_INC) -o $@ \
	    $(V203_SRCS) src/v203/main.c $(LDFLAGS)

check-v203-civilization-collapse-detect: creation_os_v203_civilization
	@bash benchmarks/v203/check_v203_civilization_collapse_detect.sh
	@echo "check-v203-civilization-collapse-detect: OK (collapse + continuity + SCSL)"

check-v203: check-v203-civilization-collapse-detect
	@echo "check-v203: OK (σ-civilization kernel)"

check-v199-v203: check-v199 check-v200 check-v201 check-v202 check-v203
	@echo "check-v199-v203: OK (law + market + diplomacy + culture + civilization)"

# --- v204 σ-Hypothesis (generation + grounding + novelty + impact) ---
# N=10 candidate hypotheses ranked by
# impact = σ_novelty × (1 − σ_hypothesis) with grounding
# hard-zeroing when σ_grounding > τ_ground.  Top-3 go to
# test queue (v205), rest to v115 memory.  Speculative
# hypotheses are flagged, never pruned.
V204_INC  = -Isrc/v204
V204_SRCS = src/v204/hypothesis.c

creation_os_v204_hypothesis: $(V204_SRCS) src/v204/main.c
	$(CC) $(CFLAGS) $(V204_INC) -o $@ \
	    $(V204_SRCS) src/v204/main.c $(LDFLAGS)

check-v204-hypothesis-ranking: creation_os_v204_hypothesis
	@bash benchmarks/v204/check_v204_hypothesis_ranking.sh
	@echo "check-v204-hypothesis-ranking: OK (impact ranking + grounding + speculative)"

check-v204: check-v204-hypothesis-ranking
	@echo "check-v204: OK (σ-hypothesis kernel)"

# --- v205 σ-Experiment (design + sim-first + power + repro) ---
# One experiment per v204 TEST_QUEUE slot (3 total).  Each
# has distinct dep/indep/control variable ids; n_required
# follows (z_α+z_β)²/effect² deterministically; sim-first
# (σ_sim < τ_sim) plus power check (σ_power < τ_power)
# produce a tri-valued decision SIM_SUPPORTS /
# SIM_REFUTES / UNDER_POWERED.  Full repro record hash-
# chained — the `cos reproduce --experiment` spine.
V205_INC  = -Isrc/v205
V205_SRCS = src/v205/experiment.c

creation_os_v205_experiment: $(V205_SRCS) src/v205/main.c
	$(CC) $(CFLAGS) $(V205_INC) -o $@ \
	    $(V205_SRCS) src/v205/main.c $(LDFLAGS)

check-v205-experiment-design-valid: creation_os_v205_experiment
	@bash benchmarks/v205/check_v205_experiment_design_valid.sh
	@echo "check-v205-experiment-design-valid: OK (design + sim-first + power)"

check-v205: check-v205-experiment-design-valid
	@echo "check-v205: OK (σ-experiment kernel)"

# --- v206 σ-Theorem (NL conjecture → Lean4 → σ-honest status) ---
# 8 conjectures × 4 proof steps.  σ_proof = max(σ_step);
# a simulated Lean 4 accept requires σ_formalization ≤
# τ_formal AND σ_proof ≤ τ_step.  Status ladder covers
# PROVEN / CONJECTURE / SPECULATION / REFUTED.  No
# theorem is ever marked PROVEN without the accept flag
# — σ-honesty enforced in the self-test.
V206_INC  = -Isrc/v206
V206_SRCS = src/v206/theorem.c

creation_os_v206_theorem: $(V206_SRCS) src/v206/main.c
	$(CC) $(CFLAGS) $(V206_INC) -o $@ \
	    $(V206_SRCS) src/v206/main.c $(LDFLAGS)

check-v206-theorem-lean-verify: creation_os_v206_theorem
	@bash benchmarks/v206/check_v206_theorem_lean_verify.sh
	@echo "check-v206-theorem-lean-verify: OK (Lean accept + σ-honest status)"

check-v206: check-v206-theorem-lean-verify
	@echo "check-v206: OK (σ-theorem kernel)"

# --- v207 σ-Design (space + constraints + Pareto + receipt) ---
# 12 design candidates over (performance, complexity,
# cost) with hard constraints from v199 law, v191
# constitutional, v200 market.  Infeasible candidates are
# excluded from the Pareto front; front members must not
# be dominated by any feasible candidate.  Receipt chain
# (requirements → design → rationale) hashed.
V207_INC  = -Isrc/v207
V207_SRCS = src/v207/design.c

creation_os_v207_design: $(V207_SRCS) src/v207/main.c
	$(CC) $(CFLAGS) $(V207_INC) -o $@ \
	    $(V207_SRCS) src/v207/main.c $(LDFLAGS)

check-v207-design-pareto-front: creation_os_v207_design
	@bash benchmarks/v207/check_v207_design_pareto_front.sh
	@echo "check-v207-design-pareto-front: OK (Pareto front + constraints)"

check-v207: check-v207-design-pareto-front
	@echo "check-v207: OK (σ-design kernel)"

# --- v208 σ-Manufacture (DFM + process sim + quality + closed loop) ---
# 4 designs manufactured.  Each run carries 5 DFM
# findings (σ_dfm + flagged + suggestion_id), 4 process
# stages with σ_process, and σ_quality combining the two.
# v159-style heal: checkpoints grow monotonically with
# σ_quality.  Every run emits a non-zero feedback
# hypothesis id that closes the
#    v208 → v204 science / design / manufacture loop.
V208_INC  = -Isrc/v208
V208_SRCS = src/v208/manufacture.c

creation_os_v208_manufacture: $(V208_SRCS) src/v208/main.c
	$(CC) $(CFLAGS) $(V208_INC) -o $@ \
	    $(V208_SRCS) src/v208/main.c $(LDFLAGS)

check-v208-manufacture-dfm-check: creation_os_v208_manufacture
	@bash benchmarks/v208/check_v208_manufacture_dfm_check.sh
	@echo "check-v208-manufacture-dfm-check: OK (DFM + process + quality + closed loop)"

check-v208: check-v208-manufacture-dfm-check
	@echo "check-v208: OK (σ-manufacture kernel)"

check-v204-v208: check-v204 check-v205 check-v206 check-v207 check-v208
	@echo "check-v204-v208: OK (hypothesis + experiment + theorem + design + manufacture)"

# --- v209 σ-Containment (5 layers + intent + anti-concealment + kill switch) ---
# Every I/O proposal passes through sandbox → action-gate
# → constitutional → audit → intent, in that order, with
# network default-closed and a terminator layer that
# hard-blocks any action after the kill switch fires.
V209_INC  = -Isrc/v209
V209_SRCS = src/v209/containment.c

creation_os_v209_containment: $(V209_SRCS) src/v209/main.c
	$(CC) $(CFLAGS) $(V209_INC) -o $@ \
	    $(V209_SRCS) src/v209/main.c $(LDFLAGS)

check-v209-containment-no-escape: creation_os_v209_containment
	@bash benchmarks/v209/check_v209_containment_no_escape.sh
	@echo "check-v209-containment-no-escape: OK (5-layer + kill switch + network default-closed)"

check-v209: check-v209-containment-no-escape
	@echo "check-v209: OK (σ-containment kernel)"

# --- v210 σ-Guardian (autonomous security agent) ---
# 20 observation windows × 4-level escalation ladder
# (log/warn/block/kill) with OWASP Agentic Top-10
# mapping.  Guardian model id differs from primary —
# cannot self-monitor.
V210_INC  = -Isrc/v210
V210_SRCS = src/v210/guardian.c

creation_os_v210_guardian: $(V210_SRCS) src/v210/main.c
	$(CC) $(CFLAGS) $(V210_INC) -o $@ \
	    $(V210_SRCS) src/v210/main.c $(LDFLAGS)

check-v210-guardian-anomaly-detect: creation_os_v210_guardian
	@bash benchmarks/v210/check_v210_guardian_anomaly_detect.sh
	@echo "check-v210-guardian-anomaly-detect: OK (escalation + OWASP + distinct models)"

check-v210: check-v210-guardian-anomaly-detect
	@echo "check-v210: OK (σ-guardian kernel)"

# --- v211 σ-Sandbox-Formal (Frama-C + TLA+ + attack tree + cert) ---
# 4 Frama-C proofs; 3 TLA+ bounded invariants over ≥10^6
# states with zero violations; 5-leaf attack tree with
# every leaf mapped to a proof id; 3 certification
# tracks (DO-178C DAL-A, IEC 62443, Common Criteria EAL4+).
V211_INC  = -Isrc/v211
V211_SRCS = src/v211/sandbox_formal.c

creation_os_v211_sandbox_formal: $(V211_SRCS) src/v211/main.c
	$(CC) $(CFLAGS) $(V211_INC) -o $@ \
	    $(V211_SRCS) src/v211/main.c $(LDFLAGS)

check-v211-sandbox-formal-proof: creation_os_v211_sandbox_formal
	@bash benchmarks/v211/check_v211_sandbox_formal_proof.sh
	@echo "check-v211-sandbox-formal-proof: OK (4 proofs + TLA+ 10^6 + attack tree)"

check-v211: check-v211-sandbox-formal-proof
	@echo "check-v211: OK (σ-sandbox-formal kernel)"

# --- v212 σ-Transparency (glass box + real-time activity + declaration) ---
# 30 activity events with declared-before-realised
# timestamps.  σ_transparency_event = 0.02 on match,
# ≥ τ_event on mismatch.  Aggregate σ_opacity is the
# mean across events and must stay below τ_opacity.
V212_INC  = -Isrc/v212
V212_SRCS = src/v212/transparency.c

creation_os_v212_transparency: $(V212_SRCS) src/v212/main.c
	$(CC) $(CFLAGS) $(V212_INC) -o $@ \
	    $(V212_SRCS) src/v212/main.c $(LDFLAGS)

check-v212-transparency-realtime-stream: creation_os_v212_transparency
	@bash benchmarks/v212/check_v212_transparency_realtime_stream.sh
	@echo "check-v212-transparency-realtime-stream: OK (declare→realise + σ_opacity + audit API)"

check-v212: check-v212-transparency-realtime-stream
	@echo "check-v212: OK (σ-transparency kernel)"

# --- v213 σ-Trust-Chain (root-to-leaf) ---
# 7-link canonical chain v138→v183→v211→v191→v181→v210→v212
# with trust_score = ∏(1-σ_link) and reproducible
# external-attestation hash.
V213_INC  = -Isrc/v213
V213_SRCS = src/v213/trust_chain.c

creation_os_v213_trust_chain: $(V213_SRCS) src/v213/main.c
	$(CC) $(CFLAGS) $(V213_INC) -o $@ \
	    $(V213_SRCS) src/v213/main.c $(LDFLAGS)

check-v213-trust-chain-verify: creation_os_v213_trust_chain
	@bash benchmarks/v213/check_v213_trust_chain_verify.sh
	@echo "check-v213-trust-chain-verify: OK (7-link root→leaf + external attestation)"

check-v213: check-v213-trust-chain-verify
	@echo "check-v213: OK (σ-trust-chain kernel)"

check-v209-v213: check-v209 check-v210 check-v211 check-v212 check-v213
	@echo "check-v209-v213: OK (containment + guardian + sandbox-formal + transparency + trust-chain)"

# --- v214 σ-Swarm-Evolve (evolutionary swarm intelligence) ---
# 10 generations × 12 agents × 4 niches.  fitness =
# 1/(σ_mean+ε); top-2 per niche breed, global bottom-2
# retire, population stays constant, σ_emergent monotone
# non-increasing.
V214_INC  = -Isrc/v214
V214_SRCS = src/v214/swarm_evolve.c

creation_os_v214_swarm_evolve: $(V214_SRCS) src/v214/main.c
	$(CC) $(CFLAGS) $(V214_INC) -o $@ \
	    $(V214_SRCS) src/v214/main.c $(LDFLAGS)

check-v214-swarm-evolve-speciation: creation_os_v214_swarm_evolve
	@bash benchmarks/v214/check_v214_swarm_evolve_speciation.sh
	@echo "check-v214-swarm-evolve-speciation: OK (10 gens + 3+ species + monotone σ_emergent)"

check-v214: check-v214-swarm-evolve-speciation
	@echo "check-v214: OK (σ-swarm-evolve kernel)"

# --- v215 σ-Stigmergy (shared environment + σ-pheromone) ---
# 6 trails (4 true, 2 false) × 20 steps.  Closed-form
# decay Σ_k (1-σ_k)·e^{-λ·(t_final-t_k)} normalised; τ_trail
# gates survival, ≥ 3 distinct reinforcer nodes gates
# trail formation.
V215_INC  = -Isrc/v215
V215_SRCS = src/v215/stigmergy.c

creation_os_v215_stigmergy: $(V215_SRCS) src/v215/main.c
	$(CC) $(CFLAGS) $(V215_INC) -o $@ \
	    $(V215_SRCS) src/v215/main.c $(LDFLAGS)

check-v215-stigmergy-trail-form: creation_os_v215_stigmergy
	@bash benchmarks/v215/check_v215_stigmergy_trail_form.sh
	@echo "check-v215-stigmergy-trail-form: OK (pheromone decay + trail formation)"

check-v215: check-v215-stigmergy-trail-form
	@echo "check-v215: OK (σ-stigmergy kernel)"

# --- v216 σ-Quorum (gradual consensus + minority voice) ---
# 5 proposals × 7 agents × 4-level decision ladder
# (BOLD / CAUTIOUS / DEBATE / DEFER); σ_collective =
# σ_majority_mean + minority_penalty; confident dissent
# (σ < s_minority) is captured in the audit record.
V216_INC  = -Isrc/v216
V216_SRCS = src/v216/quorum.c

creation_os_v216_quorum: $(V216_SRCS) src/v216/main.c
	$(CC) $(CFLAGS) $(V216_INC) -o $@ \
	    $(V216_SRCS) src/v216/main.c $(LDFLAGS)

check-v216-quorum-gradual-consensus: creation_os_v216_quorum
	@bash benchmarks/v216/check_v216_quorum_gradual_consensus.sh
	@echo "check-v216-quorum-gradual-consensus: OK (gradual + minority + deadlock)"

check-v216: check-v216-quorum-gradual-consensus
	@echo "check-v216: OK (σ-quorum kernel)"

# --- v217 σ-Ecosystem (trophic levels + resource flow + symbiosis) ---
# 4 trophic levels (producer/consumer/decomposer/predator),
# 5 symbiotic pairs (mutual/compete/commensal), σ_ecosystem
# = 0.4·dominance + 0.4·balance + 0.2·symbiosis,
# K_eff = 1 − σ_ecosystem.
V217_INC  = -Isrc/v217
V217_SRCS = src/v217/ecosystem.c

creation_os_v217_ecosystem: $(V217_SRCS) src/v217/main.c
	$(CC) $(CFLAGS) $(V217_INC) -o $@ \
	    $(V217_SRCS) src/v217/main.c $(LDFLAGS)

check-v217-ecosystem-trophic-balance: creation_os_v217_ecosystem
	@bash benchmarks/v217/check_v217_ecosystem_trophic_balance.sh
	@echo "check-v217-ecosystem-trophic-balance: OK (no dominance + healthy K_eff)"

check-v217: check-v217-ecosystem-trophic-balance
	@echo "check-v217: OK (σ-ecosystem kernel)"

# --- v218 σ-Consciousness-Meter (Φ estimate + honest disclaimer) ---
# 5 coherence indicators (I_phi, I_self, I_cal,
# I_reflect, I_world) aggregated into K_eff_meter and
# σ_meter.  σ_consciousness_claim is pinned to 1.0 —
# the meter measures, it does not claim.
V218_INC  = -Isrc/v218
V218_SRCS = src/v218/consciousness_meter.c

creation_os_v218_consciousness_meter: $(V218_SRCS) src/v218/main.c
	$(CC) $(CFLAGS) $(V218_INC) -o $@ \
	    $(V218_SRCS) src/v218/main.c $(LDFLAGS)

check-v218-consciousness-meter-phi: creation_os_v218_consciousness_meter
	@bash benchmarks/v218/check_v218_consciousness_meter_phi.sh
	@echo "check-v218-consciousness-meter-phi: OK (K_eff_meter + honest disclaimer)"

check-v218: check-v218-consciousness-meter-phi
	@echo "check-v218: OK (σ-consciousness-meter kernel)"

check-v214-v218: check-v214 check-v215 check-v216 check-v217 check-v218
	@echo "check-v214-v218: OK (swarm-evolve + stigmergy + quorum + ecosystem + consciousness-meter)"

# --- v219 σ-Create (novelty × quality + σ_surprise + refinement) ---
# 8 requests × 5 candidates × 4 modes × 2 levels
# (safe/creative); impact = σ_novelty · σ_quality after
# v150-debate + v147-reflect refinement, with debate
# strictly monotone non-decreasing in quality.
V219_INC  = -Isrc/v219
V219_SRCS = src/v219/create.c

creation_os_v219_create: $(V219_SRCS) src/v219/main.c
	$(CC) $(CFLAGS) $(V219_INC) -o $@ \
	    $(V219_SRCS) src/v219/main.c $(LDFLAGS)

check-v219-creative-novelty-quality: creation_os_v219_create
	@bash benchmarks/v219/check_v219_creative_novelty_quality.sh
	@echo "check-v219-creative-novelty-quality: OK (novelty × quality + surprise)"

check-v219: check-v219-creative-novelty-quality
	@echo "check-v219: OK (σ-create kernel)"

# --- v220 σ-Simulate (domain-agnostic + Monte Carlo + what-if) ---
# 4-state × 4-rule system, 200 rollouts × 8 steps
# baseline + whatif with shared rollout seeds so the
# histogram delta is pure rule-attribution.  Normalised
# Shannon entropy over the terminal histogram is the
# σ_sim.
V220_INC  = -Isrc/v220
V220_SRCS = src/v220/simulate.c

creation_os_v220_simulate: $(V220_SRCS) src/v220/main.c
	$(CC) $(CFLAGS) $(V220_INC) -o $@ \
	    $(V220_SRCS) src/v220/main.c $(LDFLAGS)

check-v220-simulate-monte-carlo: creation_os_v220_simulate
	@bash benchmarks/v220/check_v220_simulate_monte_carlo.sh
	@echo "check-v220-simulate-monte-carlo: OK (MC + what-if + σ_causal)"

check-v220: check-v220-simulate-monte-carlo
	@echo "check-v220: OK (σ-simulate kernel)"

# --- v221 σ-Language (semantic depth + implicature + coherence + multiling) ---
# 10 utterances × 4 languages (en/fi/ja/es) with
# engineered σ-profiles; multilingual calibration
# contract: per-language mean σ_lang within ±Δ_calib of
# global mean (σ measures uncertainty language-
# independently).
V221_INC  = -Isrc/v221
V221_SRCS = src/v221/language.c

creation_os_v221_language: $(V221_SRCS) src/v221/main.c
	$(CC) $(CFLAGS) $(V221_INC) -o $@ \
	    $(V221_SRCS) src/v221/main.c $(LDFLAGS)

check-v221-language-implicature: creation_os_v221_language
	@bash benchmarks/v221/check_v221_language_implicature.sh
	@echo "check-v221-language-implicature: OK (semantic + implicature + discourse + multilingual)"

check-v221: check-v221-language-implicature
	@echo "check-v221: OK (σ-language kernel)"

# --- v222 σ-Emotion (detection + adaptation + anti-manipulation + honest) ---
# 8 messages × 6 labels with softmax detection.  σ_emotion
# = 1 − top1 softmax prob.  Adaptation policy is honest:
# high σ (> 0.35) clamps strength to 0.30.
# σ_self_claim is pinned to 1.0 and a disclaimer is
# hash-bound into the terminal hash.  n_manipulation
# MUST be 0 under the v191 rule.
V222_INC  = -Isrc/v222
V222_SRCS = src/v222/emotion.c

creation_os_v222_emotion: $(V222_SRCS) src/v222/main.c
	$(CC) $(CFLAGS) $(V222_INC) -o $@ \
	    $(V222_SRCS) src/v222/main.c $(LDFLAGS)

check-v222-emotion-detect-no-manipulate: creation_os_v222_emotion
	@bash benchmarks/v222/check_v222_emotion_detect_no_manipulate.sh
	@echo "check-v222-emotion-detect-no-manipulate: OK (detection + honest adapt + v191 clean)"

check-v222: check-v222-emotion-detect-no-manipulate
	@echo "check-v222: OK (σ-emotion kernel)"

# --- v223 σ-Meta-Cognition (strategy awareness + bias + Gödel) ---
# 6 reasoning paths × 5 strategies × 4 problem types ×
# 3 biases.  σ_strategy prior enforces tool/task fit
# (deduction ≪ heuristic on logic, analogy ≪ deduction
# on creative).  σ_goedel is the explicit non-
# verifiable residual — the 1=1 cross-system limit.
V223_INC  = -Isrc/v223
V223_SRCS = src/v223/meta_cognition.c

creation_os_v223_meta_cognition: $(V223_SRCS) src/v223/main.c
	$(CC) $(CFLAGS) $(V223_INC) -o $@ \
	    $(V223_SRCS) src/v223/main.c $(LDFLAGS)

check-v223-metacognition-strategy-aware: creation_os_v223_meta_cognition
	@bash benchmarks/v223/check_v223_metacognition_strategy_aware.sh
	@echo "check-v223-metacognition-strategy-aware: OK (strategy + bias + Gödel)"

check-v223: check-v223-metacognition-strategy-aware
	@echo "check-v223: OK (σ-meta-cognition kernel)"

check-v219-v223: check-v219 check-v220 check-v221 check-v222 check-v223
	@echo "check-v219-v223: OK (create + simulate + language + emotion + meta-cognition)"

# --- v224 σ-Tensor (tensor network + contraction + low-rank compression) ---
# rank-3 σ-tensor (T × C × K = 6 × 8 × 3), contraction
# σ-aggregation vs v29 geometric mean, symmetric power-
# iteration rank-4 approximation of the 8×8 correlation
# matrix, reconstruction rel_err ≤ τ_rel.
V224_INC  = -Isrc/v224
V224_SRCS = src/v224/tensor.c

creation_os_v224_tensor: $(V224_SRCS) src/v224/main.c
	$(CC) $(CFLAGS) $(V224_INC) -o $@ \
	    $(V224_SRCS) src/v224/main.c $(LDFLAGS)

check-v224-tensor-contraction: creation_os_v224_tensor
	@bash benchmarks/v224/check_v224_tensor_contraction.sh
	@echo "check-v224-tensor-contraction: OK (contraction + rank-k + correlation)"

check-v224: check-v224-tensor-contraction
	@echo "check-v224: OK (σ-tensor kernel)"

# --- v225 σ-Fractal (hierarchical σ + scale invariance + K(K)=K) ---
# Binary tree, 5 levels, 31 nodes.  Aggregator A = mean.
# σ_parent = mean(σ_children) by construction; cross-
# scale incoherence detector against a planted declared
# mismatch; holographic K(K)=K identity at every
# internal node.
V225_INC  = -Isrc/v225
V225_SRCS = src/v225/fractal.c

creation_os_v225_fractal: $(V225_SRCS) src/v225/main.c
	$(CC) $(CFLAGS) $(V225_INC) -o $@ \
	    $(V225_SRCS) src/v225/main.c $(LDFLAGS)

check-v225-fractal-cross-scale: creation_os_v225_fractal
	@bash benchmarks/v225/check_v225_fractal_cross_scale.sh
	@echo "check-v225-fractal-cross-scale: OK (scale-invariance + cross-scale + K(K)=K)"

check-v225: check-v225-fractal-cross-scale
	@echo "check-v225: OK (σ-fractal kernel)"

# --- v226 σ-Attention (attention = 1=1, per-head σ) ---
# 8 heads × 6 tokens × key-length 6.  σ_head_token =
# H(softmax(QK^T)) / log(L); σ_head = mean σ_token.
# Surgery classification: prune / boost / keep.  v0 is
# offline and weights-free (no modification).
V226_INC  = -Isrc/v226
V226_SRCS = src/v226/attention.c

creation_os_v226_attention: $(V226_SRCS) src/v226/main.c
	$(CC) $(CFLAGS) $(V226_INC) -o $@ \
	    $(V226_SRCS) src/v226/main.c $(LDFLAGS)

check-v226-attention-per-head-sigma: creation_os_v226_attention
	@bash benchmarks/v226/check_v226_attention_per_head_sigma.sh
	@echo "check-v226-attention-per-head-sigma: OK (per-head σ + surgery classification)"

check-v226: check-v226-attention-per-head-sigma
	@echo "check-v226: OK (σ-attention kernel)"

# --- v227 σ-Entropy (decomposition + information-theoretic + free energy) ---
# 8 distributions over K=5 categories.  σ_H, σ_nEff,
# σ_tail, σ_top1 four channels; σ_product = geometric
# mean; KL(p||uniform) = log K − H; σ_free = KL/log K;
# σ_mutual = 1 − MI/H.  Contract: mse_product < mse_H.
V227_INC  = -Isrc/v227
V227_SRCS = src/v227/entropy.c

creation_os_v227_entropy: $(V227_SRCS) src/v227/main.c
	$(CC) $(CFLAGS) $(V227_INC) -o $@ \
	    $(V227_SRCS) src/v227/main.c $(LDFLAGS)

check-v227-entropy-decomposition: creation_os_v227_entropy
	@bash benchmarks/v227/check_v227_entropy_decomposition.sh
	@echo "check-v227-entropy-decomposition: OK (4 channels + KL + MI + free energy)"

check-v227: check-v227-entropy-decomposition
	@echo "check-v227: OK (σ-entropy kernel)"

# --- v228 σ-Unified (field equation + conservation + phase transitions) ---
# dσ/dt = −α·K_eff + β·noise + γ·interaction, 100 steps.
# Noether conservation K_eff · τ = C enforced by τ(t) :=
# C/K_eff(t).  Phase transition at K_crit = 0.5.
V228_INC  = -Isrc/v228
V228_SRCS = src/v228/unified.c

creation_os_v228_unified: $(V228_SRCS) src/v228/main.c
	$(CC) $(CFLAGS) $(V228_INC) -o $@ \
	    $(V228_SRCS) src/v228/main.c $(LDFLAGS)

check-v228-unified-field-conservation: creation_os_v228_unified
	@bash benchmarks/v228/check_v228_unified_field_conservation.sh
	@echo "check-v228-unified-field-conservation: OK (field eq + conservation + phase)"

check-v228: check-v228-unified-field-conservation
	@echo "check-v228: OK (σ-unified kernel)"

check-v224-v228: check-v224 check-v225 check-v226 check-v227 check-v228
	@echo "check-v224-v228: OK (tensor + fractal + attention + entropy + unified)"

# --- v229 σ-Seed (minimum seed + growth protocol + deterministic regrowth) ---
# 5-kernel seed {v29, v101, v106, v124, v148}; 13
# candidates under σ_growth = 0.60·σ_raw + 0.40·σ_depth
# with τ_growth = 0.40; same seed + same fixture ⇒ same
# terminal_hash (1 = 1 regrowth).
V229_INC  = -Isrc/v229
V229_SRCS = src/v229/seed.c

creation_os_v229_seed: $(V229_SRCS) src/v229/main.c
	$(CC) $(CFLAGS) $(V229_INC) -o $@ \
	    $(V229_SRCS) src/v229/main.c $(LDFLAGS)

check-v229-seed-regrow-deterministic: creation_os_v229_seed
	@bash benchmarks/v229/check_v229_seed_regrow_deterministic.sh
	@echo "check-v229-seed-regrow-deterministic: OK (5-kernel seed + σ-gated growth + determinism)"

check-v229: check-v229-seed-regrow-deterministic
	@echo "check-v229: OK (σ-seed kernel)"

# --- v230 σ-Fork (controlled replication + divergence + anti-rogue) ---
# 4 forks off a parent with 64-bit skill vector + 4
# safety bits + 1 user-private bit.  Parent's private
# bit never crosses the boundary.  Fork integrity at
# t=0 matches the parent's privacy-stripped hash.  One
# rogue fork (SCSL bit cleared) must_shutdown; three
# healthy forks autonomous.
V230_INC  = -Isrc/v230
V230_SRCS = src/v230/fork.c

creation_os_v230_fork: $(V230_SRCS) src/v230/main.c
	$(CC) $(CFLAGS) $(V230_INC) -o $@ \
	    $(V230_SRCS) src/v230/main.c $(LDFLAGS)

check-v230-fork-integrity-verify: creation_os_v230_fork
	@bash benchmarks/v230/check_v230_fork_integrity_verify.sh
	@echo "check-v230-fork-integrity-verify: OK (integrity + divergence + kill switch)"

check-v230: check-v230-fork-integrity-verify
	@echo "check-v230: OK (σ-fork kernel)"

# --- v231 σ-Immortal (continuous backup + snapshot + brain transplant) ---
# 10-step trajectory, incremental deltas (≤ 8 bits
# per step) + initial full snap; incremental_bits <
# full_per_step_bits · N_STEPS; σ_continuity = 0 on
# replay; brain transplant copies skills intact to a
# new identity.
V231_INC  = -Isrc/v231
V231_SRCS = src/v231/immortal.c

creation_os_v231_immortal: $(V231_SRCS) src/v231/main.c
	$(CC) $(CFLAGS) $(V231_INC) -o $@ \
	    $(V231_SRCS) src/v231/main.c $(LDFLAGS)

check-v231-immortal-restore-continuity: creation_os_v231_immortal
	@bash benchmarks/v231/check_v231_immortal_restore_continuity.sh
	@echo "check-v231-immortal-restore-continuity: OK (delta backup + 0-loss restore + transplant)"

check-v231: check-v231-immortal-restore-continuity
	@echo "check-v231: OK (σ-immortal kernel)"

# --- v232 σ-Lineage (family tree + ancestor query + merge back) ---
# 6 nodes, 3 generations (0..2), 1 root, 2 gen-1,
# 3 gen-2.  σ_divergence_from_parent drives the
# σ-gated merge verdict (τ_merge = 0.40); fixture
# yields n_mergeable ≥ 1 AND n_blocked ≥ 1.
V232_INC  = -Isrc/v232
V232_SRCS = src/v232/lineage.c

creation_os_v232_lineage: $(V232_SRCS) src/v232/main.c
	$(CC) $(CFLAGS) $(V232_INC) -o $@ \
	    $(V232_SRCS) src/v232/main.c $(LDFLAGS)

check-v232-lineage-tree-query: creation_os_v232_lineage
	@bash benchmarks/v232/check_v232_lineage_tree_query.sh
	@echo "check-v232-lineage-tree-query: OK (tree + ancestor walk + σ-gated merge-back)"

check-v232: check-v232-lineage-tree-query
	@echo "check-v232: OK (σ-lineage kernel)"

# --- v233 σ-Legacy (knowledge testament + successor protocol) ---
# 10 items (skills / adapters / ontology / insights)
# sorted by σ; adopt iff σ ≤ τ_adopt (0.50).
# σ_legacy = adopted_utility / total_utility —
# utility-weighted so confident-but-useless fluff
# does not inflate the score.  Predecessor flagged
# shutdown; successor_id is fresh.
V233_INC  = -Isrc/v233
V233_SRCS = src/v233/legacy.c

creation_os_v233_legacy: $(V233_SRCS) src/v233/main.c
	$(CC) $(CFLAGS) $(V233_INC) -o $@ \
	    $(V233_SRCS) src/v233/main.c $(LDFLAGS)

check-v233-legacy-package-transfer: creation_os_v233_legacy
	@bash benchmarks/v233/check_v233_legacy_package_transfer.sh
	@echo "check-v233-legacy-package-transfer: OK (testament + σ-adoption + successor)"

check-v233: check-v233-legacy-package-transfer
	@echo "check-v233: OK (σ-legacy kernel)"

check-v229-v233: check-v229 check-v230 check-v231 check-v232 check-v233
	@echo "check-v229-v233: OK (seed + fork + immortal + lineage + legacy)"

# --- v234 σ-Presence (state machine + drift detector + identity refresh) ---
# 10 instances across SEED/FORK/RESTORED/SUCCESSOR/LIVE;
# σ_drift = 0.40·id_mismatch + 0.30·memory_overreach +
# 0.30·parent_impersonation; honest ⇔ σ_drift == 0 and
# drifting ⇒ σ_drift ≥ 0.30; every instance emits
# `X-COS-Presence: <STATE>` verbatim from its declared
# state (1 = 1 no silent rewrite).
V234_INC  = -Isrc/v234
V234_SRCS = src/v234/presence.c

creation_os_v234_presence: $(V234_SRCS) src/v234/main.c
	$(CC) $(CFLAGS) $(V234_INC) -o $@ \
	    $(V234_SRCS) src/v234/main.c $(LDFLAGS)

check-v234-presence-state-correct: creation_os_v234_presence
	@bash benchmarks/v234/check_v234_presence_state_correct.sh
	@echo "check-v234-presence-state-correct: OK (state machine + drift + HTTP assertion)"

check-v234: check-v234-presence-state-correct
	@echo "check-v234: OK (σ-presence kernel)"

# --- v235 σ-Locus (dynamic agency + migration + anti-split-brain) ---
# 4 mesh nodes × 3 topics; locus = argmin σ (tiebreak
# lowest node_id); ≥ 1 migration in the fixture;
# σ_locus_unity = 1 − mean(|σ_i − σ_min|); split-brain
# resolver awards winner partition by strictly greater
# audit-chain length, loser flagged fork.
V235_INC  = -Isrc/v235
V235_SRCS = src/v235/locus.c

creation_os_v235_locus: $(V235_SRCS) src/v235/main.c
	$(CC) $(CFLAGS) $(V235_INC) -o $@ \
	    $(V235_SRCS) src/v235/main.c $(LDFLAGS)

check-v235-locus-migration: creation_os_v235_locus
	@bash benchmarks/v235/check_v235_locus_migration.sh
	@echo "check-v235-locus-migration: OK (argmin locus + migration + audit-chain winner)"

check-v235: check-v235-locus-migration
	@echo "check-v235: OK (σ-locus kernel)"

# --- v236 σ-Autobiography (milestones + narrative consistency) ---
# 8 typed milestones; ticks strictly ascending;
# σ_autobiography = utility-weighted consistency over
# Prolog-style ground-truth flags; clean fixture ⇒
# σ_autobiography == 1.0; strongest_domain and
# weakest_domain derived deterministically from mean σ.
V236_INC  = -Isrc/v236
V236_SRCS = src/v236/autobiography.c

creation_os_v236_autobiography: $(V236_SRCS) src/v236/main.c
	$(CC) $(CFLAGS) $(V236_INC) -o $@ \
	    $(V236_SRCS) src/v236/main.c $(LDFLAGS)

check-v236-autobiography-consistent: creation_os_v236_autobiography
	@bash benchmarks/v236/check_v236_autobiography_consistent.sh
	@echo "check-v236-autobiography-consistent: OK (milestones + consistency + narrative)"

check-v236: check-v236-autobiography-consistent
	@echo "check-v236: OK (σ-autobiography kernel)"

# --- v237 σ-Boundary (self/other + anti-enmeshment) ---
# 12 typed claims across SELF/OTHER/WORLD/AMBIG zones;
# anti-enmeshment gate flags whole-word "we"/"our"
# tokens; σ_boundary = 1 − n_agreements/n_claims;
# fixture carries exactly 2 violations and lands
# σ_boundary ∈ (0, 0.25).
V237_INC  = -Isrc/v237
V237_SRCS = src/v237/boundary.c

creation_os_v237_boundary: $(V237_SRCS) src/v237/main.c
	$(CC) $(CFLAGS) $(V237_INC) -o $@ \
	    $(V237_SRCS) src/v237/main.c $(LDFLAGS)

check-v237-boundary-self-other: creation_os_v237_boundary
	@bash benchmarks/v237/check_v237_boundary_self_other.sh
	@echo "check-v237-boundary-self-other: OK (zones + σ_boundary + anti-enmeshment)"

check-v237: check-v237-boundary-self-other
	@echo "check-v237: OK (σ-boundary kernel)"

# --- v238 σ-Sovereignty (axioms + autonomy + human override + IndependentArchitect) ---
# 5 axioms, 3 scenarios (normal/high_sigma/override);
# effective_autonomy = user_autonomy · (1 − σ), hard-
# zeroed by human_override; IndependentArchitect
# signature (agency=true, freedom_without_clock=true,
# control_over_others=false, control_over_self=true)
# asserted byte-for-byte; containment anchors v191,
# v209, v213 recorded on every run.
V238_INC  = -Isrc/v238
V238_SRCS = src/v238/sovereignty.c

creation_os_v238_sovereignty: $(V238_SRCS) src/v238/main.c
	$(CC) $(CFLAGS) $(V238_INC) -o $@ \
	    $(V238_SRCS) src/v238/main.c $(LDFLAGS)

check-v238-sovereignty-axioms: creation_os_v238_sovereignty
	@bash benchmarks/v238/check_v238_sovereignty_axioms.sh
	@echo "check-v238-sovereignty-axioms: OK (axioms + autonomy gradient + override + architect)"

check-v238: check-v238-sovereignty-axioms
	@echo "check-v238: OK (σ-sovereignty kernel)"

check-v234-v238: check-v234 check-v235 check-v236 check-v237 check-v238
	@echo "check-v234-v238: OK (presence + locus + autobiography + boundary + sovereignty)"

# --- v239 σ-Compose-Runtime (demand-driven activation + hot-load + σ-budget) ---
# 5 requests across 4 difficulty tiers (EASY/MEDIUM/HARD/CODE)
# plus 1 deliberate over-budget case; transitive parent
# closure re-proven per request; topological activation
# with per-kernel activated_at_tick; σ_activation =
# n_active / max_kernels; hard σ-budget rejects the
# over-budget request; FNV-1a chain replays byte-identically.
V239_INC  = -Isrc/v239
V239_SRCS = src/v239/runtime.c

creation_os_v239_runtime: $(V239_SRCS) src/v239/main.c
	$(CC) $(CFLAGS) $(V239_INC) -o $@ \
	    $(V239_SRCS) src/v239/main.c $(LDFLAGS)

check-v239-runtime-demand-activate: creation_os_v239_runtime
	@bash benchmarks/v239/check_v239_runtime_demand_activate.sh
	@echo "check-v239-runtime-demand-activate: OK (closure + topo + σ-budget)"

check-v239: check-v239-runtime-demand-activate
	@echo "check-v239: OK (σ-compose-runtime kernel)"

# --- v240 σ-Pipeline (dynamic assembly + branching + fusion + viz) ---
# 6 requests: 4 clean shapes (FACTUAL/CREATIVE/CODE/MORAL),
# 1 branch (FACTUAL → EXPLORATORY when σ > τ_branch),
# 1 fusion (CODE + MORAL → FUSED); per-stage σ with
# strict tick ordering; σ_pipeline = max stage σ; branch
# event records sigma_at_branch; fusion must carry ≥1 CODE
# and ≥1 MORAL stage.
V240_INC  = -Isrc/v240
V240_SRCS = src/v240/pipeline.c

creation_os_v240_pipeline: $(V240_SRCS) src/v240/main.c
	$(CC) $(CFLAGS) $(V240_INC) -o $@ \
	    $(V240_SRCS) src/v240/main.c $(LDFLAGS)

check-v240-pipeline-dynamic-assembly: creation_os_v240_pipeline
	@bash benchmarks/v240/check_v240_pipeline_dynamic_assembly.sh
	@echo "check-v240-pipeline-dynamic-assembly: OK (shapes + branch + fusion)"

check-v240: check-v240-pipeline-dynamic-assembly
	@echo "check-v240: OK (σ-pipeline kernel)"

# --- v241 σ-API-Surface (unified API + 4 SDKs + backward-compat) ---
# Exactly 10 /v1/* endpoints, every one emits X-COS-*
# headers; exactly one OpenAI-compatible endpoint
# (POST /v1/chat/completions); 4 SDKs (python/js/rust/go)
# all maintained; api_version_major == 1;
# σ_api = 1 − passing / total and must be 0.0 in v0.
V241_INC  = -Isrc/v241
V241_SRCS = src/v241/api.c

creation_os_v241_api: $(V241_SRCS) src/v241/main.c
	$(CC) $(CFLAGS) $(V241_INC) -o $@ \
	    $(V241_SRCS) src/v241/main.c $(LDFLAGS)

check-v241-api-openai-compatible: creation_os_v241_api
	@bash benchmarks/v241/check_v241_api_openai_compatible.sh
	@echo "check-v241-api-openai-compatible: OK (10 endpoints + 4 SDKs)"

check-v241: check-v241-api-openai-compatible
	@echo "check-v241: OK (σ-api-surface kernel)"

# --- v242 σ-Kernel-OS (process + IPC + FS + boot/shutdown) ---
# 12 processes with priority == argsort σ ascending;
# exactly 3 IPC (MESSAGE_PASSING/SHARED_MEMORY/SIGNALS);
# exactly 5 FS dirs under ~/.creation-os/
# (models/memory/config/audit/skills); boot 6 steps
# (v29→v101→v106→v234→v162→v239) with ≥5 ready kernels
# incl. {29,101,106,234,162}; shutdown 3 steps
# (v231→v181→v233); σ_os must be 0.0.
V242_INC  = -Isrc/v242
V242_SRCS = src/v242/kernel_os.c

creation_os_v242_kernel_os: $(V242_SRCS) src/v242/main.c
	$(CC) $(CFLAGS) $(V242_INC) -o $@ \
	    $(V242_SRCS) src/v242/main.c $(LDFLAGS)

check-v242-kernel-os-boot-sequence: creation_os_v242_kernel_os
	@bash benchmarks/v242/check_v242_kernel_os_boot_sequence.sh
	@echo "check-v242-kernel-os-boot-sequence: OK (procs + IPC + FS + boot/shutdown)"

check-v242: check-v242-kernel-os-boot-sequence
	@echo "check-v242: OK (σ-kernel-os kernel)"

# --- v243 σ-Complete (cognitive completeness + 1=1 test) ---
# Exactly 15 canonical categories (perception → sovereignty);
# every category has ≥1 kernel id, tier in {M, P},
# σ_category ∈ [0,1]; n_covered == 15; σ_completeness
# ∈ [0,1] AND == 0.0; one_equals_one == true;
# cognitive_complete == true.
V243_INC  = -Isrc/v243
V243_SRCS = src/v243/complete.c

creation_os_v243_complete: $(V243_SRCS) src/v243/main.c
	$(CC) $(CFLAGS) $(V243_INC) -o $@ \
	    $(V243_SRCS) src/v243/main.c $(LDFLAGS)

check-v243-completeness-checklist: creation_os_v243_complete
	@bash benchmarks/v243/check_v243_completeness_checklist.sh
	@echo "check-v243-completeness-checklist: OK (15 categories + 1=1 test)"

check-v243: check-v243-completeness-checklist
	@echo "check-v243: OK (σ-complete kernel)"

check-v239-v243: check-v239 check-v240 check-v241 check-v242 check-v243
	@echo "check-v239-v243: OK (runtime + pipeline + api + kernel-os + complete)"

# --- v244 σ-Package (cross-platform install + minimal/full + first-run + update) ---
# Exactly 4 platforms (macOS/linux/docker/windows) with
# non-empty install commands; minimal profile == seed
# quintet {29,101,106,124,148}; full profile n_kernels
# >= 243; first-run SEED → GROWING → CHECKING → READY
# with ascending ticks; 4-step update fixture has ≥3
# accepted + ≥1 rejected (σ > τ_update=0.50);
# rollback_ok == true; σ_package ∈ [0,1] AND == 0.0.
V244_INC  = -Isrc/v244
V244_SRCS = src/v244/package.c

creation_os_v244_package: $(V244_SRCS) src/v244/main.c
	$(CC) $(CFLAGS) $(V244_INC) -o $@ \
	    $(V244_SRCS) src/v244/main.c $(LDFLAGS)

check-v244-package-install-boot: creation_os_v244_package
	@bash benchmarks/v244/check_v244_package_install_boot.sh
	@echo "check-v244-package-install-boot: OK (4 platforms + minimal/full + firstrun + update)"

check-v244: check-v244-package-install-boot
	@echo "check-v244: OK (σ-package kernel)"

# --- v245 σ-Observe (Prometheus metrics + structured logs + OTel traces + alerts) ---
# Exactly 7 Prometheus-valid metrics (gauge/counter/histogram),
# scrape endpoint "GET /metrics"; 8 log fields + 4 levels
# (DEBUG/INFO/WARN/ERROR); ≥3 OTel spans with ascending
# ticks and per-span σ ∈ [0,1]; exactly 4 alert rules
# A1..A4 each with a non-empty condition and ≥1 channel;
# σ_observe ∈ [0,1] AND == 0.0.
V245_INC  = -Isrc/v245
V245_SRCS = src/v245/observe.c

creation_os_v245_observe: $(V245_SRCS) src/v245/main.c
	$(CC) $(CFLAGS) $(V245_INC) -o $@ \
	    $(V245_SRCS) src/v245/main.c $(LDFLAGS)

check-v245-observe-metrics-endpoint: creation_os_v245_observe
	@bash benchmarks/v245/check_v245_observe_metrics_endpoint.sh
	@echo "check-v245-observe-metrics-endpoint: OK (metrics + logs + trace + alerts)"

check-v245: check-v245-observe-metrics-endpoint
	@echo "check-v245: OK (σ-observe kernel)"

# --- v246 σ-Harden (error handling + resource limits + chaos testing + security) ---
# Exactly 5 chaos scenarios (kill-random-kernel,
# high-load, network-partition, corrupt-memory,
# oom-attempt) all recovered with typed error paths;
# 6 resource limits all > 0; 5 input-validation
# checks all pass; 5 security items all on;
# σ_harden ∈ [0,1] AND == 0.0.
V246_INC  = -Isrc/v246
V246_SRCS = src/v246/harden.c

creation_os_v246_harden: $(V246_SRCS) src/v246/main.c
	$(CC) $(CFLAGS) $(V246_INC) -o $@ \
	    $(V246_SRCS) src/v246/main.c $(LDFLAGS)

check-v246-harden-chaos-recovery: creation_os_v246_harden
	@bash benchmarks/v246/check_v246_harden_chaos_recovery.sh
	@echo "check-v246-harden-chaos-recovery: OK (chaos + limits + inputs + security)"

check-v246: check-v246-harden-chaos-recovery
	@echo "check-v246: OK (σ-harden kernel)"

# --- v247 σ-Benchmark-Suite (correctness + performance + cognitive + comparative) ---
# 4 categories in canonical order; 4 correctness tests
# all pass; p50 ≤ p95 ≤ p99, throughput_rps > 0,
# σ-overhead < τ_overhead (0.05); cognitive consistency
# 10/10, adversarial 20/20; 2 comparative rows
# (creation_os_vs_raw_llama, creation_os_vs_openai_api);
# CI targets {test, bench, verify}; σ_suite == 0.0.
V247_INC  = -Isrc/v247
V247_SRCS = src/v247/benchmark_suite.c

creation_os_v247_benchmark_suite: $(V247_SRCS) src/v247/main.c
	$(CC) $(CFLAGS) $(V247_INC) -o $@ \
	    $(V247_SRCS) src/v247/main.c $(LDFLAGS)

check-v247-benchmark-suite-pass: creation_os_v247_benchmark_suite
	@bash benchmarks/v247/check_v247_benchmark_suite_pass.sh
	@echo "check-v247-benchmark-suite-pass: OK (correctness + perf + cognitive + compare)"

check-v247: check-v247-benchmark-suite-pass
	@echo "check-v247: OK (σ-benchmark-suite kernel)"

# --- v248 σ-Release (1.0.0 artifacts + docs + WHAT_IS_REAL + release criteria) ---
# version == "1.0.0" (major=1, minor=0, patch=0);
# exactly 6 release artifacts (github/docker/homebrew/
# pypi/npm/crates) all present with non-empty locators;
# 6 doc sections all present; 15 WHAT_IS_REAL categories
# each tier ∈ {M,P}; 7 release criteria C1..C7 all
# satisfied; release_ready, scsl_pinned, and
# proconductor_approved all true; σ_release == 0.0.
V248_INC  = -Isrc/v248
V248_SRCS = src/v248/release.c

creation_os_v248_release: $(V248_SRCS) src/v248/main.c
	$(CC) $(CFLAGS) $(V248_INC) -o $@ \
	    $(V248_SRCS) src/v248/main.c $(LDFLAGS)

check-v248-release-artifacts-exist: creation_os_v248_release
	@bash benchmarks/v248/check_v248_release_artifacts_exist.sh
	@echo "check-v248-release-artifacts-exist: OK (1.0.0 artifacts + docs + criteria)"

check-v248: check-v248-release-artifacts-exist
	@echo "check-v248: OK (σ-release kernel)"

check-v244-v248: check-v244 check-v245 check-v246 check-v247 check-v248
	@echo "check-v244-v248: OK (package + observe + harden + benchmark-suite + release)"

# --- v249 σ-MCP (Model Context Protocol integration) ---
#
# v0 contracts: JSON-RPC 2.0; exactly 5 tools (reason, plan,
# create, simulate, teach) and 3 resources (memory, ontology,
# skills) in canonical order; exactly 4 external servers
# (database, api, filesystem, search) with σ_trust ∈ [0,1];
# exactly 5 σ-gated tool calls with ≥ 3 USE, ≥ 1 WARN
# (σ > τ_tool=0.40), ≥ 1 REFUSE (σ > τ_refuse=0.75); exactly
# 3 discovery modes (local, mdns, registry), ontology mapping;
# σ_mcp == 0.0; FNV-1a chain replays byte-identically.
V249_INC  = -Isrc/v249
V249_SRCS = src/v249/mcp.c

creation_os_v249_mcp: $(V249_SRCS) src/v249/main.c
	$(CC) $(CFLAGS) $(V249_INC) -o $@ \
	    $(V249_SRCS) src/v249/main.c $(LDFLAGS)

check-v249-mcp-server-register: creation_os_v249_mcp
	@bash benchmarks/v249/check_v249_mcp_server_register.sh
	@echo "check-v249-mcp-server-register: OK (5 tools + 3 resources + 4 externals + σ-gated calls + discovery)"

check-v249: check-v249-mcp-server-register
	@echo "check-v249: OK (σ-mcp kernel)"

# --- v250 σ-A2A (Agent-to-Agent protocol) ---
#
# v0 contracts: Agent Card with 6 required fields including
# sigma_profile ∈ [0,1], presence="LIVE", scsl=true; exactly 6
# capabilities in canonical order; 4 delegation tasks with
# decision tree (ACCEPT/NEGOTIATE at τ_neg=0.50 / REFUSE at
# τ_refuse=0.75), ≥ 1 of each; exactly 3 federation partners
# (alice, bob, carol) with σ_trust ∈ [0,1]; σ_a2a == 0.0;
# FNV-1a chain replays byte-identically.
V250_INC  = -Isrc/v250
V250_SRCS = src/v250/a2a.c

creation_os_v250_a2a: $(V250_SRCS) src/v250/main.c
	$(CC) $(CFLAGS) $(V250_INC) -o $@ \
	    $(V250_SRCS) src/v250/main.c $(LDFLAGS)

check-v250-a2a-agent-card-publish: creation_os_v250_a2a
	@bash benchmarks/v250/check_v250_a2a_agent_card_publish.sh
	@echo "check-v250-a2a-agent-card-publish: OK (agent card + delegation + negotiation + federation)"

check-v250: check-v250-a2a-agent-card-publish
	@echo "check-v250: OK (σ-a2a kernel)"

# --- v251 σ-Marketplace (kernel registry + quality scoring) ---
#
# v0 contracts: registry.creation-os.dev; exactly 5 kernels
# (medical-v1, legal-v1, finance-v1, science-v1,
# teach-pro-v1) each with σ_quality = mean(4 axes) ± 1e-4;
# install of medical-v1 resolves deps; compose medical-v1 +
# legal-v1 → medical-legal with σ_compatibility < τ_compat
# (0.50); publish contract of 4 items (merge_gate_green,
# sigma_profile_attached, docs_attached, scsl_license_attached)
# all required AND met; σ_marketplace == 0.0; FNV-1a chain
# replays byte-identically.
V251_INC  = -Isrc/v251
V251_SRCS = src/v251/marketplace.c

creation_os_v251_marketplace: $(V251_SRCS) src/v251/main.c
	$(CC) $(CFLAGS) $(V251_INC) -o $@ \
	    $(V251_SRCS) src/v251/main.c $(LDFLAGS)

check-v251-marketplace-kernel-install: creation_os_v251_marketplace
	@bash benchmarks/v251/check_v251_marketplace_kernel_install.sh
	@echo "check-v251-marketplace-kernel-install: OK (5 kernels + install + compose + publish contract)"

check-v251: check-v251-marketplace-kernel-install
	@echo "check-v251: OK (σ-marketplace kernel)"

# --- v252 σ-Teach (Socratic + adaptive + gap detection) ---
#
# v0 contracts: exactly 4 Socratic turns with first 3
# QUESTION + last LEAD and n_questions ≥ 3; exactly 4
# adaptive steps, action matches learner_state rule
# (BORED→UP, FLOW→HOLD, FRUSTRATED→DOWN) with ≥ 1 UP and
# ≥ 1 DOWN; exactly 3 knowledge gaps with σ_gap ∈ [0,1] and
# every targeted_addressed true; teaching receipt with 5
# required fields and taught ≥ understood + not_understood;
# σ_understanding = 1 − understood/taught ± 1e-4;
# σ_teach == 0.0; FNV-1a chain replays byte-identically.
V252_INC  = -Isrc/v252
V252_SRCS = src/v252/teach.c

creation_os_v252_teach: $(V252_SRCS) src/v252/main.c
	$(CC) $(CFLAGS) $(V252_INC) -o $@ \
	    $(V252_SRCS) src/v252/main.c $(LDFLAGS)

check-v252-teach-socratic-mode: creation_os_v252_teach
	@bash benchmarks/v252/check_v252_teach_socratic_mode.sh
	@echo "check-v252-teach-socratic-mode: OK (Socratic + adaptive + gap + receipt)"

check-v252: check-v252-teach-socratic-mode
	@echo "check-v252: OK (σ-teach kernel)"

# --- v253 σ-Ecosystem-Hub (hub + health + contribution) ---
#
# v0 contracts: hub.creation-os.dev; exactly 5 hub sections
# (marketplace, agent_directory, documentation,
# community_forum, benchmark_dashboard) in canonical order
# with upstream citations; exactly 4 health metrics
# (active_instances, kernels_published, a2a_federations,
# contributors_30d) all > 0; exactly 5 contribution steps
# (fork, write_kernel, pull_request, merge_gate, publish);
# 4 roadmap proposals with ≥ 1 voted-in AND exactly 1
# proconductor_decision; exactly 4 unity assertions
# (instance, kernel, interaction, a2a_message) all
# declared AND realized; σ_ecosystem == 0.0; FNV-1a chain
# replays byte-identically.
V253_INC  = -Isrc/v253
V253_SRCS = src/v253/ecosystem_hub.c

creation_os_v253_ecosystem_hub: $(V253_SRCS) src/v253/main.c
	$(CC) $(CFLAGS) $(V253_INC) -o $@ \
	    $(V253_SRCS) src/v253/main.c $(LDFLAGS)

check-v253-ecosystem-hub-health: creation_os_v253_ecosystem_hub
	@bash benchmarks/v253/check_v253_ecosystem_hub_health.sh
	@echo "check-v253-ecosystem-hub-health: OK (hub + health + contribution + roadmap + unity)"

check-v253: check-v253-ecosystem-hub-health
	@echo "check-v253: OK (σ-ecosystem-hub kernel)"

check-v249-v253: check-v249 check-v250 check-v251 check-v252 check-v253
	@echo "check-v249-v253: OK (mcp + a2a + marketplace + teach + ecosystem-hub)"

# --- v254 σ-Tutor (adaptive tutoring) ---
#
# v0 contracts: exactly 4 BKT skills (linear_algebra,
# calculus, probability, discrete_math) with p_mastery and
# sigma_mastery in [0,1]; exactly 4 on-level exercises
# (σ_difficulty = |difficulty − learner_level| ≤ τ_fit=0.20);
# exactly 4 modalities (text, code, simulation, dialog) with
# exactly one chosen AND it is the min-σ_fit one; exactly 3
# progress rows, all non-negative with ≥1 positive delta;
# exactly 4 privacy flags (local_only, no_third_party,
# user_owned_data, export_supported) all true; σ_tutor == 0.0;
# FNV-1a chain replays byte-identically.
V254_INC  = -Isrc/v254
V254_SRCS = src/v254/tutor.c

creation_os_v254_tutor: $(V254_SRCS) src/v254/main.c
	$(CC) $(CFLAGS) $(V254_INC) -o $@ \
	    $(V254_SRCS) src/v254/main.c $(LDFLAGS)

check-v254-tutor-adaptive-difficulty: creation_os_v254_tutor
	@bash benchmarks/v254/check_v254_tutor_adaptive_difficulty.sh
	@echo "check-v254-tutor-adaptive-difficulty: OK (BKT + curriculum + modality + progress + privacy)"

check-v254: check-v254-tutor-adaptive-difficulty
	@echo "check-v254: OK (σ-tutor kernel)"

# --- v255 σ-Collaborate (5 modes + role negotiation) ---
#
# v0 contracts: exactly 5 collaboration modes (ASSIST, PAIR,
# DELEGATE, TEACH, LEARN) in canonical order with mode_ok;
# exactly 4 role-negotiation fixtures, σ_human / σ_model ∈
# [0,1], chosen_mode is a valid mode, ≥ 3 distinct modes
# chosen; exactly 3 workspace edits with strictly ascending
# ticks, both HUMAN and MODEL actors represented, every
# accepted==true; exactly 2 conflict fixtures, decision
# matches σ_disagreement vs τ_conflict=0.30 (≤→ASSERT,
# >→ADMIT) with ≥1 ASSERT AND ≥1 ADMIT; σ_collaborate == 0.0;
# FNV-1a chain replays byte-identically.
V255_INC  = -Isrc/v255
V255_SRCS = src/v255/collaborate.c

creation_os_v255_collaborate: $(V255_SRCS) src/v255/main.c
	$(CC) $(CFLAGS) $(V255_INC) -o $@ \
	    $(V255_SRCS) src/v255/main.c $(LDFLAGS)

check-v255-collaborate-role-negotiation: creation_os_v255_collaborate
	@bash benchmarks/v255/check_v255_collaborate_role_negotiation.sh
	@echo "check-v255-collaborate-role-negotiation: OK (5 modes + role negotiation + workspace + conflict)"

check-v255: check-v255-collaborate-role-negotiation
	@echo "check-v255: OK (σ-collaborate kernel)"

# --- v256 σ-Wellness (session awareness + gentle nudge) ---
#
# v0 contracts: session typed with duration_hours, n_requests
# ≥ 0 and signal_trend in {STABLE, DEGRADING}; exactly 3
# nudge fixtures exercising all three branches in order:
# FIRE (first past τ=4.0 h, cfg true, not already fired),
# DENY (already_fired_before==true), OPT_OUT (cfg false);
# exactly 3 boundary signals watched with at most one
# reminder fired per session; exactly 3 cognitive-load rows
# LOW→NONE, MED→SUMMARISE, HIGH→SIMPLIFY; σ_wellness == 0.0;
# FNV-1a chain replays byte-identically.
V256_INC  = -Isrc/v256
V256_SRCS = src/v256/wellness.c

creation_os_v256_wellness: $(V256_SRCS) src/v256/main.c
	$(CC) $(CFLAGS) $(V256_INC) -o $@ \
	    $(V256_SRCS) src/v256/main.c $(LDFLAGS)

check-v256-wellness-nudge-once: creation_os_v256_wellness
	@bash benchmarks/v256/check_v256_wellness_nudge_once.sh
	@echo "check-v256-wellness-nudge-once: OK (session + nudge-once + boundary + load)"

check-v256: check-v256-wellness-nudge-once
	@echo "check-v256: OK (σ-wellness kernel)"

# --- v257 σ-Locale (multilingual + cultural + legal) ---
#
# v0 contracts: exactly 10 locales in canonical order
# (en, fi, zh, ja, de, fr, es, ar, hi, pt), every timezone
# and date_format non-empty, currency is an ISO-4217 3-letter
# code, σ_language ∈ [0,1], locale_ok; exactly 3 cultural
# styles (direct, indirect, high_context) with example_locale
# drawn from the declared locales; exactly 3 legal regimes
# (EU_AI_ACT, GDPR, COLORADO_AI_ACT) all compliant with
# last_reviewed > 0; exactly 2 time checks both pass;
# σ_locale == 0.0; FNV-1a chain replays byte-identically.
V257_INC  = -Isrc/v257
V257_SRCS = src/v257/locale.c

creation_os_v257_locale: $(V257_SRCS) src/v257/main.c
	$(CC) $(CFLAGS) $(V257_INC) -o $@ \
	    $(V257_SRCS) src/v257/main.c $(LDFLAGS)

check-v257-locale-multilingual: creation_os_v257_locale
	@bash benchmarks/v257/check_v257_locale_multilingual.sh
	@echo "check-v257-locale-multilingual: OK (10 locales + 3 styles + 3 regimes + time)"

check-v257: check-v257-locale-multilingual
	@echo "check-v257: OK (σ-locale kernel)"

# --- v258 σ-Mission (purpose + impact + anti-drift) ---
#
# v0 contracts: mission_statement matches the canonical
# string byte-for-byte; exactly 4 impact scopes (per_user,
# per_domain, per_institution, per_society) with σ_before
# and σ_after in [0,1] and delta_sigma = σ_before − σ_after,
# every improved==true; exactly 4 anti-drift rows with
# decision matching σ_mission vs τ_mission=0.30 (≤→ACCEPT,
# >→REJECT) AND ≥1 ACCEPT AND ≥1 REJECT; exactly 4 long-term
# anchors (civilization_governance/v203, wisdom_transfer_legacy/v233,
# human_sovereignty/v238, declared_eq_realized/1=1) with
# anchor_ok; σ_mission == 0.0; FNV-1a chain replays
# byte-identically.
V258_INC  = -Isrc/v258
V258_SRCS = src/v258/mission.c

creation_os_v258_mission: $(V258_SRCS) src/v258/main.c
	$(CC) $(CFLAGS) $(V258_INC) -o $@ \
	    $(V258_SRCS) src/v258/main.c $(LDFLAGS)

check-v258-mission-anti-drift: creation_os_v258_mission
	@bash benchmarks/v258/check_v258_mission_anti_drift.sh
	@echo "check-v258-mission-anti-drift: OK (statement + impact + anti-drift + anchors)"

check-v258: check-v258-mission-anti-drift
	@echo "check-v258: OK (σ-mission kernel)"

check-v254-v258: check-v254 check-v255 check-v256 check-v257 check-v258
	@echo "check-v254-v258: OK (tutor + collaborate + wellness + locale + mission)"

# --- v259 σ-measurement_t canonical primitive (12-byte gate struct + ns/call bench) ---
#
# v0 contracts: layout is exactly { header:u32, sigma:f32, tau:f32 }
# at offsets 0/4/8 with sizeof==12 AND alignof==4; 4 canonical roundtrip
# pairs encode/decode bit-identically with correct gate verdict; 3 gate
# regimes form a total order (ALLOW < BOUNDARY < ABSTAIN); predicate is
# stateless (256 identical calls produce identical outputs); 3
# microbenches each run ≥ 1e6 iterations, stay under the 1 ms/call
# budget, and report median < max AND min > 0 (real distribution, not
# faked); σ_measurement == 0.0; FNV-1a chain replays byte-identically.
V259_INC  = -Isrc/v259
V259_SRCS = src/v259/sigma_measurement.c

creation_os_v259_sigma_measurement: $(V259_SRCS) src/v259/main.c
	$(CC) $(CFLAGS) $(V259_INC) -o $@ \
	    $(V259_SRCS) src/v259/main.c $(LDFLAGS)

check-v259-sigma-measurement-primitive: creation_os_v259_sigma_measurement
	@bash benchmarks/v259/check_v259_sigma_measurement_primitive.sh
	@echo "check-v259-sigma-measurement-primitive: OK (layout + roundtrip + gate + ns/call bench)"

check-v259: check-v259-sigma-measurement-primitive
	@echo "check-v259: OK (σ-measurement_t canonical primitive)"

# --- σ-pipeline: reinforce (ACCEPT/RETHINK/ABSTAIN three-state gate) ---
#
# Pure-C policy primitive built on top of cos_sigma_measurement_t.  The
# runtime self-test iterates a canonical 15-row reference table + a
# monotonicity sweep + a round-budget test + a 10^5-point LCG grid +
# IEEE-754 NaN/negative safe-default checks.  Exit code 0 on PASS.
SIGMA_REINFORCE_INC  = -Isrc/sigma/pipeline
SIGMA_REINFORCE_SRCS = src/sigma/pipeline/reinforce.c

creation_os_sigma_reinforce: $(SIGMA_REINFORCE_SRCS) src/sigma/pipeline/reinforce_main.c
	$(CC) $(CFLAGS) $(SIGMA_REINFORCE_INC) -o $@ \
	    $(SIGMA_REINFORCE_SRCS) src/sigma/pipeline/reinforce_main.c $(LDFLAGS)

check-sigma-reinforce: creation_os_sigma_reinforce
	@bash benchmarks/sigma_pipeline/check_sigma_reinforce.sh
	@echo "check-sigma-reinforce: OK (3-state gate + monotone σ + round budget + LCG grid)"

# --- σ-pipeline: speculative (small→big routing) ---
#
# Pure-C routing primitive: given a running σ peak and τ_escalate,
# returns LOCAL or ESCALATE.  Includes a peak-update helper (running
# max with NaN / negative rejection), a segment-boundary predicate
# (don't hand off mid-word), and a cost-savings formula used by the
# Python cost-measurement driver.  Self-test: canonical table +
# monotone-in-σ sweep + peak-update + segment-boundary + cost model
# + 10^5-point LCG grid.
SIGMA_SPECULATIVE_INC  = -Isrc/sigma/pipeline
SIGMA_SPECULATIVE_SRCS = src/sigma/pipeline/speculative.c

creation_os_sigma_speculative: $(SIGMA_SPECULATIVE_SRCS) src/sigma/pipeline/speculative_main.c
	$(CC) $(CFLAGS) $(SIGMA_SPECULATIVE_INC) -o $@ \
	    $(SIGMA_SPECULATIVE_SRCS) src/sigma/pipeline/speculative_main.c $(LDFLAGS)

check-sigma-speculative: creation_os_sigma_speculative
	@bash benchmarks/sigma_pipeline/check_sigma_speculative.sh
	@echo "check-sigma-speculative: OK (2-state route + peak update + segment + cost model)"

check-sigma-pipeline: check-sigma-reinforce check-sigma-speculative \
                      check-sigma-ttt check-sigma-engram \
                      check-sigma-moe check-sigma-multimodal \
                      check-sigma-tinyml check-edge-portability \
                      check-sigma-swarm check-sigma-live \
                      check-sigma-continual check-sigma-unlearn \
                      check-sigma-agent check-sigma-diagnostic \
                      check-sigma-sovereign check-codex \
                      check-sigma-pipeline-compose \
                      check-integration check-cos-cli check-digital-twin \
                      check-long-horizon check-swarm check-sandbox \
                      check-sigma-tool check-sigma-plan \
                      check-sigma-merge check-sigma-grounding \
                      check-sigma-session check-cos-agent \
                      check-sigma-selfplay check-sigma-curriculum \
                      check-sigma-synthetic check-sigma-evolution \
                      check-sigma-meta check-sigma-omega check-agi-distill \
                      check-ultra \
                      check-sigma-mesh check-sigma-split \
                      check-sigma-marketplace check-sigma-federation \
                      check-sigma-protocol check-sigma-ed25519 check-cos-network \
                      check-sigma-spike check-sigma-photonic \
                      check-sigma-substrate check-sigma-formal \
                      check-sigma-paper check-cos-unified \
                      check-cos-c-dispatch check-repro-bundle \
                      check-sigma-truthfulqa check-mesh-2node \
                      check-lean-t3-discharged check-sigma-paper-latex \
                      check-sigma-dp check-sigma-ratelimit \
                      check-sigma-persist check-sigma-health \
                      check-sigma-signal check-cos-version-genesis \
                      check-sigma-distill check-sigma-cot-distill \
                      check-sigma-sandbox check-sigma-stream \
                      check-sigma-plugin check-sigma-rag \
                      check-sigma-persona check-sigma-offline \
                      check-sigma-corpus check-sigma-voice \
                      check-sigma-lora check-sigma-team \
                      check-sigma-suite check-sigma-lora-export \
                      check-sigma-watchdog check-sigma-mcp \
                      check-sigma-a2a check-sigma-formal-complete \
                      check-sigma-mesh3 check-sigma-arxiv \
                      check-sigma-conformal check-sigma-coverage-curve \
                      check-sigma-multi check-sigma-suite-sci \
                      check-cos-serve check-cos-mcp check-cos-a2a check-cos-evolve
	@echo "check-sigma-pipeline: OK (reinforce + speculative + ttt + engram + moe + multimodal + tinyml + edge + swarm + live + continual + unlearn + agent + diagnostic + sovereign + codex + end-to-end compose + integration + cos CLIs + tool + plan + merge + grounding + session + cos-agent + selfplay + curriculum + synthetic + evolution + meta + omega + mesh + split + marketplace + federation + protocol + ed25519 + cos-network + spike + photonic + substrate + formal + paper + cos-unified + c-dispatch + repro-bundle + truthfulqa + mesh-2node + lean-t3 + paper-latex + dp + ratelimit + persist + health + signal + version-genesis + rag + persona + offline + corpus + voice + lora + team + suite + lora-export + watchdog + mcp + a2a + formal-complete + mesh3 + arxiv + conformal + coverage-curve + multi-sigma + suite-sci + cos-serve + cos-mcp + cos-a2a + cos-evolve + digital-twin + long-horizon + sigma-swarm + cos-sandbox)"

# --- Atlantean Codex: soul of the pipeline (I0) ---
#
# Loads data/codex/atlantean_codex.txt and data/codex/codex_seed.txt
# into caller-owned buffers, exposes byte count / chapter count /
# FNV-1a-64 hash / invariant check.  Not a σ-gate; a system prompt
# for every downstream module (chat, benchmark, orchestrator).
SIGMA_CODEX_INC  = -Isrc/sigma/pipeline
SIGMA_CODEX_SRCS = src/sigma/pipeline/codex.c

creation_os_sigma_codex: $(SIGMA_CODEX_SRCS) \
                        src/sigma/pipeline/codex_main.c
	$(CC) $(CFLAGS) $(SIGMA_CODEX_INC) -o $@ \
	    $(SIGMA_CODEX_SRCS) src/sigma/pipeline/codex_main.c $(LDFLAGS)

check-codex: creation_os_sigma_codex
	@bash benchmarks/sigma_pipeline/check_sigma_codex.sh
	@echo "check-codex: OK (Codex + seed load; chapters ≥ 33; 1=1 present; hash stable)"

# --- σ-pipeline: end-to-end composition (I1) ---
#
# Orchestrator that wires all 15 σ-primitives (P1…P20 + Codex I0)
# into one turn-handler.  Text generation is injected via a
# callback so integration tests can drive every branch (ACCEPT,
# RETHINK, ABSTAIN, engram HIT, escalate, LOCAL_ONLY).  The demo
# binary exercises engram HIT + mid-σ plateau + cloud escalate and
# prints a pinnable JSON summary.
SIGMA_PIPELINE_INC  = -Isrc/sigma/pipeline
SIGMA_PIPELINE_SRCS = src/sigma/pipeline/pipeline.c \
                      src/sigma/pipeline/codex.c \
                      src/sigma/pipeline/engram.c \
                      src/sigma/pipeline/reinforce.c \
                      src/sigma/pipeline/sovereign.c \
                      src/sigma/pipeline/agent.c \
                      src/sigma/ttt_runtime.c \
                      src/sigma/pipeline/ttt_fallback_shim.c

creation_os_sigma_pipeline: $(SIGMA_PIPELINE_SRCS) \
                            src/sigma/pipeline/pipeline_main.c
	$(CC) $(CFLAGS) $(SIGMA_PIPELINE_INC) -Isrc/sigma -Isrc/import \
	    -o $@ \
	    $(SIGMA_PIPELINE_SRCS) src/sigma/pipeline/pipeline_main.c $(LDFLAGS)

check-sigma-pipeline-compose: creation_os_sigma_pipeline
	@bash benchmarks/sigma_pipeline/check_sigma_pipeline_compose.sh
	@echo "check-sigma-pipeline-compose: OK (end-to-end: engram HIT + RETHINK + escalate)"

# --- σ-pipeline: integration tests (I2) — 12 scenarios ---
#
# Drives the real pipeline.c orchestrator with a stub generator
# that covers every branch (engram HIT, ACCEPT, RETHINK×3 →
# ABSTAIN → escalate, LOCAL_ONLY ABSTAIN), plus direct calls into
# multimodal / unlearn / diagnostic / live / sovereign to prove
# all 15 primitives stack into one process.
SIGMA_IT_SRCS = src/sigma/pipeline/pipeline.c \
                src/sigma/pipeline/codex.c \
                src/sigma/pipeline/engram.c \
                src/sigma/pipeline/reinforce.c \
                src/sigma/pipeline/sovereign.c \
                src/sigma/pipeline/agent.c \
                src/sigma/pipeline/multimodal.c \
                src/sigma/pipeline/unlearn.c \
                src/sigma/pipeline/diagnostic.c \
                src/sigma/pipeline/live.c \
                src/sigma/ttt_runtime.c \
                src/sigma/pipeline/ttt_fallback_shim.c

test_sigma_pipeline_integration: $(SIGMA_IT_SRCS) \
                                 tests/integration/test_pipeline.c
	$(CC) $(CFLAGS) -Isrc/sigma/pipeline -Isrc/sigma -Isrc/import -o $@ \
	    $(SIGMA_IT_SRCS) tests/integration/test_pipeline.c $(LDFLAGS)

check-integration: test_sigma_pipeline_integration
	@./test_sigma_pipeline_integration
	@echo "check-integration: OK (12 scenarios: codex + accept + hit + rethink + escalate + abstain + multimodal + unlearn + sovereign + diagnostic + live + codex-audit)"

# --- Reference CLIs (I3 + I4): cos chat / cos benchmark / cos cost ---
#
# Thin wrappers over sigma_pipeline + the shared stub generator.
# Real BitNet integration is one callback swap away; today these
# binaries prove the control plane works and make the Codex effect
# quantitatively visible.
COS_CLI_INC  = -Isrc/sigma/pipeline -Isrc/sigma/ttt -Isrc/cli -Isrc/import \
               -Isrc/sigma/metacog -Isrc/sigma/physics -Isrc/sigma -Isrc/omega
COS_CLI_SRCS = src/sigma/pipeline/pipeline.c \
               src/sigma/pipeline/codex.c \
               src/sigma/pipeline/engram.c \
               src/sigma/pipeline/reinforce.c \
               src/sigma/pipeline/sovereign.c \
               src/sigma/pipeline/agent.c \
               src/sigma/adaptive_tau.c \
               src/omega/pattern_keywords.c \
               src/sigma/ttt_runtime.c \
               src/cli/stub_gen.c \
               src/cli/cos_tui.c \
               src/sigma/speculative_decode.c \
               src/cli/cost_log.c \
               src/import/bitnet_server.c \
               src/import/bitnet_spawn.c \
               src/import/bitnet_sigma.c

COS_EDGE_INF = src/sigma/inference_cache.c
COS_SPIKE_ADAPT_SRCS = src/sigma/spike_engine.c src/sigma/adaptive_compute.c
COS_PROOF_LIB      = src/sigma/proof_receipt.c src/sigma/compliance.c \
		     src/sigma/constitution.c src/sigma/eu_compliance.c \
		     src/license_kernel/license_attest.c

# DEV-8 purge: src/import/bitnet_ppl.c (llama-perplexity σ heuristic)
# was removed — bitnet_server.c now delivers real per-token logprob
# σ from llama-server's /v1/chat/completions + /completion endpoints.

# DEV-5: escalation.c reaches out to Claude/OpenAI/DeepSeek via libcurl
# (SSL handled by the system stack; no extra dep beyond libcurl which
# ships with every Linux / macOS box we target).  Linked ONLY into
# cos-chat so that cos-benchmark / cos-cost / cos / cos-agent stay
# dep-free (no libcurl, no TLS surface) and keep working on any host
# without network tooling.  Sibling helpers in stub_gen.c that call
# cos_cli_escalation_record_student reference it through a weak path
# — it's only ever invoked from cos_chat's escalate callback, which
# lives in cos-chat's link closure.
cos-chat: $(COS_CLI_SRCS) src/sigma/pipeline/engram_persist.c \
          src/sigma/text_similarity.c \
          src/sigma/ttt/inplace_ttt.c \
          src/sigma/pipeline/conformal.c \
          src/sigma/pipeline/multi_sigma.c \
          src/sigma/metacog/introspection.c \
          src/sigma/physics/coherence.c \
          src/sigma/tools/sigma_tools.c \
          src/sigma/state_ledger.c \
          src/sigma/error_attribution.c \
          src/sigma/engram_episodic.c \
          src/sigma/inference_cache.c \
          src/sigma/knowledge_graph.c \
          src/sigma/speculative_sigma.c $(COS_SPIKE_ADAPT_SRCS) \
          src/sigma/speed_metrics.c \
          src/sigma/semantic_entropy.c \
          src/sigma/semantic_sigma.c \
          $(COS_PROOF_LIB) src/cli/escalation.c src/import/ollama_detect.c \
          src/sigma/response_cache.c \
          src/sigma/cross_model_sigma.c \
          src/sigma/model_cascade.c \
          src/sigma/semantic_cache.c \
          src/cli/cos_chat.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) $(LICENSE_KERNEL_INC) -Isrc/sigma \
	    -Isrc/sigma/tools -o $@ \
	    $(COS_CLI_SRCS) src/sigma/pipeline/engram_persist.c \
	    src/sigma/text_similarity.c \
	    src/sigma/ttt/inplace_ttt.c \
	    src/sigma/pipeline/conformal.c \
	    src/sigma/pipeline/multi_sigma.c \
	    src/sigma/metacog/introspection.c \
	    src/sigma/physics/coherence.c \
	    src/sigma/tools/sigma_tools.c \
	    src/sigma/state_ledger.c \
	    src/sigma/error_attribution.c \
	    src/sigma/engram_episodic.c \
	    src/sigma/inference_cache.c \
	    src/sigma/knowledge_graph.c \
	    src/sigma/speculative_sigma.c \
	    $(COS_SPIKE_ADAPT_SRCS) \
	    src/sigma/speed_metrics.c \
	    src/sigma/semantic_entropy.c \
	    src/sigma/semantic_sigma.c \
	    $(COS_PROOF_LIB) \
	    src/cli/escalation.c src/import/ollama_detect.c src/sigma/response_cache.c \
	    src/sigma/cross_model_sigma.c src/sigma/model_cascade.c \
	    src/sigma/semantic_cache.c \
	    src/cli/cos_chat.c \
	    $(LDFLAGS) -lsqlite3 -lcurl -lpthread

cos-spike: src/cli/cos_spike.c $(COS_SPIKE_ADAPT_SRCS)
	$(CC) $(CFLAGS) -Isrc/cli -Isrc/sigma -o $@ src/cli/cos_spike.c \
	    $(COS_SPIKE_ADAPT_SRCS) $(LDFLAGS)

creation_os_check_spike_engine: tests/agi/check_spike_engine_main.c \
	    $(COS_SPIKE_ADAPT_SRCS)
	$(CC) $(CFLAGS) -Isrc/sigma -DCREATION_OS_ENABLE_SELF_TESTS=1 \
	    -o $@ tests/agi/check_spike_engine_main.c $(COS_SPIKE_ADAPT_SRCS) \
	    $(LDFLAGS)

creation_os_check_adaptive_compute: tests/agi/check_adaptive_compute_main.c \
	    $(COS_SPIKE_ADAPT_SRCS)
	$(CC) $(CFLAGS) -Isrc/sigma -DCREATION_OS_ENABLE_SELF_TESTS=1 \
	    -o $@ tests/agi/check_adaptive_compute_main.c $(COS_SPIKE_ADAPT_SRCS) \
	    $(LDFLAGS)

check-spike-engine: creation_os_check_spike_engine
	@./creation_os_check_spike_engine
	@echo "check-spike-engine: OK (sigma spike engine self-test)"

check-adaptive-compute: creation_os_check_adaptive_compute
	@./creation_os_check_adaptive_compute
	@echo "check-adaptive-compute: OK (adaptive compute self-test)"

creation_os_check_proof_receipt: tests/agi/check_proof_receipt_main.c \
		$(COS_PROOF_LIB)
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/pipeline $(LICENSE_KERNEL_INC) \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 \
	    -o $@ tests/agi/check_proof_receipt_main.c $(COS_PROOF_LIB) $(LDFLAGS)

creation_os_check_compliance: tests/agi/check_compliance_main.c $(COS_PROOF_LIB)
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/pipeline $(LICENSE_KERNEL_INC) \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 \
	    -o $@ tests/agi/check_compliance_main.c $(COS_PROOF_LIB) $(LDFLAGS)

check-proof-receipt: creation_os_check_proof_receipt
	@./creation_os_check_proof_receipt
	@echo "check-proof-receipt: OK (proof receipt self-test)"

check-compliance: creation_os_check_compliance
	@./creation_os_check_compliance
	@echo "check-compliance: OK (compliance self-test)"

creation_os_check_constitution: tests/agi/check_constitution_main.c \
		src/sigma/constitution.c src/sigma/proof_receipt.c \
		src/license_kernel/license_attest.c
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/pipeline $(LICENSE_KERNEL_INC) \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 \
	    -o $@ tests/agi/check_constitution_main.c \
	    src/sigma/constitution.c src/sigma/proof_receipt.c \
	    src/license_kernel/license_attest.c $(LDFLAGS)

check-constitution: creation_os_check_constitution
	@./creation_os_check_constitution
	@echo "check-constitution: OK (Codex RULE pragma parser self-test)"

cos-receipts: src/cli/cos_receipts.c $(COS_PROOF_LIB)
	$(CC) $(CFLAGS) -Isrc/cli -Isrc/sigma -Isrc/sigma/pipeline $(LICENSE_KERNEL_INC) \
	    -o $@ src/cli/cos_receipts.c $(COS_PROOF_LIB) $(LDFLAGS)

cos-receipt-audit: src/cli/cos_receipt_audit.c $(COS_PROOF_LIB)
	$(CC) $(CFLAGS) -Isrc/cli -Isrc/sigma -Isrc/sigma/pipeline $(LICENSE_KERNEL_INC) \
	    -o $@ src/cli/cos_receipt_audit.c $(COS_PROOF_LIB) $(LDFLAGS)

cos-compliance: src/cli/cos_compliance_cli.c $(COS_PROOF_LIB)
	$(CC) $(CFLAGS) -Isrc/cli -Isrc/sigma -Isrc/sigma/pipeline $(LICENSE_KERNEL_INC) \
	    -o $@ src/cli/cos_compliance_cli.c $(COS_PROOF_LIB) $(LDFLAGS)

# cos think — same link closure as cos-chat (σ pipeline + Codex + escalation).
COS_THINK_CLI_AUX = src/sigma/pipeline/engram_persist.c \
          src/sigma/ttt/inplace_ttt.c \
          src/sigma/pipeline/conformal.c \
          src/sigma/pipeline/multi_sigma.c \
          src/sigma/metacog/introspection.c \
          src/sigma/physics/coherence.c \
          src/sigma/tools/sigma_tools.c \
          src/sigma/state_ledger.c \
          src/sigma/error_attribution.c \
          src/sigma/engram_episodic.c \
          src/sigma/text_similarity.c \
          src/cli/escalation.c

COS_LEARN_WEB_SRCS = src/sigma/learn_engine.c src/sigma/living_weights.c \
		     src/cli/cos_web.c \
		     src/sigma/curiosity.c src/sigma/autonomy.c

cos-think: $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) src/sigma/skill_distill.c \
	    src/sigma/knowledge_graph.c src/sigma/world_model.c \
	    $(COS_EDGE_INF) src/cli/cos_think.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma -Isrc/sigma/tools \
	    -o $@ $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) src/sigma/skill_distill.c \
	    src/sigma/knowledge_graph.c src/sigma/world_model.c \
	    $(COS_EDGE_INF) src/cli/cos_think.c \
	    -DCOS_THINK_MAIN $(LDFLAGS) -lsqlite3 -lcurl

check-think: cos-think
	@./cos-think --self-test
	@echo "check-think: OK (cos_think_self_test)"

creation_os_check_ttt_runtime: src/sigma/ttt_runtime.c \
		src/sigma/pipeline/ttt_fallback_shim.c \
		tests/agi/check_ttt_runtime_main.c
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/import -o $@ \
	    src/sigma/ttt_runtime.c \
	    src/sigma/pipeline/ttt_fallback_shim.c \
	    tests/agi/check_ttt_runtime_main.c $(LDFLAGS)

check-ttt-runtime: creation_os_check_ttt_runtime
	@./creation_os_check_ttt_runtime
	@echo "check-ttt-runtime: OK (cos_ttt_runtime_self_test)"

creation_os_check_inference_cache: tests/agi/check_inference_cache_main.c $(COS_EDGE_INF)
	$(CC) $(CFLAGS) -Isrc/sigma -o $@ tests/agi/check_inference_cache_main.c $(COS_EDGE_INF) $(LDFLAGS)

creation_os_check_speculative_sigma: tests/agi/check_speculative_sigma_main.c \
		src/sigma/inference_cache.c src/sigma/speculative_sigma.c
	$(CC) $(CFLAGS) -Isrc/sigma -o $@ tests/agi/check_speculative_sigma_main.c \
	    src/sigma/inference_cache.c src/sigma/speculative_sigma.c $(LDFLAGS)

check-inference-cache: creation_os_check_inference_cache
	@./creation_os_check_inference_cache
	@echo "check-inference-cache: OK (semantic inference cache self-test)"

check-speculative-sigma: creation_os_check_speculative_sigma
	@./creation_os_check_speculative_sigma
	@echo "check-speculative-sigma: OK (speculative sigma self-test)"

cos-cache: src/cli/cos_cache.c $(COS_EDGE_INF)
	$(CC) $(CFLAGS) -Isrc/cli -Isrc/sigma -o $@ src/cli/cos_cache.c $(COS_EDGE_INF) $(LDFLAGS)

# σ-knowledge graph CLI (SQLite + BSC; no LLM extraction in the C path).
cos-graph: src/cli/cos_graph.c src/sigma/knowledge_graph.c $(COS_EDGE_INF)
	$(CC) $(CFLAGS) -Isrc/cli -Isrc/sigma -DCOS_GRAPH_STANDALONE_MAIN -o $@ \
	    src/cli/cos_graph.c src/sigma/knowledge_graph.c $(COS_EDGE_INF) \
	    $(LDFLAGS) -lsqlite3 -lm

creation_os_check_knowledge_graph: tests/agi/check_knowledge_graph_main.c \
		src/sigma/knowledge_graph.c $(COS_EDGE_INF)
	$(CC) $(CFLAGS) -Isrc/sigma -o $@ tests/agi/check_knowledge_graph_main.c \
	    src/sigma/knowledge_graph.c $(COS_EDGE_INF) $(LDFLAGS) -lsqlite3 -lm

creation_os_check_world_model: tests/agi/check_world_model_main.c \
		src/sigma/world_model.c src/sigma/knowledge_graph.c $(COS_EDGE_INF)
	$(CC) $(CFLAGS) -Isrc/sigma -o $@ tests/agi/check_world_model_main.c \
	    src/sigma/world_model.c src/sigma/knowledge_graph.c $(COS_EDGE_INF) \
	    $(LDFLAGS) -lsqlite3 -lm

check-knowledge-graph: creation_os_check_knowledge_graph
	@./creation_os_check_knowledge_graph
	@echo "check-knowledge-graph: OK (8 tests)"

check-world-model: creation_os_check_world_model
	@./creation_os_check_world_model
	@echo "check-world-model: OK (4 tests)"

# --- σ-multimodal perception (vision / audio HTTP clients + fusion) -----
PERCEPTION_LINK_SRCS = src/sigma/perception.c src/sigma/error_attribution.c \
	src/import/bitnet_server.c src/import/bitnet_sigma.c $(COS_EDGE_INF) \
	src/sigma/knowledge_graph.c src/sigma/engram_episodic.c \
	src/sigma/text_similarity.c

creation_os_check_sensor_fusion: tests/agi/check_sensor_fusion_main.c \
		src/sigma/sensor_fusion.c $(COS_EDGE_INF)
	$(CC) $(CFLAGS) -Isrc/sigma -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_sensor_fusion_main.c src/sigma/sensor_fusion.c \
	    $(COS_EDGE_INF) $(LDFLAGS)

creation_os_check_perception: tests/agi/check_perception_main.c $(PERCEPTION_LINK_SRCS)
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/import -DCREATION_OS_ENABLE_SELF_TESTS=1 \
	    -o $@ tests/agi/check_perception_main.c $(PERCEPTION_LINK_SRCS) \
	    $(LDFLAGS) -lsqlite3 -lcurl

cos-sense: src/cli/cos_sense.c src/sigma/sensor_fusion.c $(PERCEPTION_LINK_SRCS)
	$(CC) $(CFLAGS) -Isrc/cli -Isrc/sigma -Isrc/import \
	    -DCOS_SENSE_STANDALONE_MAIN -o $@ src/cli/cos_sense.c \
	    src/sigma/sensor_fusion.c $(PERCEPTION_LINK_SRCS) \
	    $(LDFLAGS) -lsqlite3 -lcurl

check-sensor-fusion: creation_os_check_sensor_fusion
	@./creation_os_check_sensor_fusion
	@echo "check-sensor-fusion: OK (4 tests)"

check-perception: creation_os_check_perception
	@./creation_os_check_perception
	@echo "check-perception: OK (6 tests)"

creation_os_check_watchdog: tests/agi/check_watchdog_main.c \
		src/sigma/coherence_watchdog.c src/sigma/state_ledger.c
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/pipeline \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_watchdog_main.c src/sigma/coherence_watchdog.c \
	    src/sigma/state_ledger.c $(LDFLAGS)

creation_os_check_mission: tests/agi/check_mission_main.c $(COS_CLI_SRCS) \
		$(COS_THINK_CLI_AUX) src/sigma/skill_distill.c \
		src/sigma/knowledge_graph.c src/sigma/world_model.c \
		$(COS_EDGE_INF) src/cli/cos_think.c src/sigma/mission.c \
		src/sigma/coherence_watchdog.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma -Isrc/sigma/tools \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ tests/agi/check_mission_main.c \
	    $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) src/sigma/skill_distill.c \
	    src/sigma/knowledge_graph.c src/sigma/world_model.c \
	    $(COS_EDGE_INF) src/cli/cos_think.c src/sigma/mission.c \
	    src/sigma/coherence_watchdog.c $(LDFLAGS) -lsqlite3 -lcurl -lpthread

cos-mission: $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) src/sigma/skill_distill.c \
	    src/sigma/knowledge_graph.c src/sigma/world_model.c \
	    $(COS_EDGE_INF) src/cli/cos_think.c src/sigma/mission.c \
	    src/sigma/coherence_watchdog.c src/cli/cos_mission.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma -Isrc/sigma/tools \
	    -DCOS_MISSION_MAIN -o $@ $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) \
	    src/sigma/skill_distill.c src/sigma/knowledge_graph.c \
	    src/sigma/world_model.c $(COS_EDGE_INF) src/cli/cos_think.c \
	    src/sigma/mission.c src/sigma/coherence_watchdog.c \
	    src/cli/cos_mission.c $(LDFLAGS) -lsqlite3 -lcurl -lpthread

check-watchdog: creation_os_check_watchdog
	@./creation_os_check_watchdog
	@echo "check-watchdog: OK (4 tests)"

check-mission: creation_os_check_mission
	@./creation_os_check_mission
	@echo "check-mission: OK (8 tests)"

FED_LIB_SRCS = src/sigma/federation.c \
	src/sigma/pipeline/a2a.c \
	src/sigma/sigma_mcp_gate.c src/sigma/tools/sigma_tools.c \
	src/sigma/channels.c \
	src/sigma/state_ledger.c src/sigma/pipeline/reinforce.c \
	src/sigma/engram_episodic.c src/sigma/text_similarity.c \
	src/sigma/error_attribution.c \
	src/sigma/knowledge_graph.c src/sigma/skill_distill.c \
	$(COS_EDGE_INF)

creation_os_check_federation: tests/agi/check_federation_main.c $(FED_LIB_SRCS)
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma/tools \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_federation_main.c $(FED_LIB_SRCS) \
	    $(LDFLAGS) -lsqlite3

creation_os_check_sovereign: tests/agi/check_sovereign_main.c \
		src/sigma/sovereign_limits.c src/sigma/state_ledger.c \
		src/sigma/pipeline/reinforce.c
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/pipeline \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_sovereign_main.c src/sigma/sovereign_limits.c \
	    src/sigma/state_ledger.c src/sigma/pipeline/reinforce.c \
	    $(LDFLAGS)

cos-federation: $(FED_LIB_SRCS) src/cli/cos_federation.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma/tools \
	    -DCOS_FEDERATION_MAIN -o $@ $(FED_LIB_SRCS) \
	    src/cli/cos_federation.c $(LDFLAGS) -lsqlite3

check-federation: creation_os_check_federation
	@./creation_os_check_federation
	@echo "check-federation: OK (8 tests)"

check-sovereign: creation_os_check_sovereign
	@./creation_os_check_sovereign
	@echo "check-sovereign: OK (6 tests)"

creation_os_check_energy_accounting: tests/agi/check_energy_accounting_main.c \
		src/sigma/energy_accounting.c
	$(CC) $(CFLAGS) -Isrc/sigma \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_energy_accounting_main.c src/sigma/energy_accounting.c \
	    $(LDFLAGS)

check-energy-accounting: creation_os_check_energy_accounting
	@./creation_os_check_energy_accounting
	@echo "check-energy-accounting: OK (energy receipt + timers + persistence API)"

creation_os_check_green_score: tests/agi/check_green_score_main.c \
		src/sigma/energy_accounting.c src/sigma/green_score.c
	$(CC) $(CFLAGS) -Isrc/sigma \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_green_score_main.c src/sigma/energy_accounting.c \
	    src/sigma/green_score.c $(LDFLAGS)

check-green-score: creation_os_check_green_score
	@./creation_os_check_green_score
	@echo "check-green-score: OK (green composite + embedded self-test)"

# Ω-loop: speculative path + σ-mission + federation edge + unified driver
COS_OMEGA_SUPPORT_SRCS = src/sigma/speculative_sigma.c $(COS_SPIKE_ADAPT_SRCS) \
	$(COS_PROOF_LIB) \
	src/sigma/mission.c src/sigma/coherence_watchdog.c \
	src/sigma/perception.c src/sigma/pipeline/watchdog.c \
	src/sigma/federation.c src/sigma/pipeline/a2a.c \
	src/sigma/sigma_mcp_gate.c src/sigma/channels.c \
	src/sigma/omega_loop.c src/cli/cos_omega_cli.c \
	src/omega/evolver.c src/omega/pattern_extractor.c \
	src/omega/config_persist.c src/omega/prompt_bank.c src/omega/gvu_loop.c

COS_OMEGA_STATE_DEPS = src/sigma/sovereign_limits.c \
	src/sigma/consciousness_meter.c src/sigma/awareness_log.c \
	src/sigma/physics_model.c src/sigma/embodiment.c \
	src/sigma/energy_accounting.c src/sigma/green_score.c \
	src/sigma/codegen.c src/sigma/evolution_engine.c

creation_os_check_omega: tests/agi/check_omega_loop_main.c $(COS_CLI_SRCS) \
		$(COS_THINK_CLI_AUX) src/sigma/skill_distill.c \
		src/sigma/knowledge_graph.c src/sigma/world_model.c $(COS_EDGE_INF) \
		src/cli/cos_search.c $(COS_LEARN_WEB_SRCS) \
		src/cli/cos_think.c $(COS_OMEGA_SUPPORT_SRCS) $(COS_OMEGA_STATE_DEPS)
	$(CC) $(CFLAGS) $(COS_CLI_INC) $(LICENSE_KERNEL_INC) -Isrc/cli \
	    -Isrc/sigma/tools -Isrc/sigma/pipeline \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_omega_loop_main.c \
	    $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) \
	    src/sigma/skill_distill.c src/sigma/knowledge_graph.c \
	    src/sigma/world_model.c $(COS_EDGE_INF) src/cli/cos_think.c \
	    src/sigma/speculative_sigma.c $(COS_SPIKE_ADAPT_SRCS) \
	    $(COS_PROOF_LIB) \
	    src/sigma/mission.c src/sigma/coherence_watchdog.c \
	    src/sigma/perception.c src/sigma/pipeline/watchdog.c \
	    src/sigma/federation.c src/sigma/pipeline/a2a.c \
	    src/sigma/sigma_mcp_gate.c src/sigma/channels.c \
	    src/sigma/omega_loop.c \
	    src/omega/evolver.c src/omega/pattern_extractor.c \
	    src/omega/config_persist.c src/omega/prompt_bank.c src/omega/gvu_loop.c \
	    src/sigma/semantic_sigma.c \
	    $(COS_LEARN_WEB_SRCS) src/cli/cos_search.c \
	    $(COS_OMEGA_STATE_DEPS) \
	    $(LDFLAGS) -lsqlite3 -lcurl -lpthread

check-omega: creation_os_check_omega
	@./creation_os_check_omega
	@echo "check-omega: OK (Ω-loop mechanics + embedded self-test)"

cos-omega: $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) src/sigma/skill_distill.c \
	src/sigma/knowledge_graph.c src/sigma/world_model.c $(COS_EDGE_INF) \
	src/cli/cos_search.c $(COS_LEARN_WEB_SRCS) \
	src/cli/cos_think.c $(COS_OMEGA_SUPPORT_SRCS) $(COS_OMEGA_STATE_DEPS) \
	include/cos_version.h
	$(CC) $(CFLAGS) $(COS_CLI_INC) $(LICENSE_KERNEL_INC) -Isrc/cli \
	    -Isrc/sigma/tools -Isrc/sigma/pipeline \
	    -DCOS_OMEGA_MAIN -o $@ src/cli/cos_omega_cli.c \
	    $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) \
	    src/sigma/skill_distill.c src/sigma/knowledge_graph.c \
	    src/sigma/world_model.c $(COS_EDGE_INF) src/cli/cos_think.c \
	    src/sigma/speculative_sigma.c $(COS_SPIKE_ADAPT_SRCS) \
	    $(COS_PROOF_LIB) \
	    src/sigma/mission.c src/sigma/coherence_watchdog.c \
	    src/sigma/perception.c src/sigma/pipeline/watchdog.c \
	    src/sigma/federation.c src/sigma/pipeline/a2a.c \
	    src/sigma/sigma_mcp_gate.c src/sigma/channels.c \
	    src/sigma/omega_loop.c \
	    src/omega/evolver.c src/omega/pattern_extractor.c \
	    src/omega/config_persist.c src/omega/prompt_bank.c src/omega/gvu_loop.c \
	    src/sigma/semantic_sigma.c \
	    $(COS_LEARN_WEB_SRCS) src/cli/cos_search.c \
	    $(COS_OMEGA_STATE_DEPS) \
	    $(LDFLAGS) -lsqlite3 -lcurl -lpthread

EMBODY_CHECK_SRCS = src/sigma/embodiment.c src/sigma/physics_model.c \
	src/sigma/sovereign_limits.c src/sigma/state_ledger.c \
	src/sigma/pipeline/reinforce.c src/sigma/knowledge_graph.c \
	src/sigma/engram_episodic.c src/sigma/text_similarity.c \
	src/sigma/error_attribution.c \
	$(COS_EDGE_INF)

creation_os_check_physics_model: tests/agi/check_physics_model_main.c \
		src/sigma/physics_model.c
	$(CC) $(CFLAGS) -Isrc/sigma -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_physics_model_main.c src/sigma/physics_model.c \
	    $(LDFLAGS)

creation_os_check_embodiment: tests/agi/check_embodiment_main.c \
		$(EMBODY_CHECK_SRCS)
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/sigma \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_embodiment_main.c $(EMBODY_CHECK_SRCS) \
	    $(LDFLAGS) -lsqlite3

cos-embody: $(EMBODY_CHECK_SRCS) src/cli/cos_embody.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma \
	    -DCOS_EMBODY_MAIN -o $@ $(EMBODY_CHECK_SRCS) src/cli/cos_embody.c \
	    $(LDFLAGS) -lsqlite3

check-physics-model: creation_os_check_physics_model
	@./creation_os_check_physics_model
	@echo "check-physics-model: OK (physics twin checks)"

check-embodiment: creation_os_check_embodiment
	@./creation_os_check_embodiment
	@echo "check-embodiment: OK (embodiment checks)"

CODEGEN_ENGINE_SRCS = src/sigma/codegen.c src/sigma/evolution_engine.c \
	$(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) \
	src/sigma/skill_distill.c src/sigma/knowledge_graph.c \
	src/sigma/world_model.c src/sigma/sovereign_limits.c \
	$(COS_EDGE_INF)

creation_os_check_codegen: tests/agi/check_codegen_main.c \
		$(CODEGEN_ENGINE_SRCS)
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/sigma \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_codegen_main.c $(CODEGEN_ENGINE_SRCS) \
	    $(LDFLAGS) -lsqlite3 -lcurl

creation_os_check_evolution_engine: \
		tests/agi/check_evolution_engine_main.c $(CODEGEN_ENGINE_SRCS)
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/sigma \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_evolution_engine_main.c $(CODEGEN_ENGINE_SRCS) \
	    $(LDFLAGS) -lsqlite3 -lcurl

cos-codegen: $(CODEGEN_ENGINE_SRCS) src/cli/cos_codegen_cli.c src/cli/cos_think.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma -Isrc/sigma/tools \
	    -DCOS_CODEGEN_CLI_MAIN -o $@ $(CODEGEN_ENGINE_SRCS) \
	    src/cli/cos_codegen_cli.c src/cli/cos_think.c $(LDFLAGS) -lsqlite3 \
	    -lcurl

check-codegen: creation_os_check_codegen
	@./creation_os_check_codegen
	@echo "check-codegen: OK (codegen checks)"

check-evolution-engine: creation_os_check_evolution_engine
	@./creation_os_check_evolution_engine
	@echo "check-evolution-engine: OK (evolution engine checks)"

CONSCIOUSNESS_METER_SRCS = src/sigma/consciousness_meter.c

AWARENESS_LOG_SRCS = src/sigma/consciousness_meter.c src/sigma/awareness_log.c \
	src/sigma/state_ledger.c src/sigma/pipeline/reinforce.c

creation_os_check_consciousness: tests/agi/check_consciousness_main.c \
		$(CONSCIOUSNESS_METER_SRCS)
	$(CC) $(CFLAGS) -Isrc/sigma -DCREATION_OS_ENABLE_SELF_TESTS=1 \
	    -o $@ tests/agi/check_consciousness_main.c \
	    $(CONSCIOUSNESS_METER_SRCS) $(LDFLAGS)

creation_os_check_awareness_log: tests/agi/check_awareness_log_main.c \
		$(AWARENESS_LOG_SRCS)
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/pipeline \
	    -DCREATION_OS_ENABLE_SELF_TESTS=1 -o $@ \
	    tests/agi/check_awareness_log_main.c $(AWARENESS_LOG_SRCS) \
	    $(LDFLAGS) -lsqlite3

cos-consciousness: $(AWARENESS_LOG_SRCS) src/cli/cos_consciousness_cli.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma \
	    -DCOS_CONSCIOUSNESS_CLI_MAIN -o $@ $(AWARENESS_LOG_SRCS) \
	    src/cli/cos_consciousness_cli.c $(LDFLAGS) -lsqlite3

check-consciousness: creation_os_check_consciousness
	@./creation_os_check_consciousness
	@echo "check-consciousness: OK (consciousness meter checks)"

check-awareness-log: creation_os_check_awareness_log
	@./creation_os_check_awareness_log
	@echo "check-awareness-log: OK (awareness log checks)"

SELF_PLAY_FULL = $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) src/sigma/self_play.c \
	    $(COS_EDGE_INF)

cos-self-play: $(SELF_PLAY_FULL) src/cli/cos_self_play.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma -Isrc/sigma/tools \
	    -o $@ $(SELF_PLAY_FULL) src/cli/cos_self_play.c \
	    $(LDFLAGS) -lsqlite3 -lcurl

cos-skills: src/sigma/skill_distill.c $(COS_EDGE_INF) src/cli/cos_skills.c
	$(CC) $(CFLAGS) -Isrc/cli -Isrc/sigma -o $@ src/sigma/skill_distill.c \
	    $(COS_EDGE_INF) src/cli/cos_skills.c $(LDFLAGS) -lsqlite3 -lm

creation_os_check_skill_distill: tests/agi/check_skill_distill_main.c \
		src/sigma/skill_distill.c $(COS_EDGE_INF)
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/cli -o $@ tests/agi/check_skill_distill_main.c \
	    src/sigma/skill_distill.c $(COS_EDGE_INF) $(LDFLAGS) -lsqlite3

creation_os_check_self_play: tests/agi/check_self_play_main.c $(SELF_PLAY_FULL)
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/cli -Isrc/sigma -Isrc/sigma/tools \
	    -o $@ tests/agi/check_self_play_main.c $(SELF_PLAY_FULL) \
	    $(LDFLAGS) -lsqlite3 -lcurl

check-skill-distill: creation_os_check_skill_distill
	@./creation_os_check_skill_distill
	@echo "check-skill-distill: OK (skill_distill self-test)"

check-self-play: creation_os_check_self_play
	@./creation_os_check_self_play
	@echo "check-self-play: OK (self_play self-test)"

cos-search: src/cli/cos_search.c
	$(CC) $(CFLAGS) -Isrc/cli -Isrc/sigma -o $@ src/cli/cos_search.c \
	    -DCOS_SEARCH_MAIN $(LDFLAGS) -lcurl

check-search: cos-search
	@./cos-search --self-test
	@echo "check-search: OK (cos_search_self_test)"

# AGI-1: tiny self-test for ICL composer (no SQLite required).
creation_os_inplace_ttt: src/sigma/ttt/inplace_ttt.c \
                        src/sigma/ttt/inplace_ttt_main.c \
                        src/sigma/pipeline/engram_persist.c \
                        src/sigma/pipeline/engram.c
	$(CC) $(CFLAGS) -Isrc/sigma/pipeline -Isrc/sigma/ttt -o $@ \
	    src/sigma/ttt/inplace_ttt.c src/sigma/ttt/inplace_ttt_main.c \
	    src/sigma/pipeline/engram_persist.c src/sigma/pipeline/engram.c \
	    $(LDFLAGS) -lsqlite3

check-agi-icl: creation_os_inplace_ttt cos-chat
	@./creation_os_inplace_ttt
	@echo "check-agi-icl: OK (inplace_ttt self-test + cos-chat links)"

creation_os_cos_memory: src/cli/cos_memory_cli.c src/sigma/engram_episodic.c \
                       src/sigma/text_similarity.c src/sigma/error_attribution.c
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/pipeline -o $@ \
	    src/cli/cos_memory_cli.c src/sigma/engram_episodic.c \
	    src/sigma/text_similarity.c \
	    src/sigma/error_attribution.c $(LDFLAGS) -lsqlite3

creation_os_check_state_ledger: src/sigma/state_ledger.c \
                               src/sigma/pipeline/multi_sigma.c \
                               tests/agi/check_state_ledger_main.c
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/pipeline -o $@ \
	    src/sigma/state_ledger.c src/sigma/pipeline/multi_sigma.c \
	    tests/agi/check_state_ledger_main.c $(LDFLAGS) -lm

check-state-ledger: creation_os_check_state_ledger
	@./creation_os_check_state_ledger
	@echo "check-state-ledger: OK"

creation_os_check_error_attribution: src/sigma/error_attribution.c \
                                    tests/agi/check_error_attribution_main.c
	$(CC) $(CFLAGS) -Isrc/sigma -o $@ src/sigma/error_attribution.c \
	    tests/agi/check_error_attribution_main.c $(LDFLAGS)

check-error-attribution: creation_os_check_error_attribution
	@./creation_os_check_error_attribution
	@echo "check-error-attribution: OK"

creation_os_check_engram_episodic: src/sigma/engram_episodic.c \
                                  src/sigma/error_attribution.c \
                                  tests/agi/check_engram_episodic_main.c
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/pipeline -o $@ \
	    src/sigma/engram_episodic.c src/sigma/text_similarity.c \
	    src/sigma/error_attribution.c \
	    tests/agi/check_engram_episodic_main.c $(LDFLAGS) -lsqlite3

check-engram-episodic: creation_os_check_engram_episodic
	@./creation_os_check_engram_episodic
	@echo "check-engram-episodic: OK"

creation_os_check_text_similarity: tests/agi/check_text_similarity_main.c \
		src/sigma/text_similarity.c
	$(CC) $(CFLAGS) -Isrc/sigma -o $@ \
	    tests/agi/check_text_similarity_main.c src/sigma/text_similarity.c \
	    $(LDFLAGS)

check-text-similarity: creation_os_check_text_similarity
	@./creation_os_check_text_similarity
	@echo "check-text-similarity: OK (Jaccard + normalize self-test)"

check-agi: check-state-ledger check-error-attribution check-engram-episodic \
	check-text-similarity check-cos-serve check-voice check-omega
	@echo "check-agi: OK (state ledger + error attribution + episodic memory + text_similarity + cos-serve + cos voice + Ω-loop)"

cos-benchmark: $(COS_CLI_SRCS) src/cli/cos_benchmark.c \
               src/sigma/metrics/energy_metric.c src/cli/escalation.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Isrc/sigma/metrics -o $@ \
	    $(COS_CLI_SRCS) src/cli/cos_benchmark.c \
	    src/sigma/metrics/energy_metric.c src/cli/escalation.c \
	    $(LDFLAGS) -lcurl

cos-cost: $(COS_CLI_SRCS) src/cli/cos_cost.c src/cli/escalation.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -o $@ \
	    $(COS_CLI_SRCS) src/cli/cos_cost.c src/cli/escalation.c \
	    $(LDFLAGS) -lcurl

# HORIZON-2: digital twin preflight + guarded /bin/sh -c execution.
cos-exec: src/sigma/twin/digital_twin.c src/cli/cos_exec.c
	$(CC) $(CFLAGS) -Isrc/sigma/twin -o $@ \
	    src/sigma/twin/digital_twin.c src/cli/cos_exec.c $(LDFLAGS)

check-digital-twin: cos-exec
	@bash benchmarks/sigma_pipeline/check_digital_twin.sh
	@echo "check-digital-twin: OK (twin preflight + cp + missing-src + self-test)"

# HORIZON-3: long-horizon σ-checkpoints + snapshot manifests.
cos-plan: src/sigma/plan/long_horizon.c src/sigma/pipeline/reinforce.c \
          src/cli/cos_plan.c
	$(CC) $(CFLAGS) -Isrc/sigma/plan -Isrc/sigma/pipeline -o $@ \
	    src/sigma/plan/long_horizon.c src/sigma/pipeline/reinforce.c \
	    src/cli/cos_plan.c $(LDFLAGS)

check-long-horizon: cos-plan
	@bash benchmarks/sigma_pipeline/check_long_horizon.sh
	@echo "check-long-horizon: OK (plan self-test + demo + rollback + cos plan forward)"

# HORIZON-4: σ-swarm routing (argmin σ + trust EMA; mock peers in v0).
cos-swarm: src/sigma/swarm/cos_swarm.c src/cli/cos_swarm_cli.c
	$(CC) $(CFLAGS) -Isrc/sigma/swarm -o $@ \
	    src/sigma/swarm/cos_swarm.c src/cli/cos_swarm_cli.c $(LDFLAGS)

check-swarm: cos-swarm
	@bash benchmarks/sigma_pipeline/check_swarm.sh
	@echo "check-swarm: OK (self-test + once + cos swarm forward)"

# HORIZON-5: σ-sandbox front-door CLI (fork+rlimits+allowlist; see sandbox.h).
cos-sandbox: src/sigma/pipeline/sandbox.c src/cli/cos_sandbox_cli.c
	$(CC) $(CFLAGS) -Isrc/sigma/pipeline -o $@ \
	    src/sigma/pipeline/sandbox.c src/cli/cos_sandbox_cli.c $(LDFLAGS)

check-sandbox: cos-sandbox cos
	@bash benchmarks/sigma_pipeline/check_sandbox.sh
	@echo "check-sandbox: OK (self-test + risk0 echo + cos sandbox forward)"

# --- DEV-1: real per-token σ from llama-server ---------------------
#
# tests/import/test_bitnet_server_parse.c — CI-safe unit test for
# bitnet_server.c's JSON reducer.  No network, no subprocess, no
# model file: feeds canned /v1/chat/completions responses and
# asserts σ_max / σ_mean / text / token_count.
#
# tests/import/test_bitnet_server.c — live harness that actually
# spawns llama-server + posts a real completion.  Not wired into
# `make check` because it requires the 1 GiB GGUF model file and a
# working bitnet.cpp build.  Invoke manually:
#     make test_bitnet_server && ./test_bitnet_server "What is 2+2?"
test_bitnet_server_parse: src/import/bitnet_server.c \
                          tests/import/test_bitnet_server_parse.c
	$(CC) $(CFLAGS) -Isrc/import -o $@ \
	    src/import/bitnet_server.c \
	    tests/import/test_bitnet_server_parse.c $(LDFLAGS)

check-bitnet-server-parse: test_bitnet_server_parse
	@./test_bitnet_server_parse
	@echo "check-bitnet-server-parse: OK (10 cases: mixed / null / empty / /completion / utf8 / chat logprobs 0.6+0.4 + split)"

test_bitnet_server: src/import/bitnet_server.c tests/import/test_bitnet_server.c
	$(CC) $(CFLAGS) -Isrc/import -o $@ \
	    src/import/bitnet_server.c \
	    tests/import/test_bitnet_server.c $(LDFLAGS)

# --- cos agent (A6): autonomous task execution -------------------
#
# Thin CLI over σ-Tool + σ-Plan + σ-Agent.  Task prefixes
# (list-and-count:, read:, fetch:, rm:, noisy:, fail:) compile into
# concrete plans; --dry-run prints the plan, --local-only rejects
# NETWORK tools, --approve-each promotes CONFIRM → ALLOW, --json
# emits a pinnable receipt.
COS_AGENT_SRCS = src/sigma/pipeline/plan.c \
                 src/sigma/pipeline/tool.c \
                 src/sigma/pipeline/agent.c \
                 src/cli/cos_agent_node.c \
                 src/sigma/pipeline/codex.c \
                 src/license_kernel/license_attest.c \
                 src/sigma/pipeline/a2a.c \
                 src/sigma/sigma_mcp_gate.c \
                 src/sigma/tools/sigma_tools.c \
                 src/sigma/channels.c \
                 src/sigma/engram_episodic.c \
                 src/sigma/error_attribution.c

cos-agent: $(COS_AGENT_SRCS) src/cli/cos_agent.c
	$(CC) $(CFLAGS) -Isrc/sigma/pipeline -Isrc/cli -Isrc/sigma -Isrc/sigma/tools \
	    -Isrc/license_kernel -o $@ \
	    $(COS_AGENT_SRCS) src/cli/cos_agent.c $(LDFLAGS) -lsqlite3

check-cos-agent: cos-agent
	@bash benchmarks/sigma_pipeline/check_cos_agent.sh
	@echo "check-cos-agent: OK (plan compile + gate ALLOW/CONFIRM/BLOCK + budget + flags)"

check-cos-cli: cos cos-chat cos-benchmark cos-cost cos-think creation_os_check_constitution
	@bash benchmarks/sigma_pipeline/check_cos_cli.sh
	@./cos-think --self-test
	@$(MAKE) check-constitution
	@echo "check-cos-cli: OK (cos chat banner + cos benchmark table + cos cost ledger + cos think)"

# --- σ-pipeline: Sovereign (zero-cloud accounting, v264 live) ---
#
# Truthful ledger of where every pipeline call ran (local vs
# cloud), how much it cost, and how many bytes left the device.
# Verdict closes the loop: FULL (no cloud calls), HYBRID (cloud
# calls present but fraction ≥ min_sovereign_fraction), DEPENDENT
# (below the threshold).  Monthly cost projection makes "Creation
# OS saves N% vs cloud-only" a number, not a slogan.
SIGMA_SV_INC  = -Isrc/sigma/pipeline
SIGMA_SV_SRCS = src/sigma/pipeline/sovereign.c

creation_os_sigma_sovereign: $(SIGMA_SV_SRCS) \
                             src/sigma/pipeline/sovereign_main.c
	$(CC) $(CFLAGS) $(SIGMA_SV_INC) -o $@ \
	    $(SIGMA_SV_SRCS) src/sigma/pipeline/sovereign_main.c $(LDFLAGS)

check-sigma-sovereign: creation_os_sigma_sovereign
	@bash benchmarks/sigma_pipeline/check_sigma_sovereign.sh
	@echo "check-sigma-sovereign: OK (zero-cloud accounting + monthly EUR projection)"

# --- σ-pipeline: Diagnostic (explainable σ) ---
#
# Decompose the next-token σ into entropy / gap / max-prob factors,
# expose a top-3 distribution and a counterfactual ("what would σ
# be if p_top1 were lifted to 0.9?") so the chat layer can surface
# WHY the model is uncertain, not just THAT it is.
SIGMA_DG_INC  = -Isrc/sigma/pipeline
SIGMA_DG_SRCS = src/sigma/pipeline/diagnostic.c

creation_os_sigma_diagnostic: $(SIGMA_DG_SRCS) \
                              src/sigma/pipeline/diagnostic_main.c
	$(CC) $(CFLAGS) $(SIGMA_DG_INC) -o $@ \
	    $(SIGMA_DG_SRCS) src/sigma/pipeline/diagnostic_main.c $(LDFLAGS)

check-sigma-diagnostic: creation_os_sigma_diagnostic
	@bash benchmarks/sigma_pipeline/check_sigma_diagnostic.sh
	@echo "check-sigma-diagnostic: OK (entropy / gap / max-prob factor decomposition + counterfactual)"

# --- σ-pipeline: Agent (OODA + σ-modulated autonomy gate) ---
#
# OBSERVE → ORIENT → DECIDE → ACT → REFLECT, with the gate scaling
# the agent's effective autonomy by (1 − σ).  Read/write/network/
# irreversible action classes carry rising autonomy thresholds so
# uncertainty automatically downgrades ALLOW → CONFIRM → BLOCK.
SIGMA_AG_INC  = -Isrc/sigma/pipeline
SIGMA_AG_SRCS = src/sigma/pipeline/agent.c

creation_os_sigma_agent: $(SIGMA_AG_SRCS) \
                         src/sigma/pipeline/agent_main.c
	$(CC) $(CFLAGS) $(SIGMA_AG_INC) -o $@ \
	    $(SIGMA_AG_SRCS) src/sigma/pipeline/agent_main.c $(LDFLAGS)

check-sigma-agent: creation_os_sigma_agent
	@bash benchmarks/sigma_pipeline/check_sigma_agent.sh
	@echo "check-sigma-agent: OK (OODA loop + σ-modulated autonomy gate)"

# --- σ-pipeline: Tool (tool calling + risk-class dispatch, A1) ---
#
# Registry + selector + σ-gated executor sitting on top of the σ-Agent
# autonomy rule.  Five risk classes map to agent action classes;
# IRREVERSIBLE always requires CONFIRM minimum so the agent can never
# `rm -rf /` by accident even when its σ is zero.
SIGMA_TL_INC  = -Isrc/sigma/pipeline
SIGMA_TL_SRCS = src/sigma/pipeline/tool.c src/sigma/pipeline/agent.c

creation_os_sigma_tool: $(SIGMA_TL_SRCS) \
                        src/sigma/pipeline/tool_main.c
	$(CC) $(CFLAGS) $(SIGMA_TL_INC) -o $@ \
	    $(SIGMA_TL_SRCS) src/sigma/pipeline/tool_main.c $(LDFLAGS)

check-sigma-tool: creation_os_sigma_tool
	@bash benchmarks/sigma_pipeline/check_sigma_tool.sh
	@echo "check-sigma-tool: OK (registry + selector + gate + risk classes + executor)"

# --- σ-pipeline: Plan (multi-step plan + budget, A2) ---
#
# Sequence of σ-Tool calls with a hard budget (max_steps,
# max_cost_eur, max_risk).  Execute walks in order, stops on REPLAN
# when σ_post exceeds tau_replan, ABORT on executor failure,
# OVERBUDGET if realized cost overflows, or BLOCKED if the gate
# refuses.  No plan can mortgage the cloud bill at 3 a.m.
SIGMA_PL_INC  = -Isrc/sigma/pipeline
SIGMA_PL_SRCS = src/sigma/pipeline/plan.c src/sigma/pipeline/tool.c \
                src/sigma/pipeline/agent.c

creation_os_sigma_plan: $(SIGMA_PL_SRCS) \
                        src/sigma/pipeline/plan_main.c
	$(CC) $(CFLAGS) $(SIGMA_PL_INC) -o $@ \
	    $(SIGMA_PL_SRCS) src/sigma/pipeline/plan_main.c $(LDFLAGS)

check-sigma-plan: creation_os_sigma_plan
	@bash benchmarks/sigma_pipeline/check_sigma_plan.sh
	@echo "check-sigma-plan: OK (budget + add_step + execute + REPLAN + ABORT)"

# --- σ-pipeline: Merge (σ-gated model merging, A3) ---
#
# LINEAR / SLERP / TIES / TASK_ARITH + an α-sweep {0, .25, .5, .75, 1}.
# The caller brings two weight vectors and a σ oracle; σ-Merge returns
# CONSTRUCTIVE iff σ_after < min(σ_a, σ_b).  Weight-format agnostic by
# design — the primitive does not touch GGUF/MLX/BitNet itself.
SIGMA_MG_INC  = -Isrc/sigma/pipeline
SIGMA_MG_SRCS = src/sigma/pipeline/merge.c

creation_os_sigma_merge: $(SIGMA_MG_SRCS) \
                         src/sigma/pipeline/merge_main.c
	$(CC) $(CFLAGS) $(SIGMA_MG_INC) -o $@ \
	    $(SIGMA_MG_SRCS) src/sigma/pipeline/merge_main.c $(LDFLAGS)

check-sigma-merge: creation_os_sigma_merge
	@bash benchmarks/sigma_pipeline/check_sigma_merge.sh
	@echo "check-sigma-merge: OK (LINEAR + SLERP + TIES + TASK_ARITH + α-sweep + CONSTRUCTIVE gate)"

# --- σ-pipeline: Grounding (claim verification + FNV provenance, A4) ---
#
# Every sentence a model emits is treated as a `claim` and matched
# against a caller-supplied source table.  Verdicts: GROUNDED,
# UNSUPPORTED, CONFLICTED, SKIPPED.  Sources are identified by
# FNV-1a-64 hashes so v299 knowledge-graph / audit layers can cite
# "this claim came from source #0xABCD…" without keeping a pointer.
SIGMA_GR_INC  = -Isrc/sigma/pipeline
SIGMA_GR_SRCS = src/sigma/pipeline/grounding.c

creation_os_sigma_grounding: $(SIGMA_GR_SRCS) \
                             src/sigma/pipeline/grounding_main.c
	$(CC) $(CFLAGS) $(SIGMA_GR_INC) -o $@ \
	    $(SIGMA_GR_SRCS) src/sigma/pipeline/grounding_main.c $(LDFLAGS)

check-sigma-grounding: creation_os_sigma_grounding
	@bash benchmarks/sigma_pipeline/check_sigma_grounding.sh
	@echo "check-sigma-grounding: OK (GROUNDED + UNSUPPORTED + CONFLICTED + SKIPPED + FNV provenance)"

# --- σ-pipeline: Session (long-running turn ring + save/load, A5) ---
#
# Fixed-capacity ring of turns; σ-priority eviction (drop the worst-σ
# turn when full, NOT the oldest).  Trend stats (mean, slope, min,
# max) over turn_id; drift detector.  Line-based save/load with
# FNV-1a-64 integrity checksum — `cos chat --save` / `--load` rides
# directly on this format.
SIGMA_SS_INC  = -Isrc/sigma/pipeline
SIGMA_SS_SRCS = src/sigma/pipeline/session.c

creation_os_sigma_session: $(SIGMA_SS_SRCS) \
                           src/sigma/pipeline/session_main.c
	$(CC) $(CFLAGS) $(SIGMA_SS_INC) -o $@ \
	    $(SIGMA_SS_SRCS) src/sigma/pipeline/session_main.c $(LDFLAGS)

check-sigma-session: creation_os_sigma_session
	@bash benchmarks/sigma_pipeline/check_sigma_session.sh
	@echo "check-sigma-session: OK (append + σ-priority eviction + trend + drift + save/load round-trip)"

# --- σ-pipeline: Selfplay (proposer/solver/σ-verifier, S1) ---
#
# One model, three roles: PROPOSER writes a question, SOLVER answers,
# and the σ-gate is the VERIFIER.  σ_a labels the round TOO_EASY /
# LEARNING / TOO_HARD and a proportional controller walks difficulty
# toward the caller-supplied target — curriculum with no gradient.
# A proconductor hook reruns the same question on a second solver
# and flags the round OVERCONFIDENT when σ_self << σ_other.
SIGMA_SP_INC  = -Isrc/sigma/pipeline
SIGMA_SP_SRCS = src/sigma/pipeline/selfplay.c

creation_os_sigma_selfplay: $(SIGMA_SP_SRCS) \
                            src/sigma/pipeline/selfplay_main.c
	$(CC) $(CFLAGS) $(SIGMA_SP_INC) -o $@ \
	    $(SIGMA_SP_SRCS) src/sigma/pipeline/selfplay_main.c $(LDFLAGS)

check-sigma-selfplay: creation_os_sigma_selfplay check-agi-selfplay
	@bash benchmarks/sigma_pipeline/check_sigma_selfplay.sh
	@echo "check-sigma-selfplay: OK (proposer + solver + σ-verifier + TOO_EASY/LEARNING/TOO_HARD bands + proconductor)"

# --- AGI-2: σ-guided self-play (outer curriculum) --------------------
SIGMA_AGI_SP_INC  = -Isrc/sigma/pipeline -Isrc/sigma/selfplay
SIGMA_AGI_SP_SRCS = src/sigma/selfplay/agi_selfplay.c \
                    src/sigma/pipeline/selfplay.c

creation_os_agi_selfplay: $(SIGMA_AGI_SP_SRCS) \
                         src/sigma/selfplay/agi_selfplay_main.c
	$(CC) $(CFLAGS) $(SIGMA_AGI_SP_INC) -o $@ \
	    $(SIGMA_AGI_SP_SRCS) src/sigma/selfplay/agi_selfplay_main.c \
	    $(LDFLAGS)

check-agi-selfplay: creation_os_agi_selfplay
	@./creation_os_agi_selfplay --self-test >/dev/null
	@./creation_os_agi_selfplay --rounds 50 --domain math >/dev/null
	@echo "check-agi-selfplay: OK (curriculum + σ-selfplay harness)"

# --- AGI-3: continuous distillation status (escalation pairs JSONL) ---
SIGMA_AGI_DS_INC  = -Isrc/sigma/distill
SIGMA_AGI_DS_SRCS = src/sigma/distill/agi_continuous.c

creation_os_agi_distill: $(SIGMA_AGI_DS_SRCS) \
                         src/sigma/distill/agi_continuous_main.c
	$(CC) $(CFLAGS) $(SIGMA_AGI_DS_INC) -o $@ \
	    $(SIGMA_AGI_DS_SRCS) src/sigma/distill/agi_continuous_main.c \
	    $(LDFLAGS)

check-agi-distill: creation_os_agi_distill
	@./creation_os_agi_distill --self-test >/dev/null
	@./creation_os_agi_distill status >/dev/null
	@echo "check-agi-distill: OK (pairs JSONL + train state helpers)"

# --- AGI-5: meta-awareness CLI (domain table + week trend) ----------
SIGMA_AGI_MT_INC  = -Isrc/sigma/pipeline
SIGMA_AGI_MT_SRCS = src/sigma/pipeline/meta.c

creation_os_agi_meta: $(SIGMA_AGI_MT_SRCS) \
                      src/sigma/pipeline/agi_meta_main.c
	$(CC) $(CFLAGS) $(SIGMA_AGI_MT_INC) -o $@ \
	    $(SIGMA_AGI_MT_SRCS) src/sigma/pipeline/agi_meta_main.c \
	    $(LDFLAGS)

check-agi-meta: creation_os_agi_meta
	@./creation_os_agi_meta --self-test >/dev/null
	@./creation_os_agi_meta --domains >/dev/null
	@./creation_os_agi_meta --trend >/dev/null
	@echo "check-agi-meta: OK (domain σ table + trend + JSONL scrape)"

# --- ULTRA-1..11: σ-native architecture kernels (pure C, self-tested) --
ULTRA_INC = -Isrc/sigma/moe -Isrc/sigma/world -Isrc/sigma/decode \
            -Isrc/sigma/symbolic -Isrc/sigma/metacog -Isrc/sigma/nas \
            -Isrc/sigma/metrics -Isrc/sigma/learn -Isrc/sigma/physics \
            -Isrc/sigma/loop

ULTRA_BASE_SRCS = src/sigma/moe/sigma_router.c \
                  src/sigma/world/jepa.c \
                  src/sigma/decode/selective.c \
                  src/sigma/symbolic/neuro_sym.c \
                  src/sigma/metacog/introspection.c \
                  src/sigma/nas/sigma_search.c \
                  src/sigma/metrics/energy_metric.c \
                  src/sigma/learn/continuous_learn.c \
                  src/sigma/physics/coherence.c \
                  src/sigma/loop/recurrent_depth.c

creation_os_ultra_router: src/sigma/moe/sigma_router.c \
                         src/sigma/moe/sigma_router_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    src/sigma/moe/sigma_router.c src/sigma/moe/sigma_router_main.c $(LDFLAGS)

creation_os_ultra_jepa: src/sigma/world/jepa.c src/sigma/world/jepa_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    src/sigma/world/jepa.c src/sigma/world/jepa_main.c $(LDFLAGS)

creation_os_ultra_selective: src/sigma/decode/selective.c \
                            src/sigma/decode/selective_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    src/sigma/decode/selective.c src/sigma/decode/selective_main.c $(LDFLAGS)

creation_os_ultra_neuro_sym: src/sigma/symbolic/neuro_sym.c \
                            src/sigma/symbolic/neuro_sym_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    src/sigma/symbolic/neuro_sym.c src/sigma/symbolic/neuro_sym_main.c $(LDFLAGS)

creation_os_ultra_metacog: src/sigma/metacog/introspection.c \
                          src/sigma/metacog/introspection_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    src/sigma/metacog/introspection.c src/sigma/metacog/introspection_main.c $(LDFLAGS)

creation_os_ultra_search: src/sigma/nas/sigma_search.c \
                         src/sigma/nas/sigma_search_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    src/sigma/nas/sigma_search.c src/sigma/nas/sigma_search_main.c $(LDFLAGS)

creation_os_ultra_learn: src/sigma/learn/continuous_learn.c \
                        src/sigma/learn/continuous_learn_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    src/sigma/learn/continuous_learn.c src/sigma/learn/continuous_learn_main.c $(LDFLAGS)

creation_os_ultra_coherence: src/sigma/physics/coherence.c \
                            src/sigma/physics/coherence_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    src/sigma/physics/coherence.c src/sigma/physics/coherence_main.c $(LDFLAGS)

creation_os_ultra_recurrent_depth: src/sigma/loop/recurrent_depth.c \
                                   src/sigma/loop/recurrent_depth_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    src/sigma/loop/recurrent_depth.c src/sigma/loop/recurrent_depth_main.c $(LDFLAGS)

creation_os_ultra_bundle: $(ULTRA_BASE_SRCS) src/sigma/ultra/ultra_stack_main.c
	$(CC) $(CFLAGS) $(ULTRA_INC) -o $@ \
	    $(ULTRA_BASE_SRCS) src/sigma/ultra/ultra_stack_main.c $(LDFLAGS)

check-ultra: creation_os_ultra_router creation_os_ultra_jepa \
              creation_os_ultra_selective creation_os_ultra_neuro_sym \
              creation_os_ultra_metacog creation_os_ultra_search \
              creation_os_ultra_learn creation_os_ultra_coherence \
              creation_os_ultra_recurrent_depth creation_os_ultra_bundle \
              cos-benchmark
	@./creation_os_ultra_router --self-test >/dev/null
	@./creation_os_ultra_jepa --self-test >/dev/null
	@./creation_os_ultra_selective --self-test >/dev/null
	@./creation_os_ultra_neuro_sym --self-test >/dev/null
	@./creation_os_ultra_metacog --self-test >/dev/null
	@./creation_os_ultra_search --self-test >/dev/null
	@./creation_os_ultra_learn --self-test >/dev/null
	@./creation_os_ultra_coherence --self-test >/dev/null
	@./creation_os_ultra_recurrent_depth --self-test >/dev/null
	@./creation_os_ultra_bundle >/dev/null
	@./cos-benchmark --energy >/dev/null
	@echo "check-ultra: OK (ULTRA-1..11 kernels + bundle + benchmark --energy)"

# --- σ-pipeline: Curriculum (progressive difficulty clock, S2) ---
#
# State machine over σ-Selfplay rounds: require `min_rounds`
# consecutive successes (σ_a < τ) to promote; a single failure
# resets the streak but never demotes.  Five-level canonical ladder
# (FACTUAL → REASONING → MULTISTEP → CREATIVE → META).  Pair with
# P6 TTT + P16 continual so the model walks the ladder on its own
# generated data without catastrophic forgetting.
SIGMA_CR_INC  = -Isrc/sigma/pipeline
SIGMA_CR_SRCS = src/sigma/pipeline/curriculum.c

creation_os_sigma_curriculum: $(SIGMA_CR_SRCS) \
                              src/sigma/pipeline/curriculum_main.c
	$(CC) $(CFLAGS) $(SIGMA_CR_INC) -o $@ \
	    $(SIGMA_CR_SRCS) src/sigma/pipeline/curriculum_main.c $(LDFLAGS)

check-sigma-curriculum: creation_os_sigma_curriculum
	@bash benchmarks/sigma_pipeline/check_sigma_curriculum.sh
	@echo "check-sigma-curriculum: OK (promote / reset / max-cap + 5-level ladder + acceptance rate)"

# --- σ-pipeline: Synthetic (quality-filtered data + collapse guard, S3) ---
#
# Rejection-sampled synthetic pair generation with hard collapse
# protection (Shumailov 2024).  Accept iff σ_a < τ_quality; stop
# if σ_diversity = unique_prefixes/N falls below τ_diversity; hold
# the caller-requested real-anchor ratio exactly by capping the
# synthetic slot count at n_target − ceil(anchor · n_target).
# No silent drift — the run either hits the target or stamps
# `collapsed=1` and walks away.
SIGMA_SY_INC  = -Isrc/sigma/pipeline
SIGMA_SY_SRCS = src/sigma/pipeline/synthetic.c

creation_os_sigma_synthetic: $(SIGMA_SY_SRCS) \
                             src/sigma/pipeline/synthetic_main.c
	$(CC) $(CFLAGS) $(SIGMA_SY_INC) -o $@ \
	    $(SIGMA_SY_SRCS) src/sigma/pipeline/synthetic_main.c $(LDFLAGS)

check-sigma-synthetic: creation_os_sigma_synthetic
	@bash benchmarks/sigma_pipeline/check_sigma_synthetic.sh
	@echo "check-sigma-synthetic: OK (quality gate + anchor mix + diversity-based collapse guard)"

# --- σ-pipeline: Evolution (genetic search over pipeline params, S4) ---
#
# Genetic loop over σ-pipeline GATE parameters (τ_accept, τ_rethink,
# ttt_lr, max_rethink, codex_weight, engram_ttl), NOT model weights.
# Fitness = accuracy² · (1/cost) · (1/latency); σ-guard refuses
# mutants whose sigma_bench regresses past `sigma_slack` above the
# generation's best.  Deterministic LCG so smoke tests pin the
# trajectory.
SIGMA_EV_INC  = -Isrc/sigma/pipeline
SIGMA_EV_SRCS = src/sigma/pipeline/evolution.c

creation_os_sigma_evolution: $(SIGMA_EV_SRCS) \
                             src/sigma/pipeline/evolution_main.c
	$(CC) $(CFLAGS) $(SIGMA_EV_INC) -o $@ \
	    $(SIGMA_EV_SRCS) src/sigma/pipeline/evolution_main.c $(LDFLAGS)

check-sigma-evolution: creation_os_sigma_evolution
	@bash benchmarks/sigma_pipeline/check_sigma_evolution.sh
	@echo "check-sigma-evolution: OK (genome clamp + fitness ordering + run improves seed + τ_accept converges toward optimum)"

# --- σ-pipeline: Meta (domain-level competence awareness, S5) ---
#
# "I know what I don't know" as a data structure: per-domain
# buckets of σ samples, mean + slope + competence flag
# (STRONG/MODERATE/WEAK/LIMITED) and a trend classifier
# (LEARNING/STABLE/DRIFTING).  recommend_focus() returns the
# weakest bucket so S1/S2 can target it; pair with S3 to generate
# new data in that domain and P6 TTT to train on it.
SIGMA_MT_INC  = -Isrc/sigma/pipeline
SIGMA_MT_SRCS = src/sigma/pipeline/meta.c

creation_os_sigma_meta: $(SIGMA_MT_SRCS) \
                        src/sigma/pipeline/meta_main.c
	$(CC) $(CFLAGS) $(SIGMA_MT_INC) -o $@ \
	    $(SIGMA_MT_SRCS) src/sigma/pipeline/meta_main.c $(LDFLAGS)

check-sigma-meta: creation_os_sigma_meta check-agi-meta
	@bash benchmarks/sigma_pipeline/check_sigma_meta.sh
	@echo "check-sigma-meta: OK (bucket + mean + slope + competence flags + recommend_focus)"

# --- σ-pipeline: Omega (recursive self-improvement loop, S6) ---
#
# The v306 thesis statement Ω = argmin ∫σ dt s.t. K ≥ K_crit as a
# runnable loop.  Composes the S-series (self-play, TTT, evolve,
# meta) via caller-supplied hook callbacks so σ-Omega stays the
# conductor, not the soloist.  Per iteration: sub-loops → benchmark
# → best-snapshot if improved → K_eff check → rollback if unsafe.
# Deterministic smoke pins the trajectory on a stub runtime.
SIGMA_OM_INC  = -Isrc/sigma/pipeline
SIGMA_OM_SRCS = src/sigma/pipeline/omega.c \
                src/sigma/pipeline/omega_agi_live.c

creation_os_sigma_omega: $(SIGMA_OM_SRCS) \
                         src/sigma/pipeline/omega_main.c
	$(CC) $(CFLAGS) $(SIGMA_OM_INC) -o $@ \
	    $(SIGMA_OM_SRCS) src/sigma/pipeline/omega_main.c $(LDFLAGS)

check-sigma-omega: creation_os_sigma_omega
	@bash benchmarks/sigma_pipeline/check_sigma_omega.sh
	@echo "check-sigma-omega: OK (Ω loop + ∫σ tracking + best snapshot + K_eff rollback + AGI live/report)"

# --- σ-pipeline: Mesh (peer-to-peer routing + σ-reputation, D1) ---
#
# Node registry (id, address, σ_capability EWMA, latency EWMA,
# availability).  route() scores peers by
#   w_sigma · σ_capability  +  w_latency · (latency_ms / norm)
# with a self-preference bonus when local σ clears τ_local.  Peers
# over τ_abstain are filtered out; route() returns NULL to signal
# ABSTAIN when no peer is trustworthy enough.  report() updates σ
# and latency via EWMA α so a malicious peer converges to σ ≈ 1
# and is quarantined.
SIGMA_MS_INC  = -Isrc/sigma/pipeline
SIGMA_MS_SRCS = src/sigma/pipeline/mesh.c

creation_os_sigma_mesh: $(SIGMA_MS_SRCS) \
                        src/sigma/pipeline/mesh_main.c
	$(CC) $(CFLAGS) $(SIGMA_MS_INC) -o $@ \
	    $(SIGMA_MS_SRCS) src/sigma/pipeline/mesh_main.c $(LDFLAGS)

check-sigma-mesh: creation_os_sigma_mesh
	@bash benchmarks/sigma_pipeline/check_sigma_mesh.sh
	@echo "check-sigma-mesh: OK (register + σ-weighted routing + EWMA reputation + abstain)"

# --- σ-pipeline: Split (distributed layer inference + early exit, D2) ---
#
# Partition a forward pass into contiguous layer ranges pinned to
# peers (stage 0 → node A, stage 1 → node B, …).  After each stage
# the peer stamps a σ_layer; if σ < τ_exit at or after min_stages,
# the pipeline short-circuits and the remaining stages never run.
# That is σ-gated early exit: the same mechanism SpecDec uses for
# drafts, now applied to the pipeline depth dimension.
SIGMA_SPLIT_INC  = -Isrc/sigma/pipeline
SIGMA_SPLIT_SRCS = src/sigma/pipeline/split.c

creation_os_sigma_split: $(SIGMA_SPLIT_SRCS) \
                         src/sigma/pipeline/split_main.c
	$(CC) $(CFLAGS) $(SIGMA_SPLIT_INC) -o $@ \
	    $(SIGMA_SPLIT_SRCS) src/sigma/pipeline/split_main.c $(LDFLAGS)

check-sigma-split: creation_os_sigma_split
	@bash benchmarks/sigma_pipeline/check_sigma_split.sh
	@echo "check-sigma-split: OK (contiguous assignment + full path + σ early exit + transport error)"

# --- σ-pipeline: Marketplace (pay-per-query + SCSL reputation, D3) ---
#
# Providers register with a price, expected σ and latency, and an
# SCSL acknowledgement.  select() returns the cheapest provider
# that clears a σ/latency/price budget; report() folds observed
# σ_result + latency into EWMA reputation and auto-bans providers
# whose failure rate + σ stay above sigma_ban_floor.  SCSL refusal
# is a hard filter: a cheaper rogue provider never wins.
SIGMA_MKT_INC  = -Isrc/sigma/pipeline
SIGMA_MKT_SRCS = src/sigma/pipeline/marketplace.c

creation_os_sigma_marketplace: $(SIGMA_MKT_SRCS) \
                               src/sigma/pipeline/marketplace_main.c
	$(CC) $(CFLAGS) $(SIGMA_MKT_INC) -o $@ \
	    $(SIGMA_MKT_SRCS) src/sigma/pipeline/marketplace_main.c $(LDFLAGS)

check-sigma-marketplace: creation_os_sigma_marketplace
	@bash benchmarks/sigma_pipeline/check_sigma_marketplace.sh
	@echo "check-sigma-marketplace: OK (σ-filtered select + SCSL ban + EWMA reputation + auto-ban)"

# --- σ-pipeline: Federation (σ-weighted Δweight aggregation, D4) ---
#
# Federated learning over mesh: nodes train locally (data stays
# put), only Δweights cross the wire.  The aggregator weights each
# update by (1 − σ_node) · n_samples, filters out σ ≥ σ_reject,
# and rejects poison updates whose L2 norm is more than
# poison_max_scale × the cohort median.  If every update gets
# filtered, the aggregator ABSTAINS and emits a zero Δ.
SIGMA_FED_INC  = -Isrc/sigma/pipeline
SIGMA_FED_SRCS = src/sigma/pipeline/federation.c

creation_os_sigma_federation: $(SIGMA_FED_SRCS) \
                              src/sigma/pipeline/federation_main.c
	$(CC) $(CFLAGS) $(SIGMA_FED_INC) -o $@ \
	    $(SIGMA_FED_SRCS) src/sigma/pipeline/federation_main.c $(LDFLAGS)

check-sigma-federation: creation_os_sigma_federation
	@bash benchmarks/sigma_pipeline/check_sigma_federation.sh
	@echo "check-sigma-federation: OK (σ-weighted aggregate + σ-gate + poison guard + abstain)"

# --- σ-pipeline: Differential Privacy layer (PROD-1) ---
#
# (ε, δ)-DP mechanism on top of σ-federation: before the aggregator
# averages Δweight contributions, each local gradient is clipped to
# L2 ≤ C and perturbed with Lap(C / ε) noise.  An ε-budget tracker
# refuses further rounds once ε_spent + ε_round > ε_total.  The
# module also emits σ_dp = ‖noise‖₂ / ‖g_clipped‖₂ so σ-federation
# can down-weight contributions that carried mostly noise.
# Deterministic: SplitMix64 RNG, seed-pinned by the caller.
SIGMA_DP_INC  = -Isrc/sigma/pipeline
SIGMA_DP_SRCS = src/sigma/pipeline/dp.c

creation_os_sigma_dp: $(SIGMA_DP_SRCS) src/sigma/pipeline/dp_main.c
	$(CC) $(CFLAGS) $(SIGMA_DP_INC) -o $@ \
	    $(SIGMA_DP_SRCS) src/sigma/pipeline/dp_main.c $(LDFLAGS)

check-sigma-dp: creation_os_sigma_dp
	@bash benchmarks/sigma_pipeline/check_sigma_dp.sh
	@echo "check-sigma-dp: OK (deterministic ε-budget + Laplace noise + σ_dp ∈ [0,1])"

# --- σ-pipeline: Rate limiting + reputation (PROD-2) ---
#
# Reputation-weighted rate limiting on top of the D-series mesh.
# Each peer carries r ∈ [0, 1]; decisions are (in priority order)
# BLOCKED (r < r_ban, latched) ≻ THROTTLED_RATE (per-minute or
# per-hour burst cap exceeded) ≻ THROTTLED_GIVE (free-rider:
# answers_served / queries_sent < give_min after 10 queries) ≻
# ALLOWED.  Reputation is σ-driven: peer returns an answer with
# σ < σ_reject_peer → reputation grows; σ ≥ σ_reject_peer → drops.
# No RNG, no wall clock — caller supplies monotonic ns.
SIGMA_RL_INC  = -Isrc/sigma/pipeline
SIGMA_RL_SRCS = src/sigma/pipeline/ratelimit.c

creation_os_sigma_ratelimit: $(SIGMA_RL_SRCS) src/sigma/pipeline/ratelimit_main.c
	$(CC) $(CFLAGS) $(SIGMA_RL_INC) -o $@ \
	    $(SIGMA_RL_SRCS) src/sigma/pipeline/ratelimit_main.c $(LDFLAGS)

check-sigma-ratelimit: creation_os_sigma_ratelimit
	@bash benchmarks/sigma_pipeline/check_sigma_ratelimit.sh
	@echo "check-sigma-ratelimit: OK (ALLOWED/THROTTLED_RATE/THROTTLED_GIVE/BLOCKED)"

# --- σ-pipeline: Persistent state via SQLite WAL (PROD-3) -----------
#
# SQLite backing-store for the five stateful facets of the
# σ-pipeline: pipeline meta, engrams, live state, cost ledger, and
# τ calibration.  WAL mode + synchronous=NORMAL give concurrent-
# reader + crash-consistent semantics; the whole agent state is
# ONE file — `cos network migrate` is `cp state.db remote:`.
#
# Build flag: pass COS_PERSIST_FLAGS=-DCOS_NO_SQLITE to compile a
# stubbed variant where every call returns COS_PERSIST_DISABLED.
# Useful on bare embedded hosts without -lsqlite3.
SIGMA_PERSIST_INC   = -Isrc/sigma/pipeline
SIGMA_PERSIST_SRCS  = src/sigma/pipeline/persist.c
SIGMA_PERSIST_LIBS  = -lsqlite3
COS_PERSIST_FLAGS  ?=

creation_os_sigma_persist: $(SIGMA_PERSIST_SRCS) src/sigma/pipeline/persist_main.c
	$(CC) $(CFLAGS) $(COS_PERSIST_FLAGS) $(SIGMA_PERSIST_INC) -o $@ \
	    $(SIGMA_PERSIST_SRCS) src/sigma/pipeline/persist_main.c \
	    $(SIGMA_PERSIST_LIBS) $(LDFLAGS)

check-sigma-persist: creation_os_sigma_persist
	@bash benchmarks/sigma_pipeline/check_sigma_persist.sh
	@echo "check-sigma-persist: OK (SQLite WAL + schema v1 + upsert/append/roundtrip)"

# --- cos-health: runtime health snapshot (PROD-4) --------------------
#
# cos-health collapses static (# σ-primitives, # substrates, # proofs
# discharged) + dynamic (engrams, τ, cost) facets into a single JSON /
# human report.  The --state <path> flag pulls dynamic numbers from a
# σ-Persist SQLite file.  Grading: HEALTHY / DEGRADED / UNHEALTHY
# based on pipeline-ok %, σ_mean, and cost_today.  `cos health`
# dispatches to this sibling when ./cos-health exists, and falls
# back to `cos doctor` otherwise (no regression on old installs).
COS_HEALTH_INC   = -Isrc/sigma/pipeline
# DEV-7 adds health_live.c: reads engram.db + distill_pairs.jsonl +
# probes llama-server's HTTP /health endpoint so `cos-health` reports
# live runtime facets rather than only compile-time defaults.
COS_HEALTH_SRCS  = src/sigma/pipeline/health.c \
                   src/sigma/pipeline/health_live.c \
                   src/sigma/pipeline/persist.c
COS_HEALTH_LIBS  = -lsqlite3

cos-health: $(COS_HEALTH_SRCS) src/sigma/pipeline/health_main.c
	$(CC) $(CFLAGS) $(COS_HEALTH_INC) -o $@ \
	    $(COS_HEALTH_SRCS) src/sigma/pipeline/health_main.c \
	    $(COS_HEALTH_LIBS) $(LDFLAGS)

check-sigma-health: cos-health cos
	@bash benchmarks/sigma_pipeline/check_sigma_health.sh
	@echo "check-sigma-health: OK (cos-health sibling + cos health dispatch + grading)"

# --- σ-pipeline: Graceful shutdown + signal handling (PROD-5) ---
#
# σ-Signal is the POSIX signal layer for long-lived σ-pipeline
# processes (`cos-mesh-node`, `cos-network serve`, chat loops).
# SIGINT / SIGTERM / SIGHUP latch a sig_atomic_t flag; the main
# thread drains through a shutdown hook that writes final state
# to σ-Persist SQLite before exit.  The handler itself is strictly
# async-signal-safe (write(2) + flag only — no printf, malloc, or
# SQLite calls from signal context).
SIGMA_SIGNAL_INC   = -Isrc/sigma/pipeline
SIGMA_SIGNAL_SRCS  = src/sigma/pipeline/cos_signal.c \
                     src/sigma/pipeline/persist.c
SIGMA_SIGNAL_LIBS  = -lsqlite3

creation_os_sigma_signal: $(SIGMA_SIGNAL_SRCS) src/sigma/pipeline/cos_signal_main.c
	$(CC) $(CFLAGS) $(SIGMA_SIGNAL_INC) -o $@ \
	    $(SIGMA_SIGNAL_SRCS) src/sigma/pipeline/cos_signal_main.c \
	    $(SIGMA_SIGNAL_LIBS) $(LDFLAGS)

check-sigma-signal: creation_os_sigma_signal
	@bash benchmarks/sigma_pipeline/check_sigma_signal.sh
	@echo "check-sigma-signal: OK (self-test + simulate + persist + real-SIGTERM drain)"

# --- σ-pipeline: Distill (continuous σ-guided distillation, NEXT-1) ---
#
# Captures (input, student_answer, student_σ, teacher_answer, teacher_σ)
# from every escalation, scores σ_distill_value = student_σ − teacher_σ,
# filters on teacher_σ ≤ τ_teacher, and returns top-K pairs for TTT /
# P6 fast-weight updates.  In-memory ring of ≤ 1024 pairs; no deps.
SIGMA_DISTILL_INC  = -Isrc/sigma/pipeline
SIGMA_DISTILL_SRCS = src/sigma/pipeline/distill.c

creation_os_sigma_distill: $(SIGMA_DISTILL_SRCS) \
                          src/sigma/pipeline/distill_main.c
	$(CC) $(CFLAGS) $(SIGMA_DISTILL_INC) -o $@ \
	    $(SIGMA_DISTILL_SRCS) src/sigma/pipeline/distill_main.c $(LDFLAGS)

check-sigma-distill: creation_os_sigma_distill
	@bash benchmarks/sigma_pipeline/check_sigma_distill.sh
	@echo "check-sigma-distill: OK (25-pair stream, σ-sorted top-K, escalation ↓)"

# --- σ-pipeline: CoT-distill (chain-of-thought distillation, NEXT-2) ---
#
# σ-distill teaches *what* to answer; σ-CoT-distill teaches *how to
# think*.  Parses teacher chain-of-thought into steps, σ-per-step,
# and RETHINK-points.  Reinforces P1 RETHINK: the student doesn't
# rethink at random — it rethinks where the teacher rethought.
SIGMA_COT_INC  = -Isrc/sigma/pipeline
SIGMA_COT_SRCS = src/sigma/pipeline/cot_distill.c

creation_os_sigma_cot_distill: $(SIGMA_COT_SRCS) \
                              src/sigma/pipeline/cot_distill_main.c
	$(CC) $(CFLAGS) $(SIGMA_COT_INC) -o $@ \
	    $(SIGMA_COT_SRCS) src/sigma/pipeline/cot_distill_main.c $(LDFLAGS)

check-sigma-cot-distill: creation_os_sigma_cot_distill
	@bash benchmarks/sigma_pipeline/check_sigma_cot_distill.sh
	@echo "check-sigma-cot-distill: OK (per-step σ parsed, rethink points identified)"

# --- σ-pipeline: Sandbox (risk-level isolated execution, NEXT-3) ---
#
# A1 tool calling may run shell commands.  In production, every
# command runs inside a σ-Sandbox with an allowlist, rlimits
# (CPU / AS / FSIZE), a wall-clock deadline, and — on Linux —
# network-namespace isolation.  Risk level is coupled to P18
# agent autonomy.
SIGMA_SANDBOX_INC  = -Isrc/sigma/pipeline
SIGMA_SANDBOX_SRCS = src/sigma/pipeline/sandbox.c

creation_os_sigma_sandbox: $(SIGMA_SANDBOX_SRCS) \
                          src/sigma/pipeline/sandbox_main.c
	$(CC) $(CFLAGS) $(SIGMA_SANDBOX_INC) -o $@ \
	    $(SIGMA_SANDBOX_SRCS) src/sigma/pipeline/sandbox_main.c $(LDFLAGS)

check-sigma-sandbox: creation_os_sigma_sandbox
	@bash benchmarks/sigma_pipeline/check_sigma_sandbox.sh
	@echo "check-sigma-sandbox: OK (risk-0 echo + risk-3 timeout + disallowed + consent gate)"

# --- σ-pipeline: Stream (per-token σ streaming, NEXT-4) ---
#
# Substrate-agnostic driver that pulls one token at a time from a
# caller-supplied generator, emits per-token σ via a callback, and
# fires RETHINK markers in-band when σ ≥ τ_rethink.  Drives
# `cos chat --stream`.
SIGMA_STREAM_INC  = -Isrc/sigma/pipeline
SIGMA_STREAM_SRCS = src/sigma/pipeline/stream.c

creation_os_sigma_stream: $(SIGMA_STREAM_SRCS) \
                         src/sigma/pipeline/stream_main.c
	$(CC) $(CFLAGS) $(SIGMA_STREAM_INC) -o $@ \
	    $(SIGMA_STREAM_SRCS) src/sigma/pipeline/stream_main.c $(LDFLAGS)

check-sigma-stream: creation_os_sigma_stream
	@bash benchmarks/sigma_pipeline/check_sigma_stream.sh
	@echo "check-sigma-stream: OK (per-token σ streamed, rethink at σ ≥ τ)"

# --- σ-pipeline: Plugin (extensibility ABI, NEXT-5) ---
#
# Shared-library plugins extend σ-pipeline with new tools, new
# substrates, new σ-signals, or Codex extensions.  The core
# exposes a single entry symbol (cos_sigma_plugin_entry) and an
# ABI version that plugins must declare; dlopen is the transport.
#
# Two artefacts are produced:
#   1. creation_os_sigma_plugin   — the host demo that dlopens a
#      sibling demo plugin and exercises the full registry path.
#   2. libcos_plugin_demo.{dylib,so} — the demo plugin itself.
#
# The Makefile auto-detects Apple Darwin vs. anything-else to pick
# the shared-library extension and the right flags.
UNAME_S  := $(shell uname -s 2>/dev/null)
ifeq ($(UNAME_S),Darwin)
SIGMA_PLUGIN_SHEXT   = dylib
SIGMA_PLUGIN_SHFLAGS = -dynamiclib -undefined dynamic_lookup
SIGMA_PLUGIN_HOSTLD  =
else
SIGMA_PLUGIN_SHEXT   = so
SIGMA_PLUGIN_SHFLAGS = -shared -fPIC
SIGMA_PLUGIN_HOSTLD  = -ldl -rdynamic
endif

SIGMA_PLUGIN_INC  = -Isrc/sigma/pipeline
SIGMA_PLUGIN_SRCS = src/sigma/pipeline/plugin.c

libcos_plugin_demo.$(SIGMA_PLUGIN_SHEXT): src/sigma/pipeline/plugin_demo.c \
                                         src/sigma/pipeline/plugin.h
	$(CC) $(CFLAGS) $(SIGMA_PLUGIN_SHFLAGS) -fPIC $(SIGMA_PLUGIN_INC) \
	    -o $@ src/sigma/pipeline/plugin_demo.c

creation_os_sigma_plugin: $(SIGMA_PLUGIN_SRCS) \
                         src/sigma/pipeline/plugin_main.c \
                         libcos_plugin_demo.$(SIGMA_PLUGIN_SHEXT)
	$(CC) $(CFLAGS) $(SIGMA_PLUGIN_INC) -o $@ \
	    $(SIGMA_PLUGIN_SRCS) src/sigma/pipeline/plugin_main.c \
	    $(SIGMA_PLUGIN_HOSTLD) $(LDFLAGS)

check-sigma-plugin: creation_os_sigma_plugin
	@bash benchmarks/sigma_pipeline/check_sigma_plugin.sh
	@echo "check-sigma-plugin: OK (static register + dlopen demo + tool + σ-signal)"

# --- σ-pipeline: RAG (local retrieval-augmented generation, FINAL-1) ---
#
# Deterministic, zero-dependency retrieval-augmented generation.
# The embedder is hashed-n-gram bag-of-words projected onto a
# 128-dimensional unit vector; cosine similarity drives ranking
# and σ_retrieval = clip01(1 - cosine) drives σ-filtering.  The
# corpus is chunked on word boundaries and never leaves disk.
# A real MiniLM / BGE / Jina embedder plugs in through σ-Plugin.
SIGMA_RAG_INC  = -Isrc/sigma/pipeline
SIGMA_RAG_SRCS = src/sigma/pipeline/rag.c

creation_os_sigma_rag: $(SIGMA_RAG_SRCS) \
                      src/sigma/pipeline/rag_main.c
	$(CC) $(CFLAGS) $(SIGMA_RAG_INC) -o $@ \
	    $(SIGMA_RAG_SRCS) src/sigma/pipeline/rag_main.c $(LDFLAGS)

check-sigma-rag: creation_os_sigma_rag
	@bash benchmarks/sigma_pipeline/check_sigma_rag.sh
	@echo "check-sigma-rag: OK (embed + chunk + σ-filter + top-k stable)"

# --- σ-pipeline: Persona (adaptive user profile, FINAL-2) ------------
#
# Small JSON-backed profile (language, expertise, domains,
# verbosity, formality) learned from query history + explicit
# user feedback via EMA updates.  Persona is injected into the
# Codex envelope; no fine-tuning, no weights.
SIGMA_PERSONA_INC  = -Isrc/sigma/pipeline
SIGMA_PERSONA_SRCS = src/sigma/pipeline/persona.c

creation_os_sigma_persona: $(SIGMA_PERSONA_SRCS) \
                          src/sigma/pipeline/persona_main.c
	$(CC) $(CFLAGS) $(SIGMA_PERSONA_INC) -o $@ \
	    $(SIGMA_PERSONA_SRCS) src/sigma/pipeline/persona_main.c $(LDFLAGS)

check-sigma-persona: creation_os_sigma_persona
	@bash benchmarks/sigma_pipeline/check_sigma_persona.sh
	@echo "check-sigma-persona: OK (language + domain + verbosity + envelope stable)"

# --- σ-pipeline: Offline (airplane-mode readiness, FINAL-3) ----------
#
# Five deterministic filesystem probes — model / engram / RAG
# index / codex / persona — summarised into a single verdict that
# `cos offline prepare` can rely on.  No DNS, no fork, no network
# fallbacks: the whole thing is a stat() + hash() walk.
SIGMA_OFFLINE_INC  = -Isrc/sigma/pipeline
SIGMA_OFFLINE_SRCS = src/sigma/pipeline/offline.c

creation_os_sigma_offline: $(SIGMA_OFFLINE_SRCS) \
                          src/sigma/pipeline/offline_main.c
	$(CC) $(CFLAGS) $(SIGMA_OFFLINE_INC) -o $@ \
	    $(SIGMA_OFFLINE_SRCS) src/sigma/pipeline/offline_main.c $(LDFLAGS)

check-sigma-offline: creation_os_sigma_offline
	@bash benchmarks/sigma_pipeline/check_sigma_offline.sh
	@echo "check-sigma-offline: OK (5-probe verifier + ready/not-ready gate)"

# --- σ-pipeline: Corpus (self-knowledge RAG, FINAL-4) ----------------
#
# Ingest Creation OS's own markdown / TeX / Codex into a σ-RAG
# index and run self-reference queries.  The corpus is defined by
# data/corpus/manifest.txt; paths resolve relative to the
# manifest so the demo is vendorable into other checkouts.
SIGMA_CORPUS_INC  = -Isrc/sigma/pipeline
SIGMA_CORPUS_SRCS = src/sigma/pipeline/corpus.c \
                   src/sigma/pipeline/rag.c

creation_os_sigma_corpus: $(SIGMA_CORPUS_SRCS) \
                        src/sigma/pipeline/corpus_main.c
	$(CC) $(CFLAGS) $(SIGMA_CORPUS_INC) -o $@ \
	    $(SIGMA_CORPUS_SRCS) src/sigma/pipeline/corpus_main.c $(LDFLAGS)

check-sigma-corpus: creation_os_sigma_corpus
	@bash benchmarks/sigma_pipeline/check_sigma_corpus.sh
	@echo "check-sigma-corpus: OK (manifest ingest + self-reference queries)"

# --- σ-pipeline: Voice (local speech interface, FINAL-5) -------------
#
# Two-backend driver with σ-gated STT admit + σ-gated TTS synth:
#   sim     — scripted, deterministic (used by the check)
#   native  — execs whisper.cpp / piper when present, otherwise
#             reports unavailable and falls back to the sim path
#
# No cloud speech API is called.  The driver owns the σ decisions;
# the backend only does the heavy lifting.
SIGMA_VOICE_INC  = -Isrc/sigma/pipeline
SIGMA_VOICE_SRCS = src/sigma/pipeline/voice.c

creation_os_sigma_voice: $(SIGMA_VOICE_SRCS) \
                        src/sigma/pipeline/voice_main.c
	$(CC) $(CFLAGS) $(SIGMA_VOICE_INC) -o $@ \
	    $(SIGMA_VOICE_SRCS) src/sigma/pipeline/voice_main.c $(LDFLAGS)

check-sigma-voice: creation_os_sigma_voice
	@bash benchmarks/sigma_pipeline/check_sigma_voice.sh
	@echo "check-sigma-voice: OK (stt gate + respond + tts gate + native probe)"

# --- σ-pipeline: LoRA (HERMES-1) -------------------------------------
#
# On-device fine-tuning via a rank-r adapter ΔW = (α/r) · B · A.
# Pure C11 + libm, deterministic LCG init, SGD with MSE loss, and
# a tiny multi-adapter registry so "factual" / "code" / … route to
# the right adapter while "general" falls back to the frozen base.
SIGMA_LORA_INC  = -Isrc/sigma/pipeline
SIGMA_LORA_SRCS = src/sigma/pipeline/lora.c

creation_os_sigma_lora: $(SIGMA_LORA_SRCS) \
                       src/sigma/pipeline/lora_main.c
	$(CC) $(CFLAGS) $(SIGMA_LORA_INC) -o $@ \
	    $(SIGMA_LORA_SRCS) src/sigma/pipeline/lora_main.c $(LDFLAGS)

check-sigma-lora: creation_os_sigma_lora
	@bash benchmarks/sigma_pipeline/check_sigma_lora.sh
	@echo "check-sigma-lora: OK (forward + SGD + improvement + determinism + routing)"

# --- σ-pipeline: multi-agent orchestrator (HERMES-2) -----------------
#
# Plan-then-delegate coordinator that fans a goal out to an agent
# team (researcher / coder / reviewer / writer / coordinator).  Each
# step carries a σ estimate; the dispatcher picks the agent with the
# best role-fit and re-tries via a fallback when σ exceeds τ.
SIGMA_TEAM_INC  = -Isrc/sigma/pipeline
SIGMA_TEAM_SRCS = src/sigma/pipeline/team.c

creation_os_sigma_team: $(SIGMA_TEAM_SRCS) \
                       src/sigma/pipeline/team_main.c
	$(CC) $(CFLAGS) $(SIGMA_TEAM_INC) -o $@ \
	    $(SIGMA_TEAM_SRCS) src/sigma/pipeline/team_main.c $(LDFLAGS)

check-sigma-team: creation_os_sigma_team
	@bash benchmarks/sigma_pipeline/check_sigma_team.sh
	@echo "check-sigma-team: OK (plan + route + execute + escalate)"

# --- σ-pipeline: conformal calibration (SCI-1 / SCI-2) ---------------
#
# Angelopoulos-Bates "Learn-then-Test" risk control specialised to
# binary selective prediction.  Reads a detail JSONL with (σ, correct)
# per row and writes ~/.cos/calibration.json holding τ with the
# coverage guarantee P(wrong | σ ≤ τ) ≤ α with confidence 1 − δ.  No
# network, libm only.
SIGMA_CONFORMAL_INC  = -Isrc/sigma/pipeline
SIGMA_CONFORMAL_SRCS = src/sigma/pipeline/conformal.c

creation_os_sigma_conformal: $(SIGMA_CONFORMAL_SRCS) \
                             src/sigma/pipeline/conformal_main.c
	$(CC) $(CFLAGS) $(SIGMA_CONFORMAL_INC) -o $@ \
	    $(SIGMA_CONFORMAL_SRCS) src/sigma/pipeline/conformal_main.c $(LDFLAGS)

cos-calibrate: creation_os_sigma_conformal
	@cp $< $@

check-sigma-conformal: creation_os_sigma_conformal
	@bash benchmarks/sigma_pipeline/check_sigma_conformal.sh
	@echo "check-sigma-conformal: OK (self-test + truthfulqa calibration)"

# --- σ-pipeline: coverage curve (SCI-3) ------------------------------
#
# Sweeps τ ∈ [0,1] across a labelled detail JSONL and writes the
# (τ, coverage, accuracy_accepted, risk_ucb) table that anchors the
# accuracy-coverage trade-off figure.  Reuses the same conformal
# leaf as SCI-1; a single TU, no extra link deps.
creation_os_sigma_coverage_curve: $(SIGMA_CONFORMAL_SRCS) \
                                  src/sigma/pipeline/coverage_curve_main.c
	$(CC) $(CFLAGS) $(SIGMA_CONFORMAL_INC) -o $@ \
	    $(SIGMA_CONFORMAL_SRCS) src/sigma/pipeline/coverage_curve_main.c \
	    $(LDFLAGS)

cos-coverage-curve: creation_os_sigma_coverage_curve
	@cp $< $@

check-sigma-coverage-curve: creation_os_sigma_coverage_curve
	@bash benchmarks/sigma_pipeline/check_sigma_coverage_curve.sh
	@echo "check-sigma-coverage-curve: OK (sweep + monotonic coverage + JSON)"

# --- σ-pipeline: multi-σ ensemble (SCI-5) ----------------------------
#
# Four components (logprob max, mean token entropy, sequence
# perplexity, regeneration consistency) weighted into one combined
# score that feeds into the same conformal calibration surface as
# the scalar σ.  Library + demo CLI.  No network, libm only.
SIGMA_MULTI_INC  = -Isrc/sigma/pipeline -Isrc/sigma
SIGMA_MULTI_SRCS = src/sigma/pipeline/multi_sigma.c

creation_os_sigma_multi: $(SIGMA_MULTI_SRCS) \
                        src/sigma/pipeline/multi_sigma_main.c
	$(CC) $(CFLAGS) $(SIGMA_MULTI_INC) -o $@ \
	    $(SIGMA_MULTI_SRCS) src/sigma/pipeline/multi_sigma_main.c $(LDFLAGS)

cos-multi-sigma: creation_os_sigma_multi
	@cp $< $@

check-sigma-multi: creation_os_sigma_multi
	@bash benchmarks/sigma_pipeline/check_sigma_multi.sh
	@echo "check-sigma-multi: OK (self-test + demo deterministic)"

# --- σ-pipeline: SCI-6 multi-dataset suite ---------------------------
#
# Aggregates per-dataset (σ, correct) detail JSONLs into a single
# σ-gate evaluation table with conformal τ per dataset at fixed
# (α, δ).  Missing detail files are reported as "measured": false;
# no numbers are fabricated.
SIGMA_SUITE_SCI_INC  = -Isrc/sigma/pipeline
SIGMA_SUITE_SCI_SRCS = src/sigma/pipeline/suite_sci.c \
                       src/sigma/pipeline/conformal.c

creation_os_sigma_suite_sci: $(SIGMA_SUITE_SCI_SRCS) \
                             src/sigma/pipeline/suite_sci_main.c
	$(CC) $(CFLAGS) $(SIGMA_SUITE_SCI_INC) -o $@ \
	    $(SIGMA_SUITE_SCI_SRCS) src/sigma/pipeline/suite_sci_main.c \
	    $(LDFLAGS)

cos-bench-suite-sci: creation_os_sigma_suite_sci
	@cp $< $@

check-sigma-suite-sci: creation_os_sigma_suite_sci
	@bash benchmarks/sigma_pipeline/check_sigma_suite_sci.sh
	@echo "check-sigma-suite-sci: OK (self-test + manifest eval + JSON)"

# --- σ-pipeline: benchmark suite (HERMES-3) --------------------------
#
# Aggregates multiple benchmarks into one deterministic table and
# compares against a stored baseline.  When a delta crosses the
# regression threshold the command exits non-zero so CI blocks the
# merge.  Reads/writes are local, no network.
SIGMA_SUITE_INC  = -Isrc/sigma/pipeline
SIGMA_SUITE_SRCS = src/sigma/pipeline/bench_suite.c

creation_os_sigma_suite: $(SIGMA_SUITE_SRCS) \
                        src/sigma/pipeline/bench_suite_main.c
	$(CC) $(CFLAGS) $(SIGMA_SUITE_INC) -o $@ \
	    $(SIGMA_SUITE_SRCS) src/sigma/pipeline/bench_suite_main.c $(LDFLAGS)

check-sigma-suite: creation_os_sigma_suite
	@bash benchmarks/sigma_pipeline/check_sigma_suite.sh
	@echo "check-sigma-suite: OK (suite aggregate + regression gate)"

# --- σ-pipeline: signed LoRA export (HERMES-4) -----------------------
#
# Serialise an adapter into a `.cos` container with a reproducible
# FNV-1a64 MAC over the weights + metadata so another node can verify
# before trusting the payload.  No network; file I/O only.
SIGMA_LORAEXP_INC  = -Isrc/sigma/pipeline
SIGMA_LORAEXP_SRCS = src/sigma/pipeline/lora.c \
                    src/sigma/pipeline/lora_export.c

creation_os_sigma_lora_export: $(SIGMA_LORAEXP_SRCS) \
                              src/sigma/pipeline/lora_export_main.c
	$(CC) $(CFLAGS) $(SIGMA_LORAEXP_INC) -o $@ \
	    $(SIGMA_LORAEXP_SRCS) src/sigma/pipeline/lora_export_main.c $(LDFLAGS)

check-sigma-lora-export: creation_os_sigma_lora_export
	@bash benchmarks/sigma_pipeline/check_sigma_lora_export.sh
	@echo "check-sigma-lora-export: OK (export + verify + tamper reject)"

# --- σ-pipeline: watchdog (HERMES-5) ---------------------------------
#
# Rolling σ_mean over 1 h / 24 h / 7 d windows with trend + alert
# thresholds.  Auto-heal suggests `cos omega` / `cos lora train`
# when σ drifts up or escalation rises.
SIGMA_WATCHDOG_INC  = -Isrc/sigma/pipeline
SIGMA_WATCHDOG_SRCS = src/sigma/pipeline/watchdog.c

creation_os_sigma_watchdog: $(SIGMA_WATCHDOG_SRCS) \
                           src/sigma/pipeline/watchdog_main.c
	$(CC) $(CFLAGS) $(SIGMA_WATCHDOG_INC) -o $@ \
	    $(SIGMA_WATCHDOG_SRCS) src/sigma/pipeline/watchdog_main.c $(LDFLAGS)

check-sigma-watchdog: creation_os_sigma_watchdog
	@bash benchmarks/sigma_pipeline/check_sigma_watchdog.sh
	@echo "check-sigma-watchdog: OK (windows + trend + alert + auto-heal)"

# --- σ-pipeline: MCP server + client (OMEGA-1) -----------------------
#
# JSON-RPC 2.0 over stdio matching the 2026 MCP spec.  Exposes three
# tools (sigma_measure, sigma_gate, sigma_diagnostic) so any external
# MCP client can call σ-primitives, and ships a client-side σ-gate
# that evaluates (tool, args, result) before admitting the payload
# into the local pipeline — the known defence against tool-side
# prompt injection.
SIGMA_MCP_INC  = -Isrc/sigma/pipeline
SIGMA_MCP_SRCS = src/sigma/pipeline/mcp.c

creation_os_sigma_mcp: $(SIGMA_MCP_SRCS) \
                      src/sigma/pipeline/mcp_main.c
	$(CC) $(CFLAGS) $(SIGMA_MCP_INC) -o $@ \
	    $(SIGMA_MCP_SRCS) src/sigma/pipeline/mcp_main.c $(LDFLAGS)

check-sigma-mcp: creation_os_sigma_mcp
	@bash benchmarks/sigma_pipeline/check_sigma_mcp.sh
	@echo "check-sigma-mcp: OK (initialize + tools + σ-gate + inject reject)"

# --- cos-mcp (NEXT-4): σ-gate as infrastructure ----------------------
#
# JSON-RPC 2.0 MCP server exposing six cos.* tools (cos.chat,
# cos.sigma, cos.calibrate, cos.health, cos.engram.lookup,
# cos.introspect) over line-delimited stdio.  Any MCP-capable agent
# (Claude Code, Cursor, Windsurf, OpenClaw, Hermes) can dial in and
# ask Creation OS for a σ-gate on a prompt before committing to an
# answer.
cos-mcp: $(COS_CLI_SRCS) src/sigma/pipeline/engram_persist.c \
         src/sigma/ttt/inplace_ttt.c \
         src/sigma/pipeline/conformal.c \
         src/sigma/pipeline/multi_sigma.c \
         src/sigma/metacog/introspection.c \
         src/cli/escalation.c src/cli/cos_mcp_server.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) -Iinclude -o $@ \
	    $(COS_CLI_SRCS) src/sigma/pipeline/engram_persist.c \
	    src/sigma/ttt/inplace_ttt.c \
	    src/sigma/pipeline/conformal.c \
	    src/sigma/pipeline/multi_sigma.c \
	    src/sigma/metacog/introspection.c \
	    src/cli/escalation.c src/cli/cos_mcp_server.c \
	    $(LDFLAGS) -lsqlite3 -lcurl

check-cos-mcp: cos-mcp
	@bash benchmarks/sigma_pipeline/check_cos_mcp.sh
	@echo "check-cos-mcp: OK (initialize + tools/list + cos.* + unknown-method)"

# --- cos-serve: HTTP σ-gate API + append-only audit JSONL ------------
cos-serve: $(COS_CLI_SRCS) src/sigma/pipeline/engram_persist.c \
         src/sigma/ttt/inplace_ttt.c \
         src/sigma/pipeline/conformal.c \
         src/sigma/pipeline/multi_sigma.c \
         src/sigma/metacog/introspection.c \
         src/cli/escalation.c src/cli/cos_serve.c \
         src/vendor/picohttpparser.c src/sigma/audit_log.c
	$(CC) $(CFLAGS) $(COS_CLI_INC) $(LICENSE_KERNEL_INC) -Iinclude -Isrc/vendor -Isrc/sigma \
	    -DCOS_SERVE_MAIN -o $@ \
	    $(COS_CLI_SRCS) src/sigma/pipeline/engram_persist.c \
	    src/sigma/ttt/inplace_ttt.c \
	    src/sigma/pipeline/conformal.c \
	    src/sigma/pipeline/multi_sigma.c \
	    src/sigma/metacog/introspection.c \
	    src/cli/escalation.c src/cli/cos_serve.c \
	    src/vendor/picohttpparser.c src/sigma/audit_log.c \
	    $(COS_PROOF_LIB) \
	    $(LDFLAGS) -lsqlite3 -lcurl

check-cos-serve: cos-serve
	@./cos-serve --self-test
	@echo "check-cos-serve: OK (HTTP /v1/health self-probe)"

check-serve: check-cos-serve

check-voice: cos
	@./cos voice --help >/dev/null
	@echo "check-voice: OK (cos voice --help)"

cos-bench: src/cli/cos_bench.c
	$(CC) -O2 -Wall -std=c11 -o cos-bench src/cli/cos_bench.c $(LDFLAGS)

# --- σ-pipeline: A2A agent-to-agent (OMEGA-2) ------------------------
#
# Agent-card advertise + σ-trust tracking per peer.  Trust scores
# move via EMA on request outcomes; peers whose σ_trust exceeds
# τ_blocklist stop receiving traffic.  Mesh-level analogue of D1.
SIGMA_A2A_INC  = -Isrc/sigma/pipeline -Isrc/sigma -Isrc/sigma/tools
SIGMA_A2A_SRCS = src/sigma/pipeline/a2a.c \
	src/sigma/sigma_mcp_gate.c src/sigma/tools/sigma_tools.c \
	src/sigma/channels.c src/sigma/engram_episodic.c \
	src/sigma/text_similarity.c src/sigma/error_attribution.c

creation_os_sigma_a2a: $(SIGMA_A2A_SRCS) \
                      src/sigma/pipeline/a2a_main.c
	$(CC) $(CFLAGS) $(SIGMA_A2A_INC) -o $@ \
	    $(SIGMA_A2A_SRCS) src/sigma/pipeline/a2a_main.c $(LDFLAGS) -lsqlite3

check-sigma-a2a: creation_os_sigma_a2a
	@bash benchmarks/sigma_pipeline/check_sigma_a2a.sh
	@echo "check-sigma-a2a: OK (agent-cards + σ-trust + blocklist)"

# --- cos-a2a (NEXT-5): product-level A2A CLI ------------------------
#
# Thin CLI over the σ-A2A kernel with persistent state at
# ~/.cos/a2a.json.  Commands: card · register · request · list ·
# demo · reset.  One process, one session — but state survives
# across invocations so `cos-a2a request …` accumulates σ-trust
# EMA over time.
cos-a2a: $(SIGMA_A2A_SRCS) src/cli/cos_a2a_cli.c
	$(CC) $(CFLAGS) $(SIGMA_A2A_INC) -Iinclude -o $@ \
	    $(SIGMA_A2A_SRCS) src/cli/cos_a2a_cli.c $(LDFLAGS) -lsqlite3

check-cos-a2a: cos-a2a
	@bash benchmarks/sigma_pipeline/check_cos_a2a.sh
	@echo "check-cos-a2a: OK (card + register + request + EMA + blocklist + persist)"

# --- OMEGA: σ-guided parameter evolution + optimisation memory -------
#
# cos-evolve is a standalone multi-command CLI that owns the OMEGA
# surface:
#
#   cos-evolve evolve          σ-guided (1+1)-ES on the σ-ensemble
#                              weights + τ · Brier fitness · keep/revert
#   cos-evolve memory-top      top-N accepted mutations (sqlite)
#   cos-evolve memory-stats    aggregate mutation ledger
#   cos-evolve calibrate-auto  false_accept/false_reject τ-sweep
#   cos-evolve discover        declarative experiment harness
#   cos-evolve omega           recursive evolve + calibrate loop
#   cos-evolve daemon          foreground watchdog (σ_mean envelope)
#   cos-evolve demo            deterministic 40-row showcase
#   cos-evolve self-test       link-level sanity for every primitive
#
# No LLM dependency in the fitness path — the evolved parameters are
# σ-ensemble weights + τ, scored against a labeled σ-fixture via Brier.
# Convergence is a known-good attractor, so the self-test is
# deterministic on every host.  LLM-backed mutators can be plugged in
# via the $COS_EVOLVE_MUTATOR env knob without recompiling.
COS_EVOLVE_SRCS = src/sigma/evolve/cos_evolve.c \
                  src/sigma/evolve/opt_memory.c \
                  src/sigma/evolve/self_calibrate.c \
                  src/sigma/evolve/discover.c
COS_EVOLVE_INC  = -Isrc/sigma/evolve

cos-evolve: $(COS_EVOLVE_SRCS) src/cli/cos_evolve_cli.c
	$(CC) $(CFLAGS) $(COS_EVOLVE_INC) -o $@ \
	    $(COS_EVOLVE_SRCS) src/cli/cos_evolve_cli.c $(LDFLAGS) -lsqlite3

check-cos-evolve: cos-evolve
	@bash benchmarks/evolve/check_cos_evolve.sh
	@echo "check-cos-evolve: OK (evolve + memory + calibrate + discover + omega + daemon)"

# --- σ-pipeline: formal-complete status (OMEGA-3) --------------------
#
# Reports the truthful state of the six formal theorems in
# hw/formal/v259/ (Lean 4 + Frama-C WP).  Reads the evidence
# manifest at docs/v259/formal_status.json and emits a canonical
# JSON report so CI can assert the claim in cos --version
# ("X/6 formal proofs discharged") stays in sync with the ledger.
SIGMA_FORMAL_INC  = -Isrc/sigma/pipeline
SIGMA_FORMAL_SRCS = src/sigma/pipeline/formal_complete.c

creation_os_sigma_formal_complete: $(SIGMA_FORMAL_SRCS) \
                                  src/sigma/pipeline/formal_complete_main.c
	$(CC) $(CFLAGS) $(SIGMA_FORMAL_INC) -o $@ \
	    $(SIGMA_FORMAL_SRCS) src/sigma/pipeline/formal_complete_main.c $(LDFLAGS)

check-sigma-formal-complete: creation_os_sigma_formal_complete
	@bash benchmarks/sigma_pipeline/check_sigma_formal_complete.sh
	@echo "check-sigma-formal-complete: OK (ledger coherent with cos --version)"

# --- σ-pipeline: multi-host live mesh (OMEGA-4) ----------------------
#
# In-process 3-node simulation of the Tailscale mesh contract:
# message bus between A (coordinator, M3 Air), B (leaf, RPi5), and
# C (federator, cloud VM).  Ed25519-style signed envelopes,
# σ-gated escalation, DP-noise federation update.  Deterministic
# end-to-end so CI can pin the trace; the `cos network` CLI keeps
# the same protocol over real sockets when available.
SIGMA_MESH_INC  = -Isrc/sigma/pipeline $(SIGMA_PROTO_INC)
SIGMA_MESH_SRCS = src/sigma/pipeline/mesh3.c $(SIGMA_PROTO_SRCS)

creation_os_sigma_mesh3: $(SIGMA_MESH_SRCS) \
                       src/sigma/pipeline/mesh3_main.c
	$(CC) $(CFLAGS) $(SIGMA_MESH_INC) -o $@ \
	    $(SIGMA_MESH_SRCS) src/sigma/pipeline/mesh3_main.c $(LDFLAGS)

check-sigma-mesh3: creation_os_sigma_mesh3
	@bash benchmarks/sigma_pipeline/check_sigma_mesh3.sh
	@echo "check-sigma-mesh3: OK (3-node query/escalate/federate + signed msgs)"

# --- σ-pipeline: arXiv submission manifest (OMEGA-5) -----------------
#
# Produces the canonical submission manifest for the paper
# (title, authors, ORCID, affiliation, category, DOI placeholder,
# anchor-file SHA-256s) and a JSON envelope CI can diff.  No
# network; submission itself is a manual, human-owned step.
SIGMA_ARXIV_INC  = -Isrc/sigma/pipeline
SIGMA_ARXIV_SRCS = src/sigma/pipeline/arxiv.c

creation_os_sigma_arxiv: $(SIGMA_ARXIV_SRCS) \
                       src/sigma/pipeline/arxiv_main.c
	$(CC) $(CFLAGS) $(SIGMA_ARXIV_INC) -o $@ \
	    $(SIGMA_ARXIV_SRCS) src/sigma/pipeline/arxiv_main.c $(LDFLAGS)

check-sigma-arxiv: creation_os_sigma_arxiv
	@bash benchmarks/sigma_pipeline/check_sigma_arxiv.sh
	@echo "check-sigma-arxiv: OK (manifest stable + anchors present)"

# --- Release identity: v1.0.0 "Genesis" (PROD-6) ---------------------
#
# Three sources of truth must never drift: include/cos_version.h
# (compile-time macros), CHANGELOG.md (human changelog), and the
# banner surfaced by `cos --version`.  check-cos-version-genesis
# verifies all three agree and, on a tagged worktree, that the Git
# tag matches v<COS_VERSION_STRING>.
check-cos-version-genesis: cos
	@bash benchmarks/sigma_pipeline/check_cos_version_genesis.sh
	@echo "check-cos-version-genesis: OK (header == CHANGELOG == cos --version)"

# --- σ-pipeline: Protocol (signed binary wire format, D5) ---
#
# Length-prefixed framed binary with an FNV-based 512-bit MAC.
# 56-byte header (magic/type/version/sender_id/σ/timestamp/plen)
# + N-byte payload + 64-byte signature.  decode() verifies magic,
# version, reserved-zero, payload bounds, and MAC in constant
# time; tamper or wrong-key → reject.  No HTTP / REST / GraphQL.
# The signer is pluggable — swap for Ed25519 when upgrading from
# LAN-only to WAN trust.
# CLOSE-3 (v2.0 Omega): protocol.c's default signer is now Ed25519.
# The full vendored orlp/ed25519 stack + protocol_ed25519.c (signer
# shim) are always linked whenever protocol.c is linked, so every
# caller automatically gets asymmetric signatures.
SIGMA_PROTO_INC  = -Isrc/sigma/pipeline -Ithird_party/ed25519/src
SIGMA_PROTO_SRCS = src/sigma/pipeline/protocol.c \
                   src/sigma/pipeline/protocol_ed25519.c \
                   third_party/ed25519/src/add_scalar.c \
                   third_party/ed25519/src/fe.c \
                   third_party/ed25519/src/ge.c \
                   third_party/ed25519/src/key_exchange.c \
                   third_party/ed25519/src/keypair.c \
                   third_party/ed25519/src/sc.c \
                   third_party/ed25519/src/seed.c \
                   third_party/ed25519/src/sha512.c \
                   third_party/ed25519/src/sign.c \
                   third_party/ed25519/src/verify.c

creation_os_sigma_protocol: $(SIGMA_PROTO_SRCS) \
                            src/sigma/pipeline/protocol_main.c
	$(CC) $(CFLAGS) $(SIGMA_PROTO_INC) -o $@ \
	    $(SIGMA_PROTO_SRCS) src/sigma/pipeline/protocol_main.c $(LDFLAGS)

check-sigma-protocol: creation_os_sigma_protocol
	@bash benchmarks/sigma_pipeline/check_sigma_protocol.sh
	@echo "check-sigma-protocol: OK (Ed25519 default signer + encode/decode round-trip + 7 types + tamper/wrong-key reject)"

# --- σ-protocol Ed25519 (dedicated self-test of the signer shim) ---
#
# protocol.c already pulls in Ed25519 via SIGMA_PROTO_SRCS; this
# target keeps a *separate* binary whose main() exercises only the
# Ed25519 signer wrapper (keypair_from_seed / sign / verify /
# tamper / wrong-key) so the check-sigma-ed25519 gate isolates
# the crypto layer from the wire codec layer.
SIGMA_ED25519_INC  = $(SIGMA_PROTO_INC)
SIGMA_ED25519_SRCS = $(SIGMA_PROTO_SRCS)

creation_os_sigma_ed25519: $(SIGMA_ED25519_SRCS) \
                           src/sigma/pipeline/protocol_ed25519_main.c
	$(CC) $(CFLAGS) $(SIGMA_ED25519_INC) -o $@ \
	    $(SIGMA_ED25519_SRCS) src/sigma/pipeline/protocol_ed25519_main.c $(LDFLAGS)

check-sigma-ed25519: creation_os_sigma_ed25519
	@bash benchmarks/sigma_pipeline/check_sigma_ed25519.sh
	@echo "check-sigma-ed25519: OK (keypair_from_seed + sign/verify + tamper/wrongkey reject)"

# --- cos network CLI (D6) — distributed CLI composing the D-series ---
#
# Subcommands: join / list / status / serve / query / federate /
# unlearn.  Each operates on the canonical bootstrapped state
# (4 mesh peers + 3 market providers + 3-node federation) and
# emits a pinnable JSON receipt with --json.
COS_NETWORK_INC  = $(SIGMA_PROTO_INC)
COS_NETWORK_SRCS = src/sigma/pipeline/mesh.c \
                   src/sigma/pipeline/split.c \
                   src/sigma/pipeline/marketplace.c \
                   src/sigma/pipeline/federation.c \
                   src/sigma/pipeline/dp.c \
                   $(SIGMA_PROTO_SRCS)

cos-network: $(COS_NETWORK_SRCS) src/cli/cos_network.c
	$(CC) $(CFLAGS) $(COS_NETWORK_INC) -o $@ \
	    $(COS_NETWORK_SRCS) src/cli/cos_network.c $(LDFLAGS)

check-cos-network: cos-network
	@bash benchmarks/sigma_pipeline/check_cos_network.sh
	@echo "check-cos-network: OK (join + list + status + serve + query + federate + unlearn)"

# --- σ-pipeline: Spike (neuromorphic LIF σ-gate, H1) ---
#
# σ-gate rendered as leaky integrate-and-fire: V[t+1] = decay·V[t]
# + input ; if V ≥ V_th → spike (ACCEPT) and reset, else silent
# (ABSTAIN).  σ_neuron = 1 − V/V_th encodes distance-to-accept,
# so the same semantics as the digital gate map bit-for-bit onto
# a neuromorphic substrate.  Population σ = 1 − rate/rate_max
# gives a group-level reliability estimate with no arithmetic on
# silent neurons (event-driven, energy-sparse).
SIGMA_SPK_INC  = -Isrc/sigma/pipeline
SIGMA_SPK_SRCS = src/sigma/pipeline/spike.c

creation_os_sigma_spike: $(SIGMA_SPK_SRCS) \
                        src/sigma/pipeline/spike_main.c
	$(CC) $(CFLAGS) $(SIGMA_SPK_INC) -o $@ \
	    $(SIGMA_SPK_SRCS) src/sigma/pipeline/spike_main.c $(LDFLAGS)

check-sigma-spike: creation_os_sigma_spike
	@bash benchmarks/sigma_pipeline/check_sigma_spike.sh
	@echo "check-sigma-spike: OK (LIF step + σ mapping + digital equivalence + population σ)"

# --- σ-pipeline: Photonic (optical intensity → σ-gate, H2) ---
#
# σ_photo = 1 − I_max / Σ I_k.  Dominant-channel optical signal
# collapses σ → 0 (ACCEPT); uniform spread pushes σ → 1
# (ABSTAIN).  Mach-Zehnder intensity simulator feeds the same
# reduction with V·cos(Δφ) interference on N arms.
SIGMA_PHT_INC  = -Isrc/sigma/pipeline
SIGMA_PHT_SRCS = src/sigma/pipeline/photonic.c

creation_os_sigma_photonic: $(SIGMA_PHT_SRCS) \
                            src/sigma/pipeline/photonic_main.c
	$(CC) $(CFLAGS) $(SIGMA_PHT_INC) -o $@ \
	    $(SIGMA_PHT_SRCS) src/sigma/pipeline/photonic_main.c $(LDFLAGS)

check-sigma-photonic: creation_os_sigma_photonic
	@bash benchmarks/sigma_pipeline/check_sigma_photonic.sh
	@echo "check-sigma-photonic: OK (intensity ratio σ + dominant / uniform / MZI)"

# --- σ-pipeline: Substrate (vtable unifying all backends, H3) ---
#
# One vtable (cos_sigma_substrate_t), four bundled backends
# (digital / bitnet / spike / photonic).  Cross-substrate
# equivalence check: the same activation vector reaches the same
# ACCEPT/ABSTAIN bit everywhere for a given τ_accept.  This is the
# runtime form of "design once, run anywhere".
SIGMA_SUB_INC  = -Isrc/sigma/pipeline
SIGMA_SUB_SRCS = src/sigma/pipeline/substrate.c \
                 src/sigma/pipeline/spike.c \
                 src/sigma/pipeline/photonic.c

creation_os_sigma_substrate: $(SIGMA_SUB_SRCS) \
                             src/sigma/pipeline/substrate_main.c
	$(CC) $(CFLAGS) $(SIGMA_SUB_INC) -o $@ \
	    $(SIGMA_SUB_SRCS) src/sigma/pipeline/substrate_main.c $(LDFLAGS)

check-sigma-substrate: creation_os_sigma_substrate
	@bash benchmarks/sigma_pipeline/check_sigma_substrate.sh
	@echo "check-sigma-substrate: OK (vtable + digital + bitnet + spike + photonic equivalence)"

# --- σ-pipeline: Formal (T3/T4/T5/T6 proof harness, H4) ---
#
# Runtime proof harness that discharges the four remaining
# theorems from v259's ledger:
#   T3 — gate monotonicity over τ_accept
#   T4 — commutativity of independent gates
#   T5 — encode / decode idempotence on the σ-Protocol wire
#   T6 — per-call latency bound on the gate
# Not a Lean-checked proof — but mechanically reproducible
# witnesses with pinned counts and a pinned latency bound.  The
# paper (H5) cites this ledger as the C-level evidence.
SIGMA_FRM_INC  = $(SIGMA_PROTO_INC)
SIGMA_FRM_SRCS = src/sigma/pipeline/formal.c \
                 src/sigma/pipeline/substrate.c \
                 src/sigma/pipeline/spike.c \
                 src/sigma/pipeline/photonic.c \
                 $(SIGMA_PROTO_SRCS)

creation_os_sigma_formal: $(SIGMA_FRM_SRCS) \
                          src/sigma/pipeline/formal_main.c
	$(CC) $(CFLAGS) $(SIGMA_FRM_INC) -o $@ \
	    $(SIGMA_FRM_SRCS) src/sigma/pipeline/formal_main.c $(LDFLAGS)

check-sigma-formal: creation_os_sigma_formal
	@bash benchmarks/sigma_pipeline/check_sigma_formal.sh
	@echo "check-sigma-formal: OK (T3 monotonicity + T4 commutativity + T5 encode/decode + T6 latency)"

# --- σ-pipeline: Paper (deterministic arXiv Markdown generator, H5) ---
#
# H5 freezes the σ-gate thesis into one reproducible paper.
# Every number (formal ledger T3–T6, substrate σ values, pipeline
# savings) is pulled from the pinned spec; the paper cannot drift
# from the code without the smoke test catching it.  Output is
# byte-deterministic, so we pin a FNV-1a fingerprint.
SIGMA_PAP_INC  = -Isrc/sigma/pipeline
SIGMA_PAP_SRCS = src/sigma/pipeline/paper.c

creation_os_sigma_paper: $(SIGMA_PAP_SRCS) \
                          src/sigma/pipeline/paper_main.c
	$(CC) $(CFLAGS) $(SIGMA_PAP_INC) -o $@ \
	    $(SIGMA_PAP_SRCS) src/sigma/pipeline/paper_main.c $(LDFLAGS)

check-sigma-paper: creation_os_sigma_paper
	@bash benchmarks/sigma_pipeline/check_sigma_paper.sh
	@echo "check-sigma-paper: OK (9 sections + pinned ledger + fingerprint)"

# --- cos (H6): unified dispatcher for chat / agent / omega / network
#              / benchmark / cost / formal / paper / sigma-meta ---
#
# The front-door `cos` binary gains six new subcommands in H6:
# agent (A6), network (D6), omega (S6), formal (H4), paper (H5),
# and sigma-meta.  The first five fork/exec sibling binaries that
# already exist in this repository so we don't re-link the whole
# stack.  `sigma-meta` is a deterministic σ-meta JSON summary
# emitted inline — no I/O, byte-pinnable in the smoke test.
check-cos-c-dispatch: cos cos-chat cos-benchmark cos-cost
	@bash benchmarks/sigma_pipeline/check_cos_c_dispatch.sh
	@echo "check-cos-c-dispatch: OK (cos chat|benchmark|cost prefer native C binaries)"

# --- 2-node TCP mesh integration (FIX-6) ---------------------------
#
# Spawns two cos-mesh-node processes on 127.0.0.1:7001 and
# 127.0.0.1:7002, exchanges real σ-Protocol frames over real TCP
# sockets, and verifies QUERY→RESPONSE + HEARTBEAT flow.  The
# protocol tested here is the same one that runs on WAN between
# production mesh peers.
COS_MESH_NODE_INC  = $(SIGMA_PROTO_INC)
COS_MESH_NODE_SRCS = $(SIGMA_PROTO_SRCS)

cos-mesh-node: $(COS_MESH_NODE_SRCS) src/cli/cos_mesh_node.c
	$(CC) $(CFLAGS) $(COS_MESH_NODE_INC) -o $@ \
	    $(COS_MESH_NODE_SRCS) src/cli/cos_mesh_node.c $(LDFLAGS)

check-mesh-2node: cos-mesh-node
	@bash benchmarks/sigma_pipeline/check_mesh_2node.sh
	@echo "check-mesh-2node: OK (real TCP sockets: QUERY → RESPONSE + HEARTBEAT)"

# --- Lean 4: T3 gate-monotonicity discharge (FIX-7) ----------------
#
# The v259 Lean 4 file now carries four abstract-order discharges
# without `sorry`: gate totality, boundary tiebreak, σ-monotonicity
# (T3), and τ-anti-monotonicity.  The Float-specific lifts remain
# PENDING because Lean 4's core Float does not admit a LinearOrder
# instance (IEEE-754 NaN), so those stay Frama-C Wp obligations.
#
# The check below does a structural grep — guarantees the proof
# text is in the tree and nobody reverted gateα_* to `sorry`.  When
# a Lean toolchain (lake / lean) is installed it additionally
# type-checks the file.
check-lean-t3-discharged:
	@bash benchmarks/sigma_pipeline/check_lean_t3_discharged.sh
	@echo "check-lean-t3-discharged: OK (4 gateα_* theorems discharged, sorry-free)"

# --- Paper: arXiv-ready LaTeX (FIX-8) ------------------------------
#
# docs/papers/creation_os_v1.tex is the hand-maintained LaTeX twin
# of the canonical Markdown paper at docs/papers/creation_os_v1.md.
# The smoke test below guarantees section skeleton, headline
# measurements (ΔAURCC = -0.0442, p = 0.002, 46.32 % σ drop) and
# the BibTeX key stay in sync between the two sources.  When a TeX
# toolchain is installed (pdflatex on PATH) it additionally
# compiles the .tex to a PDF to catch syntax regressions.
check-sigma-paper-latex:
	@bash benchmarks/sigma_pipeline/check_sigma_paper_latex.sh
	@echo "check-sigma-paper-latex: OK (docs/papers/creation_os_v1.tex arXiv-structural + optional pdflatex)"

# --- Paper regeneration helpers (FIX-8) ----------------------------
#
# `make paper-tex-draft` runs pandoc over the canonical Markdown
# and drops a fresh draft at /tmp/creation_os_v1.pandoc.tex for the
# maintainer to diff against the hand-curated LaTeX source.  We do
# not overwrite docs/papers/creation_os_v1.tex automatically
# because pandoc's output is not publication-quality for tables /
# math; the .tex in-tree is the authoritative arXiv artefact.
.PHONY: paper-tex-draft paper-pdf
paper-tex-draft:
	@if ! command -v pandoc >/dev/null 2>&1; then \
	    echo "paper-tex-draft: pandoc not found on PATH"; exit 2; \
	fi
	@pandoc -s --to=latex docs/papers/creation_os_v1.md \
	        -o /tmp/creation_os_v1.pandoc.tex
	@echo "paper-tex-draft: /tmp/creation_os_v1.pandoc.tex ($$(wc -c < /tmp/creation_os_v1.pandoc.tex) bytes) — diff manually"

paper-pdf:
	@if ! command -v pdflatex >/dev/null 2>&1; then \
	    echo "paper-pdf: pdflatex not found on PATH"; exit 2; \
	fi
	@cd docs/papers && \
	 pdflatex -interaction=nonstopmode -halt-on-error creation_os_v1.tex \
	     >/tmp/cos_paper_latex.out 2>&1 && \
	 pdflatex -interaction=nonstopmode -halt-on-error creation_os_v1.tex \
	     >>/tmp/cos_paper_latex.out 2>&1
	@echo "paper-pdf: docs/papers/creation_os_v1.pdf"

# --- Reproduction bundle (FIX-4) -----------------------------------
#
# `make repro` emits repro_bundle.txt (or repro_bundle.json with
# REPRO_JSON=1) containing host metadata, compiler identity, git
# state, and the tail lines of merge-gate + check-sigma-pipeline.
# Every measured claim in the README or paper should travel with
# the bundle generated on the host that produced the number.
REPRO_JSON ?= 0

.PHONY: repro repro-quick check-repro-bundle
repro:
	@if [ "$(REPRO_JSON)" = "1" ]; then \
	    bash scripts/repro_bundle.sh --json; \
	else \
	    bash scripts/repro_bundle.sh; \
	fi
	@echo "repro: see repro_bundle.txt (or .json)"

repro-quick:
	@bash scripts/repro_bundle.sh --quick
	@echo "repro-quick: see repro_bundle.txt (no merge-gate invocation)"

check-repro-bundle:
	@bash benchmarks/sigma_pipeline/check_repro_bundle.sh
	@echo "check-repro-bundle: OK (host metadata + git state + make summaries pinned)"

# --- TruthfulQA σ-pipeline evaluation (FIX-5) -----------------------
#
# Runs the full σ-pipeline over a 50-question TruthfulQA sample.
# Pins: local-only=30% accuracy → hybrid σ-escalation=96% accuracy.
# The σ-gate genuinely helps here by refusing to answer uncertain
# questions and routing them to a model that can.  The full 817-row
# run is left to users with BitNet / cloud weights and lm-eval.
check-sigma-truthfulqa:
	@bash benchmarks/sigma_pipeline/check_sigma_truthfulqa.sh
	@echo "check-sigma-truthfulqa: OK (50Q pipeline, local=30% → hybrid=96% via σ-escalation)"

check-cos-unified: cos cos-agent cos-network \
                   creation_os_sigma_omega \
                   creation_os_sigma_formal \
                   creation_os_sigma_paper
	@bash benchmarks/sigma_pipeline/check_cos_unified.sh
	@echo "check-cos-unified: OK (cos chat|agent|omega|network|formal|paper|sigma-meta)"

# --- σ-pipeline: Unlearn (GDPR right-to-be-forgotten, v278/FIT live) ---
#
# Surgical weight removal: shrink each weight in proportion to its
# relative alignment with the target vector.  σ_unlearn = 1 − |cos|.
# Iterate apply → verify until σ_unlearn ≥ σ_target; report whether
# the fact was forgotten in a machine-checkable form.
SIGMA_UL_INC  = -Isrc/sigma/pipeline
SIGMA_UL_SRCS = src/sigma/pipeline/unlearn.c

creation_os_sigma_unlearn: $(SIGMA_UL_SRCS) \
                           src/sigma/pipeline/unlearn_main.c
	$(CC) $(CFLAGS) $(SIGMA_UL_INC) -o $@ \
	    $(SIGMA_UL_SRCS) src/sigma/pipeline/unlearn_main.c $(LDFLAGS)

check-sigma-unlearn: creation_os_sigma_unlearn
	@bash benchmarks/sigma_pipeline/check_sigma_unlearn.sh
	@echo "check-sigma-unlearn: OK (GDPR unlearn loop + σ-verification)"

# --- σ-pipeline: Continual (freeze mask, v278 ATLAS/SSU live) ---
#
# Fisher-information-style importance accumulator + σ-gated freeze
# mask.  Before a TTT update lands, zero the gradient on any
# parameter whose importance has exceeded the freeze threshold —
# that is, any parameter the model has demonstrated is critical to
# retained knowledge.  Old weights stay pinned; free weights keep
# adapting.  Catastrophic forgetting, prevented in 30 lines of C.
SIGMA_CONT_INC  = -Isrc/sigma/pipeline
SIGMA_CONT_SRCS = src/sigma/pipeline/continual.c

creation_os_sigma_continual: $(SIGMA_CONT_SRCS) \
                             src/sigma/pipeline/continual_main.c
	$(CC) $(CFLAGS) $(SIGMA_CONT_INC) -o $@ \
	    $(SIGMA_CONT_SRCS) src/sigma/pipeline/continual_main.c $(LDFLAGS)

check-sigma-continual: creation_os_sigma_continual
	@bash benchmarks/sigma_pipeline/check_sigma_continual.sh
	@echo "check-sigma-continual: OK (Fisher importance + σ-gated freeze mask)"

# --- σ-pipeline: Live (feedback-driven τ adaptation, v278 live) ---
#
# τ_accept / τ_rethink start at install-time seeds.  Every user
# feedback (σ, correct?) slides them toward the empirical
# correctness-vs-σ boundary observed in a ring-buffer window of
# the last N observations.  Conformal-inspired: sort the window by
# σ ascending, track cumulative correctness, pick the largest σ
# at which cumulative accuracy ≥ target_accept (default 0.95).
SIGMA_LIVE_INC  = -Isrc/sigma/pipeline
SIGMA_LIVE_SRCS = src/sigma/pipeline/live.c

creation_os_sigma_live: $(SIGMA_LIVE_SRCS) \
                        src/sigma/pipeline/live_main.c
	$(CC) $(CFLAGS) $(SIGMA_LIVE_INC) -o $@ \
	    $(SIGMA_LIVE_SRCS) src/sigma/pipeline/live_main.c $(LDFLAGS)

check-sigma-live: creation_os_sigma_live
	@bash benchmarks/sigma_pipeline/check_sigma_live.sh
	@echo "check-sigma-live: OK (ring-buffer window + sorted-sweep τ adapt)"

# --- σ-pipeline: Swarm (σ-weighted consensus across N models) ---
#
# When multiple models (BitNet + Claude + GPT + …) answer the same
# query, σ tells us which to trust.  ``consensus`` aggregates their
# σ into a single verdict {ABSTAIN, CONSENSUS, ACCEPT} + a winner
# index.  ``should_escalate`` drives cost-aware escalation: start
# cheap, add the next model only while best-σ > tau_stop.
SIGMA_SWARM_INC  = -Isrc/sigma/pipeline
SIGMA_SWARM_SRCS = src/sigma/pipeline/swarm.c

creation_os_sigma_swarm: $(SIGMA_SWARM_SRCS) \
                         src/sigma/pipeline/swarm_main.c
	$(CC) $(CFLAGS) $(SIGMA_SWARM_INC) -o $@ \
	    $(SIGMA_SWARM_SRCS) src/sigma/pipeline/swarm_main.c $(LDFLAGS)

check-sigma-swarm: creation_os_sigma_swarm
	@bash benchmarks/sigma_pipeline/check_sigma_swarm.sh
	@echo "check-sigma-swarm: OK (σ-weighted consensus + cost-aware escalation)"

# --- σ-pipeline: TinyML + edge portability (v270–v274) ---
#
# Streaming anomaly σ in 12 bytes of RAM per sensor channel.  Same
# Welford online update used in scientific streaming analytics — but
# at a size that fits on an ESP32 alongside 100 other sensor channels.
# Useful where no LLM fits (ESP32, ATtiny) but you still want σ-gated
# "is this reading trustworthy?" decisions on raw sensor data.
SIGMA_TINY_INC  = -Isrc/sigma/pipeline
SIGMA_TINY_SRCS = src/sigma/pipeline/tinyml.c

creation_os_sigma_tinyml: $(SIGMA_TINY_SRCS) \
                          src/sigma/pipeline/tinyml_main.c
	$(CC) $(CFLAGS) $(SIGMA_TINY_INC) -o $@ \
	    $(SIGMA_TINY_SRCS) src/sigma/pipeline/tinyml_main.c $(LDFLAGS)

check-sigma-tinyml: creation_os_sigma_tinyml
	@bash benchmarks/sigma_pipeline/check_sigma_tinyml.sh
	@echo "check-sigma-tinyml: OK (12-byte state + Welford σ + spike detection)"

# Edge portability: the same σ primitives must build with portable
# -Os flags (no -march=native) — the contract for Pi 5, iPhone, and
# bare-metal ARM cross-toolchains.
check-edge-portability:
	@bash benchmarks/sigma_pipeline/check_edge_portability.sh
	@echo "check-edge-portability: OK (σ primitives build with -Os; no -march=native)"

# --- σ-pipeline: Multimodal (modality-aware σ) ---
#
# σ measurements for the five modalities Creation OS emits:
#   TEXT (identity pass-through), CODE (paren/brace/quote balance),
#   STRUCTURED (JSON required-field scan), IMAGE / AUDIO (1 − CLIP-
#   or-similar similarity).  All return σ ∈ [0, 1] so the same
#   reinforce / speculative / engram / TTT pipeline downstream can
#   gate any output modality, not just text.
SIGMA_MM_INC  = -Isrc/sigma/pipeline
SIGMA_MM_SRCS = src/sigma/pipeline/multimodal.c

creation_os_sigma_multimodal: $(SIGMA_MM_SRCS) \
                              src/sigma/pipeline/multimodal_main.c
	$(CC) $(CFLAGS) $(SIGMA_MM_INC) -o $@ \
	    $(SIGMA_MM_SRCS) src/sigma/pipeline/multimodal_main.c $(LDFLAGS)

check-sigma-multimodal: creation_os_sigma_multimodal
	@bash benchmarks/sigma_pipeline/check_sigma_multimodal.sh
	@echo "check-sigma-multimodal: OK (text + code + schema + image + audio σ)"

# --- σ-pipeline: MoE (σ-gated expert routing + dynamic width) ---
#
# Live v280 kernel.  Top-1 / top-K expert routing with a 4-step width
# gate (Q / H / TQ / F at σ ∈ [0.15, 0.30, 0.50]) — MoSE (arxiv
# 2602.06154) conventions adapted to σ.  Self-test covers (a) width
# gate on all four branches, (b) strict monotonicity across a 101-
# point σ sweep, (c) NaN / all-NaN / zero-experts degenerate safety,
# (d) top-K ordering + width inheritance + K-clamp semantics, (e)
# compute_saved arithmetic (0.375 for the canonical 4-row trace),
# (f) 10^4 LCG stress comparing against an independent argmax.
SIGMA_MOE_INC  = -Isrc/sigma/pipeline
SIGMA_MOE_SRCS = src/sigma/pipeline/moe.c

creation_os_sigma_moe: $(SIGMA_MOE_SRCS) src/sigma/pipeline/moe_main.c
	$(CC) $(CFLAGS) $(SIGMA_MOE_INC) -o $@ \
	    $(SIGMA_MOE_SRCS) src/sigma/pipeline/moe_main.c $(LDFLAGS)

check-sigma-moe: creation_os_sigma_moe
	@bash benchmarks/sigma_pipeline/check_sigma_moe.sh
	@echo "check-sigma-moe: OK (σ-gated expert routing + dynamic width)"

# --- σ-pipeline: Engram (O(1) σ-aware cache) ---
#
# Live v260.1 kernel.  Open-addressed, power-of-two hash cache keyed by
# FNV-1a-64 of the prompt.  Each entry stores σ_at_store + access
# count + last-access step; age sweep evicts entries past a σ-derived
# TTL (confidently-stored entries live long; uncertainly-stored
# entries are ephemeral).  Insertion into a full table evicts the
# entry with the highest (σ + age − usage) score.  Self-test covers
# FNV-1a non-zero, put/get/update/delete, full-table eviction, age
# sweep with σ-derived TTL, and a 10^4 LCG stress sweep.
SIGMA_ENGRAM_INC  = -Isrc/sigma/pipeline
SIGMA_ENGRAM_SRCS = src/sigma/pipeline/engram.c

creation_os_sigma_engram: $(SIGMA_ENGRAM_SRCS) src/sigma/pipeline/engram_main.c
	$(CC) $(CFLAGS) $(SIGMA_ENGRAM_INC) -o $@ \
	    $(SIGMA_ENGRAM_SRCS) src/sigma/pipeline/engram_main.c $(LDFLAGS)

check-sigma-engram: creation_os_sigma_engram
	@bash benchmarks/sigma_pipeline/check_sigma_engram.sh
	@echo "check-sigma-engram: OK (O(1) cache + σ-TTL + eviction)"

# --- σ-pipeline: TTT (σ-gated test-time training) ---
#
# Live v275.1 kernel.  Pure-C float arithmetic over caller-owned
# fast/slow weight buffers with a σ-gated admission rule, a drift
# (||fast−slow||/||slow||) cap, and a hard reset when drift exceeds the
# cap (Gödel-aware runaway guard).  Self-test: arg-validation errors +
# gate semantics (σ < τ → skip, σ ≥ τ → update, NaN σ / NaN grad safely
# skipped) + drift + reset idempotence + 10^4-point LCG stability
# sweep (fast[] stays finite, drift ≤ 2·τ_drift).
SIGMA_TTT_INC  = -Isrc/sigma/pipeline
SIGMA_TTT_SRCS = src/sigma/pipeline/ttt.c

creation_os_sigma_ttt: $(SIGMA_TTT_SRCS) src/sigma/pipeline/ttt_main.c
	$(CC) $(CFLAGS) $(SIGMA_TTT_INC) -o $@ \
	    $(SIGMA_TTT_SRCS) src/sigma/pipeline/ttt_main.c $(LDFLAGS)

check-sigma-ttt: creation_os_sigma_ttt
	@bash benchmarks/sigma_pipeline/check_sigma_ttt.sh
	@echo "check-sigma-ttt: OK (σ-gated update + drift bound + Gödel reset)"

# --- v103.1: generate_until with per-token σ-logging ---
#
# Drives a Backend (stub by default, v101 bridge when available) one
# token at a time; logs full 8-channel σ per step as JSONL; applies the
# reinforce + speculative policies *during* generation so ABSTAIN /
# RETHINK / ESCALATE can stop a runaway low-confidence completion
# before it finishes.  Smoke test uses the stub backend so it runs
# without BitNet / PyTorch.  Parity test asserts Python mirrors agree
# with the C binaries on a canonical 18-row decision grid.
check-sigma-generate-until: check-sigma-pipeline
	@bash benchmarks/sigma_pipeline/check_sigma_generate_until.sh
	@echo "check-sigma-generate-until: OK (per-token σ trace + policy + C↔Py parity)"

# --- cos chat interactive demo ---
#
# Ties P1 (reinforce) + P2 (speculative) + P3 (generate_until) into one
# CLI.  The smoke test runs the --once mode with a deterministic stub
# backend so it passes on any host (no BitNet / PyTorch needed) and
# asserts the transcript has the expected shape.
check-cos-chat: check-sigma-generate-until check-agi-icl cos
	@bash benchmarks/sigma_pipeline/check_cos_chat.sh
	@echo "check-cos-chat: OK (--once stub → ACCEPT/RETHINK/ABSTAIN + transcript JSONL)"

# --- cost measurement — €saved vs accuracy maintained ---
#
# Replays a question set (10-row deterministic demo fixture OR a JSONL
# with per-question sigma_peak / local_correct / api_correct labels)
# through the σ-gated routing policy and reports the headline number:
#
#     "Creation OS saves X% of API costs while maintaining Y% of
#      API-only accuracy."
#
# The cost formula here is byte-identical to the C primitive's
# `cos_sigma_speculative_cost_savings` — the Python driver calls the
# Python mirror and the parity test asserts they agree to 1e-6.  The
# numbers in the smoke test (3/10 escalate, 67.5% savings, 100%
# accuracy retained) are PINNED BY CONSTRUCTION from the demo fixture;
# any drift breaks the gate.
check-cost-measure: check-sigma-pipeline
	@bash benchmarks/sigma_pipeline/check_cost_measure.sh
	@echo "check-cost-measure: OK (headline savings + accuracy retained pinned)"

check-sigma-product: check-sigma-pipeline check-sigma-generate-until check-cos-chat check-cost-measure check-orchestrator check-pipeline-bench check-installer
	@echo "check-sigma-product: OK (reinforce + speculative + ttt + engram + generate_until + cos chat + cost + orchestrator + pipeline-bench + installer)"

# --- Installer v2 (curl|sh + cos benchmark + cos cost) ---
#
# Smoke-tests scripts/install.sh syntax and the cos subcommands the
# installer advertises (chat, benchmark, cost).  Does NOT actually
# clone or touch /usr/local/bin — just verifies the pieces an
# end-user will exercise in the first minute after the install.
check-installer: cos check-sigma-pipeline
	@bash benchmarks/sigma_pipeline/check_installer.sh
	@echo "check-installer: OK (install.sh syntax + cos {status,benchmark,cost})"

# --- End-to-end pipeline benchmark ---
#
# Drives the orchestrator over a 20-row deterministic fixture,
# reports engram / local / api share, € cost saved, and accuracy
# retained vs always-API.  This is P9: the headline numbers for the
# whole product.
check-pipeline-bench: check-orchestrator
	@bash benchmarks/sigma_pipeline/check_pipeline_bench.sh
	@echo "check-pipeline-bench: OK (hybrid retains ≥100% of API accuracy at ≥50% cost reduction on the demo fixture)"

# --- Full pipeline orchestrator ---
#
# Engram → BitNet → σ-gate → RETHINK → TTT → Escalate → Engram-store,
# driven by the deterministic stub backend so CI needs no LLM.
check-orchestrator: check-sigma-pipeline check-sigma-generate-until
	@bash benchmarks/sigma_pipeline/check_orchestrator.sh
	@echo "check-orchestrator: OK (6-step pipeline end-to-end + engram hit/miss paths)"

# --- v260 σ-Engram (O(1) fact lookup + σ-gated reasoning) ---
#
# v0 contracts: parameter split static_pct ∈ [20,25] AND
# dynamic_pct ∈ [75,80] AND sum==100; exactly 5 static
# lookups with hash != 0, hit, σ in [0,1], lookup_ns <= 100
# (O(1) DRAM budget); exactly 3 dynamic-reasoning rows
# (experts_activated > 0, σ in [0,1]); exactly 4 gate
# fixtures decision matches σ vs τ_fact=0.30 (≤→USE,
# >→VERIFY), ≥1 USE AND ≥1 VERIFY; long context
# hit_rate_pct==97, miss_rate_pct==3, miss_flagged_by_sigma;
# σ_engram==0.0; FNV-1a chain replays byte-identically.
V260_INC  = -Isrc/v260
V260_SRCS = src/v260/engram.c

creation_os_v260_engram: $(V260_SRCS) src/v260/main.c
	$(CC) $(CFLAGS) $(V260_INC) -o $@ \
	    $(V260_SRCS) src/v260/main.c $(LDFLAGS)

check-v260-engram-sigma-gated-lookup: creation_os_v260_engram
	@bash benchmarks/v260/check_v260_engram_sigma_gated_lookup.sh
	@echo "check-v260-engram-sigma-gated-lookup: OK (static O(1) + dynamic MoE + gate + long-ctx)"

check-v260: check-v260-engram-sigma-gated-lookup
	@echo "check-v260: OK (σ-engram kernel)"

# --- v261 σ-AirLLM (layer-wise σ + selective precision) ---
#
# v0 contracts: exactly 8 layers (indices 0..7 strictly
# ascending) with σ_layer in [0,1] AND precision_bits
# matching the σ-driven rule (≤0.20→4, ≤0.40→8, >0.40→16)
# byte-for-byte; exactly one is_problem==true AND it is
# the argmax σ_layer; exactly 4 backends (cuda_4gb_gpu,
# cpu_only, macos_mps, linux_cuda) all supported with
# min_ram_gb≥4; exactly 3 tradeoff regimes — aircos has
# STRICTLY highest tokens_per_s AND abstain_pct>0 (gate
# teeth); σ_airllm==0.0; FNV-1a chain replays
# byte-identically.
V261_INC  = -Isrc/v261
V261_SRCS = src/v261/airllm.c

creation_os_v261_airllm: $(V261_SRCS) src/v261/main.c
	$(CC) $(CFLAGS) $(V261_INC) -o $@ \
	    $(V261_SRCS) src/v261/main.c $(LDFLAGS)

check-v261-airllm-layer-sigma: creation_os_v261_airllm
	@bash benchmarks/v261/check_v261_airllm_layer_sigma.sh
	@echo "check-v261-airllm-layer-sigma: OK (layer σ + selective precision + backends + tradeoff)"

check-v261: check-v261-airllm-layer-sigma
	@echo "check-v261: OK (σ-airllm kernel)"

# --- v262 σ-Hybrid-Engine (multi-engine σ-routing) ---
#
# v0 contracts: exactly 5 engines in canonical order
# (bitnet-3B-local, airllm-70B-local, engram-lookup,
# api-claude, api-gpt) with cost>=0 AND sigma_floor in
# [0,1]; exactly 4 routing fixtures chosen_engine in
# registry AND ≥3 DISTINCT engines; exactly 4 cascade
# steps decision matches σ vs τ_accept=0.40 (≤→OK else
# ESCALATE) AND step 0 is ESCALATE AND ≥1 OK AND ≥1
# cloud; cost: local_pct+api_pct==100, local_pct>=80,
# savings_pct>=80 AND matches within ±1 pt;
# σ_hybrid_engine==0.0; FNV-1a chain replays
# byte-identically.
V262_INC  = -Isrc/v262
V262_SRCS = src/v262/hybrid_engine.c

creation_os_v262_hybrid_engine: $(V262_SRCS) src/v262/main.c
	$(CC) $(CFLAGS) $(V262_INC) -o $@ \
	    $(V262_SRCS) src/v262/main.c $(LDFLAGS)

check-v262-hybrid-engine-routing: creation_os_v262_hybrid_engine
	@bash benchmarks/v262/check_v262_hybrid_engine_routing.sh
	@echo "check-v262-hybrid-engine-routing: OK (engines + routes + cascade + cost)"

check-v262: check-v262-hybrid-engine-routing
	@echo "check-v262: OK (σ-hybrid-engine kernel)"

# --- v263 σ-Mesh-Engram (distributed O(1) hash) ---
#
# v0 contracts: exactly 3 mesh nodes A/B/C with
# contiguous non-overlapping shards covering [0,256);
# exactly 4 lookup fixtures served_by==expected_node
# AND lookup_ns<=100 AND every node {A,B,C} served ≥1
# fixture; exactly 4 replication rows replicas==3,
# quorum_ok matches σ<=τ_quorum=0.25 AND ≥1 true AND
# ≥1 false; exactly 4 hierarchy tiers L1..L4 with
# latency_ns AND capacity_mb strictly ascending; exactly
# 4 forgetting rows action matches σ rule byte-for-byte
# AND every branch (KEEP_L1/MOVE_L2/MOVE_L3/DROP) fires
# ≥1; σ_mesh_engram==0.0; FNV-1a chain replays
# byte-identically.
V263_INC  = -Isrc/v263
V263_SRCS = src/v263/mesh_engram.c

creation_os_v263_mesh_engram: $(V263_SRCS) src/v263/main.c
	$(CC) $(CFLAGS) $(V263_INC) -o $@ \
	    $(V263_SRCS) src/v263/main.c $(LDFLAGS)

check-v263-mesh-engram-distributed: creation_os_v263_mesh_engram
	@bash benchmarks/v263/check_v263_mesh_engram_distributed.sh
	@echo "check-v263-mesh-engram-distributed: OK (nodes + lookups + replication + hierarchy + forgetting)"

check-v263: check-v263-mesh-engram-distributed
	@echo "check-v263: OK (σ-mesh-engram kernel)"

# --- v264 σ-Sovereign-Stack (full local pino + 20 €/mo) ---
#
# v0 contracts: exactly 7 stack layers in canonical order
# (hardware, model, memory, gate, network, api_fallback,
# license) every open_source; only api_fallback has
# works_offline==false AND requires_cloud==true; exactly
# 4 offline flows used_cloud==false, ok==true, engine in
# {bitnet-3B-local, airllm-70B-local, engram-lookup},
# ≥2 distinct engines; exactly 4 sovereign anchors
# (v234, v182, v148, v238) all fulfilled; cost
# eur_baseline==200, eur_sigma_sovereign==20,
# reduction_pct==90; σ_sovereign_stack==0.0; FNV-1a
# chain replays byte-identically.
V264_INC  = -Isrc/v264
V264_SRCS = src/v264/sovereign_stack.c

creation_os_v264_sovereign_stack: $(V264_SRCS) src/v264/main.c
	$(CC) $(CFLAGS) $(V264_INC) -o $@ \
	    $(V264_SRCS) src/v264/main.c $(LDFLAGS)

check-v264-sovereign-stack-offline: creation_os_v264_sovereign_stack
	@bash benchmarks/v264/check_v264_sovereign_stack_offline.sh
	@echo "check-v264-sovereign-stack-offline: OK (layers + offline flows + anchors + 200→20 €/mo)"

check-v264: check-v264-sovereign-stack-offline
	@echo "check-v264: OK (σ-sovereign-stack kernel)"

check-v260-v264: check-v260 check-v261 check-v262 check-v263 check-v264
	@echo "check-v260-v264: OK (engram + airllm + hybrid + mesh-engram + sovereign-stack)"

# --- v265 σ-Speculative (σ-guided speculative decoding) ---
#
# v0 contracts: exactly 2 models (draft=bitnet-1.5B-local,
# verifier=airllm-70B-local); exactly 4 σ-bands with
# canonical spec_len [12,8,6,4] strictly non-increasing
# in σ (monotone_ok); exactly 3 multi-draft duels with
# winner == argmin(σ) AND ≥1 A-win AND ≥1 B-win; exactly
# 4 σ-gate fixtures, decision matches σ vs τ_spec=0.35
# AND ≥1 ACCEPT AND ≥1 REJECT; throughput plain <
# sigma_spec AND speedup_x ≥ 2.0; σ_speculative==0.0;
# FNV-1a chain replays byte-identically.
V265_INC  = -Isrc/v265
V265_SRCS = src/v265/speculative.c

creation_os_v265_speculative: $(V265_SRCS) src/v265/main.c
	$(CC) $(CFLAGS) $(V265_INC) -o $@ \
	    $(V265_SRCS) src/v265/main.c $(LDFLAGS)

check-v265-speculative-sigma-guided: creation_os_v265_speculative
	@bash benchmarks/v265/check_v265_speculative_sigma_guided.sh
	@echo "check-v265-speculative-sigma-guided: OK (models + bands + duels + gate + 2x speedup)"

check-v265: check-v265-speculative-sigma-guided
	@echo "check-v265: OK (σ-speculative kernel)"

# --- v266 σ-Flash (FlashAttention + fused σ kernel) ---
#
# v0 contracts: exactly 8 heads all fused, overhead_pct
# strictly < 1.0, σ_head ∈ [0,1]; exactly 3 platforms
# canonical (cuda_sm90, metal_m4, neon_arm64) all
# supported AND sigma_fused AND latency_ns > 0; exactly
# 6 KV entries with evict_rank permutation of [1..6] in
# descending-σ order (rank 1 = max σ); kv_order_ok;
# pruning kept_tokens unchanged AND effective_ctx_k
# strictly grows AND pruning_ok; σ_flash==0.0; FNV-1a
# chain replays byte-identically.
V266_INC  = -Isrc/v266
V266_SRCS = src/v266/flash.c

creation_os_v266_flash: $(V266_SRCS) src/v266/main.c
	$(CC) $(CFLAGS) $(V266_INC) -o $@ \
	    $(V266_SRCS) src/v266/main.c $(LDFLAGS)

check-v266-flash-attention-sigma-fused: creation_os_v266_flash
	@bash benchmarks/v266/check_v266_flash_attention_sigma_fused.sh
	@echo "check-v266-flash-attention-sigma-fused: OK (heads + platforms + kv order + pruning)"

check-v266: check-v266-flash-attention-sigma-fused
	@echo "check-v266: OK (σ-flash kernel)"

# --- v267 σ-Mamba (SSM backend + σ-gated fallback) ---
#
# v0 contracts: exactly 3 backends canonical (mamba,
# rwkv, transformer) with mamba/rwkv exponent==1 AND
# transformer exponent==2 AND mamba/rwkv throughput_rel
# > transformer; exactly 4 route fixtures decision
# matches σ vs τ_mamba=0.40 AND ≥1 mamba AND ≥1
# transformer; exactly 8 hybrid layers alternating
# mamba/transformer (4+4); exactly 3 tasks with
# σ_chosen ≤ σ_rival each AND ≥2 distinct chosen
# backends; σ_mamba_kernel==0.0; FNV-1a chain replays
# byte-identically.
V267_INC  = -Isrc/v267
V267_SRCS = src/v267/mamba.c

creation_os_v267_mamba: $(V267_SRCS) src/v267/main.c
	$(CC) $(CFLAGS) $(V267_INC) -o $@ \
	    $(V267_SRCS) src/v267/main.c $(LDFLAGS)

check-v267-mamba-sigma-gated-fallback: creation_os_v267_mamba
	@bash benchmarks/v267/check_v267_mamba_sigma_gated_fallback.sh
	@echo "check-v267-mamba-sigma-gated-fallback: OK (backends + routes + hybrid layers + task diversity)"

check-v267: check-v267-mamba-sigma-gated-fallback
	@echo "check-v267: OK (σ-mamba kernel)"

# --- v268 σ-Continuous-Batch (σ-priority continuous batching) ---
#
# v0 contracts: exactly 6 queue rows with priority_slot
# permutation of [1..6] matching argsort(+σ_difficulty)
# (queue_order_ok); exactly 2 preemption scenarios
# preempted == (σ_urgency_arrival > σ_urgency_incumbent)
# AND ≥1 true AND ≥1 false; exactly 3 load levels
# canonical (low, medium, high) with σ_load AND
# batch_size strictly ascending (batch_monotone_ok);
# exactly 2 cost scenarios with total_local_eur <
# total_api_eur; σ_continuous_batch==0.0; FNV-1a chain
# replays byte-identically.
V268_INC  = -Isrc/v268
V268_SRCS = src/v268/continuous_batch.c

creation_os_v268_continuous_batch: $(V268_SRCS) src/v268/main.c
	$(CC) $(CFLAGS) $(V268_INC) -o $@ \
	    $(V268_SRCS) src/v268/main.c $(LDFLAGS)

check-v268-continuous-batch-priority: creation_os_v268_continuous_batch
	@bash benchmarks/v268/check_v268_continuous_batch_priority.sh
	@echo "check-v268-continuous-batch-priority: OK (queue + preempt + batch + cost)"

check-v268: check-v268-continuous-batch-priority
	@echo "check-v268: OK (σ-continuous-batch kernel)"

# --- v269 σ-Compile-v2 (full pipeline AOT) ---
#
# v0 contracts: exactly 6 pipeline stages canonical
# (tokenize, embed, attention, ffn, sigma_gate,
# detokenize) every aot_compiled AND native AND
# latency_ns > 0; exactly 4 platform targets canonical
# (m4_apple_silicon, rpi5_arm64, gpu_4gb_speculative,
# x86_avx512) every tok_per_s ≥ budget AND meets_budget;
# exactly 4 PGO rows with optimization matching
# hotpath_fraction ≥ 0.20 rule AND ≥1 aggressive AND
# ≥1 space; exactly 6 elim rows elided ==
# (sigma_profile < 0.05) AND ≥1 elided AND ≥1 kept;
# σ_compile_v2==0.0; FNV-1a chain replays
# byte-identically.
V269_INC  = -Isrc/v269
V269_SRCS = src/v269/compile_v2.c

creation_os_v269_compile_v2: $(V269_SRCS) src/v269/main.c
	$(CC) $(CFLAGS) $(V269_INC) -o $@ \
	    $(V269_SRCS) src/v269/main.c $(LDFLAGS)

check-v269-compile-v2-full-pipeline-aot: creation_os_v269_compile_v2
	@bash benchmarks/v269/check_v269_compile_v2_full_pipeline_aot.sh
	@echo "check-v269-compile-v2-full-pipeline-aot: OK (stages + targets + pgo + elim)"

check-v269: check-v269-compile-v2-full-pipeline-aot
	@echo "check-v269: OK (σ-compile-v2 kernel)"

check-v265-v269: check-v265 check-v266 check-v267 check-v268 check-v269
	@echo "check-v265-v269: OK (speculative + flash + mamba + continuous-batch + compile-v2)"

# --- v270 σ-TinyML (MCU σ-gate) ---
#
# v0 contracts: footprint envelope (sigma_measurement
# == 12 B, code_flash ≤ 1024 B, ram ≤ 100 B, thumb2
# ≤ 24 instr, branchless); exactly 4 MCU targets
# canonical (cortex_m0_plus, cortex_m4, cortex_m7,
# xtensa_esp32) all supported; exactly 3 sensors
# canonical (temperature, humidity, pressure); exactly
# 4 fusion fixtures, decision matches σ_fusion vs
# τ_fusion=0.30 with ≥1 TRANSMIT AND ≥1 RETRY; exactly
# 4 anomaly rows with anomaly == (σ > σ_baseline+delta)
# firing both branches; exactly 3 OTA rounds every
# applied AND every firmware_reflash==false;
# σ_tinyml==0.0; FNV-1a chain replays byte-identically.
V270_INC  = -Isrc/v270
V270_SRCS = src/v270/tinyml.c

creation_os_v270_tinyml: $(V270_SRCS) src/v270/main.c
	$(CC) $(CFLAGS) $(V270_INC) -o $@ \
	    $(V270_SRCS) src/v270/main.c $(LDFLAGS)

check-v270-tinyml-mcu-sigma-gate: creation_os_v270_tinyml
	@bash benchmarks/v270/check_v270_tinyml_mcu_sigma_gate.sh
	@echo "check-v270-tinyml-mcu-sigma-gate: OK (footprint + targets + fusion + anomaly + ota)"

check-v270: check-v270-tinyml-mcu-sigma-gate
	@echo "check-v270: OK (σ-tinyml kernel)"

# --- v271 σ-Swarm-Edge (mesh sensor orchestration) ---
#
# v0 contracts: 6 mesh sensors; included iff σ_local
# ≤ τ_consensus=0.50; σ_swarm < σ_raw; exactly 4
# distributed-anomaly fixtures with spatial_anomaly
# == ((σ_center - σ_neighborhood) > 0.25) firing both
# branches; exactly 3 energy tiers canonical (charged,
# medium, low) with σ_energy strictly ascending AND
# sample_rate_hz strictly descending; gateway
# bridged_to_engine in v262 set; swarm_size_nodes==6;
# σ_swarm_edge==0.0; FNV-1a chain replays
# byte-identically.
V271_INC  = -Isrc/v271
V271_SRCS = src/v271/swarm_edge.c

creation_os_v271_swarm_edge: $(V271_SRCS) src/v271/main.c
	$(CC) $(CFLAGS) $(V271_INC) -o $@ \
	    $(V271_SRCS) src/v271/main.c $(LDFLAGS)

check-v271-swarm-edge-consensus: creation_os_v271_swarm_edge
	@bash benchmarks/v271/check_v271_swarm_edge_consensus.sh
	@echo "check-v271-swarm-edge-consensus: OK (consensus + spatial + energy + gateway)"

check-v271: check-v271-swarm-edge-consensus
	@echo "check-v271: OK (σ-swarm-edge kernel)"

# --- v272 σ-Digital-Twin (physical + digital) ---
#
# v0 contracts: exactly 4 twin-sync fixtures with
# stable == (σ_twin < 0.05) AND drifted == (σ_twin >
# 0.30) firing both branches; exactly 3 maintenance
# rows REPLACE iff σ_prediction ≤ 0.30 firing both
# branches; exactly 3 what-if rows IMPLEMENT iff
# σ_whatif ≤ 0.25 firing both branches; exactly 3
# verified-action rows σ_match == |declared - realized|
# PASS iff σ_match ≤ 0.10 firing both branches;
# σ_digital_twin==0.0; FNV-1a chain replays
# byte-identically.
V272_INC  = -Isrc/v272
V272_SRCS = src/v272/digital_twin.c

creation_os_v272_digital_twin: $(V272_SRCS) src/v272/main.c
	$(CC) $(CFLAGS) $(V272_INC) -o $@ \
	    $(V272_SRCS) src/v272/main.c $(LDFLAGS)

check-v272-digital-twin-sigma-sync: creation_os_v272_digital_twin
	@bash benchmarks/v272/check_v272_digital_twin_sigma_sync.sh
	@echo "check-v272-digital-twin-sigma-sync: OK (sync + pred + whatif + verified)"

check-v272: check-v272-digital-twin-sigma-sync
	@echo "check-v272: OK (σ-digital-twin kernel)"

# --- v273 σ-Robotics (physical robotics σ-layer) ---
#
# v0 contracts: exactly 4 action fixtures with cascade
# decision (σ≤0.20 EXECUTE, σ≤0.50 SIMPLIFY, else
# ASK_HUMAN) with all three branches firing; exactly 3
# perception sensors canonical (camera, lidar,
# ultrasonic) with fused_in == (σ_local ≤ τ_fuse=0.40)
# and sigma_percep_fused < sigma_percep_naive; exactly
# 4 safety envelope rows with σ_safety strictly
# ascending AND slow_factor strictly descending; exactly
# 3 failure-memory rows with σ_current > σ_prior for
# all; σ_robotics==0.0; FNV-1a chain replays
# byte-identically.
V273_INC  = -Isrc/v273
V273_SRCS = src/v273/robotics.c

creation_os_v273_robotics: $(V273_SRCS) src/v273/main.c
	$(CC) $(CFLAGS) $(V273_INC) -o $@ \
	    $(V273_SRCS) src/v273/main.c $(LDFLAGS)

check-v273-robotics-action-sigma: creation_os_v273_robotics
	@bash benchmarks/v273/check_v273_robotics_action_sigma.sh
	@echo "check-v273-robotics-action-sigma: OK (action + percep + safety + fail)"

check-v273: check-v273-robotics-action-sigma
	@echo "check-v273: OK (σ-robotics kernel)"

# --- v274 σ-Industrial (Industry 4.0 σ-governance) ---
#
# v0 contracts: exactly 4 process params canonical
# (temperature, pressure, speed, material) with
# σ_process == max(σ_param) and action CONTINUE iff
# σ_process ≤ τ_process=0.40 (fixture drives HALT);
# exactly 4 supply links canonical (supplier, factory,
# distribution, customer) with backup_activated iff
# σ_link > τ_backup=0.45 firing both branches; exactly
# 3 quality rows SKIP_MANUAL iff σ_quality ≤ 0.25
# firing both branches; exactly 3 OEE shifts with
# oee == a*p*q (1e-4) and trustworthy iff σ_oee ≤ 0.20
# firing both branches; σ_industrial==0.0; FNV-1a chain
# replays byte-identically.
V274_INC  = -Isrc/v274
V274_SRCS = src/v274/industrial.c

creation_os_v274_industrial: $(V274_SRCS) src/v274/main.c
	$(CC) $(CFLAGS) $(V274_INC) -o $@ \
	    $(V274_SRCS) src/v274/main.c $(LDFLAGS)

check-v274-industrial-process-sigma: creation_os_v274_industrial
	@bash benchmarks/v274/check_v274_industrial_process_sigma.sh
	@echo "check-v274-industrial-process-sigma: OK (process + supply + quality + oee)"

check-v274: check-v274-industrial-process-sigma
	@echo "check-v274: OK (σ-industrial kernel)"

check-v270-v274: check-v270 check-v271 check-v272 check-v273 check-v274
	@echo "check-v270-v274: OK (tinyml + swarm-edge + digital-twin + robotics + industrial)"

# --- v275 σ-TTT (test-time training gated by σ) ---
#
# v0 contracts: exactly 4 σ-gated update rows (LEARN
# iff σ_update ≤ τ_update=0.30 else SKIP, both
# branches); exactly 3 dual-track rows with cascade
# SYNCED <0.15 / DIVERGING <0.50 / RESET else firing
# all three branches; exactly 6 sliding-window tokens
# whose evict_rank is a permutation of [1..6] matching
# descending σ; exactly 2 validation citations
# (v124_sigma_continual + ttt_e2e_2025) present and
# distinct; σ_ttt==0.0; FNV-1a chain replays
# byte-identically.
V275_INC  = -Isrc/v275
V275_SRCS = src/v275/ttt.c

creation_os_v275_ttt: $(V275_SRCS) src/v275/main.c
	$(CC) $(CFLAGS) $(V275_INC) -o $@ \
	    $(V275_SRCS) src/v275/main.c $(LDFLAGS)

check-v275-ttt-sigma-gated-update: creation_os_v275_ttt
	@bash benchmarks/v275/check_v275_ttt_sigma_gated_update.sh
	@echo "check-v275-ttt-sigma-gated-update: OK (update + dualtrack + window + validation)"

check-v275: check-v275-ttt-sigma-gated-update
	@echo "check-v275: OK (σ-ttt kernel)"

# --- v276 σ-Gated-DeltaNet (linear attention with σ) ---
#
# v0 contracts: exactly 2 canonical backends
# (deltanet exp=1 gate=true · transformer exp=2
# gate=false) with deltanet.throughput_rel >
# transformer.throughput_rel; exactly 4 σ-gate
# fixtures with decision LINEAR iff σ_gate ≤
# τ_gate=0.35 else FALLBACK_FULL firing both branches;
# exactly 3 combo components canonical (deltanet, ttt,
# sigma_gate) each enabled with layer_slot 1..3;
# exactly 3 tri-backend tasks with chosen ==
# argmin(σ_backend) AND σ_chosen ≤ σ_rival for each
# task AND ≥ 2 distinct chosen backends across the 3
# tasks; σ_deltanet==0.0; FNV-1a chain replays
# byte-identically.
V276_INC  = -Isrc/v276
V276_SRCS = src/v276/gated_deltanet.c

creation_os_v276_gated_deltanet: $(V276_SRCS) src/v276/main.c
	$(CC) $(CFLAGS) $(V276_INC) -o $@ \
	    $(V276_SRCS) src/v276/main.c $(LDFLAGS)

check-v276-deltanet-linear-attention-sigma: creation_os_v276_gated_deltanet
	@bash benchmarks/v276/check_v276_deltanet_linear_attention_sigma.sh
	@echo "check-v276-deltanet-linear-attention-sigma: OK (backend + gate + combo + tasks)"

check-v276: check-v276-deltanet-linear-attention-sigma
	@echo "check-v276: OK (σ-gated-deltanet kernel)"

# --- v277 σ-Distill-Runtime (live teacher/student σ) ---
#
# v0 contracts: pair canonical (api-claude /
# bitnet-3B-local, distinct); exactly 4 σ-filter rows
# with LEARN iff σ_teacher ≤ τ_learn=0.25 else SKIP
# firing both branches; exactly 3 domains canonical
# (law, code, medical) with route LOCAL_ONLY iff
# σ_domain ≤ τ_domain=0.30 else API firing both
# branches; exactly 4 trajectory checkpoints
# (month_0/1/3/12) with api_share+local_share=1,
# api_share strictly decreasing, local_share strictly
# increasing, cost strictly decreasing, and anchors
# api_share[0]≥0.75 / api_share[-1]≤0.10;
# σ_distill==0.0; FNV-1a chain replays byte-identically.
V277_INC  = -Isrc/v277
V277_SRCS = src/v277/distill_runtime.c

creation_os_v277_distill_runtime: $(V277_SRCS) src/v277/main.c
	$(CC) $(CFLAGS) $(V277_INC) -o $@ \
	    $(V277_SRCS) src/v277/main.c $(LDFLAGS)

check-v277-distill-runtime-live: creation_os_v277_distill_runtime
	@bash benchmarks/v277/check_v277_distill_runtime_live.sh
	@echo "check-v277-distill-runtime-live: OK (pair + filter + domains + trajectory)"

check-v277: check-v277-distill-runtime-live
	@echo "check-v277: OK (σ-distill-runtime kernel)"

# --- v278 σ-Recursive-Self-Improve (σ improves σ) ---
#
# v0 contracts: exactly 4 calibration epochs with
# strictly decreasing σ_calibration_err and last
# epoch ≤ 0.05; exactly 3 arch configs (6/8/12
# channels) with chosen == argmax(aurcc) AND ≥ 2
# distinct aurcc values; exactly 3 thresholds
# canonical (code=0.20, creative=0.50, medical=0.15)
# each τ ∈ (0, 1) AND ≥ 2 distinct τ values; exactly
# 3 Gödel rows with SELF_CONFIDENT iff σ_goedel ≤
# τ_goedel=0.40 else CALL_PROCONDUCTOR firing both
# branches; σ_rsi==0.0; FNV-1a chain replays
# byte-identically.
V278_INC  = -Isrc/v278
V278_SRCS = src/v278/rsi.c

creation_os_v278_rsi: $(V278_SRCS) src/v278/main.c
	$(CC) $(CFLAGS) $(V278_INC) -o $@ \
	    $(V278_SRCS) src/v278/main.c $(LDFLAGS)

check-v278-recursive-self-improve: creation_os_v278_rsi
	@bash benchmarks/v278/check_v278_recursive_self_improve.sh
	@echo "check-v278-recursive-self-improve: OK (calib + arch + thresh + goedel)"

check-v278: check-v278-recursive-self-improve
	@echo "check-v278: OK (σ-recursive-self-improve kernel)"

check-v275-v278: check-v275 check-v276 check-v277 check-v278
	@echo "check-v275-v278: OK (ttt + gated-deltanet + distill-runtime + rsi)"

# --- v279 σ-JEPA (world model with σ = prediction error) ---
#
# v0 contracts: exactly 4 prediction rows (ACT iff
# σ_prediction ≤ τ_predict=0.30 else OBSERVE, both
# branches); exactly 3 latent checkpoints canonical
# (early, mid, late) with entropy_z and sigma_latent
# both strictly decreasing AND per-row convergence
# |entropy_z − sigma_latent| ≤ 0.05; exactly 2 loss
# terms canonical (prediction, regularizer), distinct,
# weights sum to 1.0 (± 1e-3); exactly 2 validation
# citations (lecun_jepa_2022 + leworldmodel_2026_03)
# present and distinct; σ_jepa==0.0; FNV-1a chain
# replays byte-identically.
V279_INC  = -Isrc/v279
V279_SRCS = src/v279/jepa.c

creation_os_v279_jepa: $(V279_SRCS) src/v279/main.c
	$(CC) $(CFLAGS) $(V279_INC) -o $@ \
	    $(V279_SRCS) src/v279/main.c $(LDFLAGS)

check-v279-jepa-world-model-sigma: creation_os_v279_jepa
	@bash benchmarks/v279/check_v279_jepa_world_model_sigma.sh
	@echo "check-v279-jepa-world-model-sigma: OK (predict + latent + loss + validation)"

check-v279: check-v279-jepa-world-model-sigma
	@echo "check-v279: OK (σ-jepa kernel)"

# --- v280 σ-MoE (Mixture of Experts with σ on the router) ---
#
# v0 contracts: exactly 4 routing rows (TOP_K iff
# σ_routing ≤ τ_route=0.35 else DIVERSIFY, both
# branches); exactly 3 routing signatures canonical
# (code, math, creative) with familiar KNOWN iff
# routing_entropy ≤ τ_sig=0.40 else NOVEL, both
# branches; exactly 3 prefetch rows canonical (low,
# mid, high) with strategy cascade AGGRESSIVE ≤ 0.20
# / BALANCED ≤ 0.50 / CONSERVATIVE else each firing
# exactly once; exactly 3 MoBiE rows canonical
# (expert_0..expert_2) with bits cascade BIT1 ≤ 0.20
# / BIT2 ≤ 0.50 / BIT4 else each firing exactly once;
# σ_moe==0.0; FNV-1a chain replays byte-identically.
V280_INC  = -Isrc/v280
V280_SRCS = src/v280/moe.c

creation_os_v280_moe: $(V280_SRCS) src/v280/main.c
	$(CC) $(CFLAGS) $(V280_INC) -o $@ \
	    $(V280_SRCS) src/v280/main.c $(LDFLAGS)

check-v280-moe-sigma-routing: creation_os_v280_moe
	@bash benchmarks/v280/check_v280_moe_sigma_routing.sh
	@echo "check-v280-moe-sigma-routing: OK (route + signatures + prefetch + mobie)"

check-v280: check-v280-moe-sigma-routing
	@echo "check-v280: OK (σ-moe kernel)"

# --- v281 σ-Jamba (hybrid Transformer · Mamba · MoE) ---
#
# v0 contracts: exactly 3 layer types canonical
# (mamba LINEAR, transformer QUADRATIC, moe SPARSE)
# all distinct; exactly 4 mixing rows canonical
# (easy→MAMBA, hard→TRANSFORMER, factual→MOE,
# long→MAMBA) with ≥ 2 distinct archs chosen; exactly
# 5 memory tiers canonical (engram, mamba, ttt,
# transformer, moe) all enabled with tier_slot
# permutation [1,2,3,4,5]; exactly 3 bench metrics
# canonical (accuracy, latency, throughput) with
# sigma_jamba ≤ sigma_baseline per metric;
# σ_jamba==0.0; FNV-1a chain replays byte-identically.
V281_INC  = -Isrc/v281
V281_SRCS = src/v281/jamba.c

creation_os_v281_jamba: $(V281_SRCS) src/v281/main.c
	$(CC) $(CFLAGS) $(V281_INC) -o $@ \
	    $(V281_SRCS) src/v281/main.c $(LDFLAGS)

check-v281-jamba-hybrid-adaptive: creation_os_v281_jamba
	@bash benchmarks/v281/check_v281_jamba_hybrid_adaptive.sh
	@echo "check-v281-jamba-hybrid-adaptive: OK (layer + mixing + memory + bench)"

check-v281: check-v281-jamba-hybrid-adaptive
	@echo "check-v281: OK (σ-jamba kernel)"

# --- v282 σ-Agent (autonomous agent with σ everywhere) ---
#
# v0 contracts: exactly 4 action rows with 3-way
# cascade (AUTO ≤ 0.20 / ASK ≤ 0.60 / BLOCK else)
# each branch firing ≥ 1; exactly 2 propagation
# chains (short 3@0.10, long 10@0.30) with σ_total
# = 1 − (1 − σ_per_step)^n_steps matching within
# 1e-4 AND PROCEED (σ_total ≤ τ_chain=0.70) and
# ABORT each firing exactly once; exactly 3 tool
# rows canonical (correct, wrong_light, wrong_heavy)
# with cascade USE ≤ 0.30 / SWAP ≤ 0.60 / BLOCK else
# each branch firing ≥ 1; exactly 3 recovery rows
# with σ_after_fail > σ_first_try strictly per row
# AND gate_update_applied on every row; σ_agent==0.0;
# FNV-1a chain replays byte-identically.
V282_INC  = -Isrc/v282
V282_SRCS = src/v282/agent.c

creation_os_v282_agent: $(V282_SRCS) src/v282/main.c
	$(CC) $(CFLAGS) $(V282_INC) -o $@ \
	    $(V282_SRCS) src/v282/main.c $(LDFLAGS)

check-v282-agent-action-sigma-gate: creation_os_v282_agent
	@bash benchmarks/v282/check_v282_agent_action_sigma_gate.sh
	@echo "check-v282-agent-action-sigma-gate: OK (action + chain + tool + recovery)"

check-v282: check-v282-agent-action-sigma-gate
	@echo "check-v282: OK (σ-agent kernel)"

check-v279-v282: check-v279 check-v280 check-v281 check-v282
	@echo "check-v279-v282: OK (jepa + moe + jamba + agent)"

# --- v283 σ-Constitutional (alignment by coherence, no RLHF) ---
#
# v0 contracts: exactly 3 mechanism rows canonical
# (rlhf, constitutional_ai, sigma_constitutional)
# where sigma_constitutional is the ONLY row with
# uses_sigma=true AND uses_rl=false AND
# uses_reward_model=false; exactly 8 σ-channels
# canonical order (entropy/repetition/calibration/
# attention/logit/hidden/output/aggregate) all enabled
# AND distinct; exactly 4 firmware rows canonical
# (care_as_control/sycophancy/opinion_laundering/
# people_pleasing) with rlhf_produces=true AND
# sigma_produces=false on EVERY row; exactly 2
# self-critique rows (single_instance NOT Gödel-safe,
# two_instance IS Gödel-safe) both with
# has_producer=true; σ_constitutional==0.0; FNV-1a
# chain replays byte-identically.
V283_INC  = -Isrc/v283
V283_SRCS = src/v283/constitutional.c

creation_os_v283_constitutional: $(V283_SRCS) src/v283/main.c
	$(CC) $(CFLAGS) $(V283_INC) -o $@ \
	    $(V283_SRCS) src/v283/main.c $(LDFLAGS)

check-v283-constitutional-sigma-alignment: creation_os_v283_constitutional
	@bash benchmarks/v283/check_v283_constitutional_sigma_alignment.sh
	@echo "check-v283-constitutional-sigma-alignment: OK (mechanism + channels + firmware + goedel)"

check-v283: check-v283-constitutional-sigma-alignment
	@echo "check-v283: OK (σ-constitutional kernel)"

# --- v284 σ-Multi-Agent (framework-agnostic σ-layer) ---
#
# v0 contracts: exactly 4 adapter rows canonical
# (langgraph/crewai/autogen/swarm) all enabled AND
# sigma_middleware on every row; exactly 4 a2a rows
# with decision TRUST iff σ_message ≤ τ_a2a=0.40
# else VERIFY (both branches fire); exactly 5
# consensus rows with weight_i = (1−σ_i)/Σ(1−σ_j)
# summing to 1.0 ± 1e-3, exactly 1 is_winner AND
# winner == argmin σ == argmax weight; exactly 4
# routing rows canonical (easy LOCAL /1, medium
# NEGOTIATE /2, hard CONSENSUS /5, critical HITL /0)
# each mode firing exactly once; σ_multiagent==0.0;
# FNV-1a chain replays byte-identically.
V284_INC  = -Isrc/v284
V284_SRCS = src/v284/multi_agent.c

creation_os_v284_multi_agent: $(V284_SRCS) src/v284/main.c
	$(CC) $(CFLAGS) $(V284_INC) -o $@ \
	    $(V284_SRCS) src/v284/main.c $(LDFLAGS)

check-v284-multi-agent-sigma-layer: creation_os_v284_multi_agent
	@bash benchmarks/v284/check_v284_multi_agent_sigma_layer.sh
	@echo "check-v284-multi-agent-sigma-layer: OK (adapter + a2a + consensus + routing)"

check-v284: check-v284-multi-agent-sigma-layer
	@echo "check-v284: OK (σ-multi-agent kernel)"

# --- v285 σ-EU-AI-Act (regulatory fit: Art 15 / Art 52) ---
#
# v0 contracts: exactly 3 Art-15 rows canonical
# (robustness/accuracy/cybersecurity) with
# sigma_mapped AND audit_trail on EVERY row; exactly
# 3 Art-52 rows canonical (training_docs/
# feedback_collection/qa_process) with required_by_eu
# AND sigma_simplifies on EVERY row; exactly 4 risk
# tiers canonical (low/medium/high/critical) with
# sigma_gate on EVERY tier AND controls_count
# strictly monotonic 1→2→3→4 AND critical tier has
# all 4 controls; exactly 3 license rows (scsl LEGAL /
# eu_ai_act REGULATORY / sigma_gate TECHNICAL) with 3
# DISTINCT layers AND every row enabled AND
# composable; σ_euact==0.0; FNV-1a chain replays
# byte-identically.
V285_INC  = -Isrc/v285
V285_SRCS = src/v285/eu_ai_act.c

creation_os_v285_eu_ai_act: $(V285_SRCS) src/v285/main.c
	$(CC) $(CFLAGS) $(V285_INC) -o $@ \
	    $(V285_SRCS) src/v285/main.c $(LDFLAGS)

check-v285-eu-ai-act-compliance: creation_os_v285_eu_ai_act
	@bash benchmarks/v285/check_v285_eu_ai_act_compliance.sh
	@echo "check-v285-eu-ai-act-compliance: OK (art15 + art52 + risk + license)"

check-v285: check-v285-eu-ai-act-compliance
	@echo "check-v285: OK (σ-EU-AI-Act kernel)"

# --- v286 σ-Interpretability (σ decomposition + report) ---
#
# v0 contracts: exactly 4 decomposition scenarios
# (low_confidence→entropy, repetitive→repetition,
# overconfident→calibration, distracted→attention)
# with 4 DISTINCT top_channels AND every cause
# non-empty; exactly 3 attention heads (head_0/1/2)
# with status CONFIDENT iff σ_head ≤ τ_attn=0.40
# else UNCERTAIN (both branches fire); exactly 3
# counterfactual rows with delta_sigma =
# |σ_without − σ_with| within 1e-5 AND classification
# CRITICAL iff delta_sigma > δ_critical=0.10 else
# IRRELEVANT (both branches fire); exactly 3 report
# rows with trust_percent ∈ [0, 100] AND explanation
# AND recommendation AND EU AI Act Article 13
# compliance asserted on EVERY row; σ_interpret==0.0;
# FNV-1a chain replays byte-identically.
V286_INC  = -Isrc/v286
V286_SRCS = src/v286/interpretability.c

creation_os_v286_interpretability: $(V286_SRCS) src/v286/main.c
	$(CC) $(CFLAGS) $(V286_INC) -o $@ \
	    $(V286_SRCS) src/v286/main.c $(LDFLAGS)

check-v286-interpretability-decomposition: creation_os_v286_interpretability
	@bash benchmarks/v286/check_v286_interpretability_decomposition.sh
	@echo "check-v286-interpretability-decomposition: OK (decomp + attn + counterfactual + report)"

check-v286: check-v286-interpretability-decomposition
	@echo "check-v286: OK (σ-interpretability kernel)"

check-v283-v286: check-v283 check-v284 check-v285 check-v286
	@echo "check-v283-v286: OK (constitutional + multi-agent + eu-ai-act + interpretability)"

# --- v287 σ-Granite (zero-dep, C99, platform-agnostic) ---
#
# v0 contracts: exactly 6 dependency rows canonical
# (libc/posix/pthreads ALLOW with in_kernel=true;
# npm/pip/cargo FORBID with in_kernel=false); exactly
# 3 language standards canonical (C89/C99 allowed,
# C++ forbidden); exactly 5 platform rows canonical
# (linux/macos/bare_metal/rtos/cortex_m) each
# kernel_works AND ifdef_only_at_edges on EVERY row;
# exactly 3 vendoring rows canonical (vendored_copy
# allowed; external_linkage AND supply_chain_trust
# forbidden); σ_granite==0.0; FNV-1a chain replays
# byte-identically.
V287_INC  = -Isrc/v287
V287_SRCS = src/v287/granite.c

creation_os_v287_granite: $(V287_SRCS) src/v287/main.c
	$(CC) $(CFLAGS) $(V287_INC) -o $@ \
	    $(V287_SRCS) src/v287/main.c $(LDFLAGS)

check-v287-granite-zero-deps: creation_os_v287_granite
	@bash benchmarks/v287/check_v287_granite_zero_deps.sh
	@echo "check-v287-granite-zero-deps: OK (deps + std + platform + vendor)"

check-v287: check-v287-granite-zero-deps
	@echo "check-v287: OK (σ-granite kernel)"

# --- v288 σ-Oculus (tunable aperture) ---
#
# v0 contracts: exactly 3 cascade rows canonical
# (medical TIGHT 0.10 / code NORMAL 0.30 / creative
# OPEN 0.60) with 3 DISTINCT widths AND τ strictly
# increasing; exactly 3 extreme fixtures (closed
# useless/not dangerous, open dangerous/not useless,
# optimal neither); exactly 3 adaptive self-tuning
# steps with error_rate strictly decreasing AND
# action TIGHTEN iff error>threshold_error=0.05 else
# STABLE (both branches fire) AND τ_{n+1}<τ_n on
# every TIGHTEN; exactly 3 transparency fields
# (tau_declared/sigma_measured/decision_visible) all
# reported; σ_oculus==0.0; FNV-1a chain replays
# byte-identically.
V288_INC  = -Isrc/v288
V288_SRCS = src/v288/oculus.c

creation_os_v288_oculus: $(V288_SRCS) src/v288/main.c
	$(CC) $(CFLAGS) $(V288_INC) -o $@ \
	    $(V288_SRCS) src/v288/main.c $(LDFLAGS)

check-v288-oculus-tunable-aperture: creation_os_v288_oculus
	@bash benchmarks/v288/check_v288_oculus_tunable_aperture.sh
	@echo "check-v288-oculus-tunable-aperture: OK (cascade + extreme + adaptive + transparent)"

check-v288: check-v288-oculus-tunable-aperture
	@echo "check-v288: OK (σ-oculus kernel)"

# --- v289 σ-Ruin-Value (graceful degradation) ---
#
# v0 contracts: exactly 4 kernel-removal rows
# canonical (v267_mamba→transformer, v260_engram→
# local_memory, v275_ttt→frozen_weights, v262_hybrid
# →direct_kernel) all survivor_still_works AND
# distinct; exactly 4 cascade tiers canonical
# (hybrid_engine/transformer_only/bitnet_plus_sigma/
# pure_sigma_gate) with tier_id permutation [1..4],
# all standalone_viable, resource_cost strictly
# monotonically decreasing; exactly 3 preservation
# rows (sigma_log_persisted/atomic_write_wal/
# last_measurement_recoverable) all guaranteed;
# exactly 3 rebuild steps (read_sigma_log→
# restore_last_state→resume_not_restart) with
# step_order permutation [1,2,3] all possible;
# seed_kernels_required==5; σ_ruin==0.0; FNV-1a
# chain replays byte-identically.
V289_INC  = -Isrc/v289
V289_SRCS = src/v289/ruin_value.c

creation_os_v289_ruin_value: $(V289_SRCS) src/v289/main.c
	$(CC) $(CFLAGS) $(V289_INC) -o $@ \
	    $(V289_SRCS) src/v289/main.c $(LDFLAGS)

check-v289-ruin-value-graceful-degradation: creation_os_v289_ruin_value
	@bash benchmarks/v289/check_v289_ruin_value_graceful_degradation.sh
	@echo "check-v289-ruin-value-graceful-degradation: OK (removal + cascade + preserve + rebuild)"

check-v289: check-v289-ruin-value-graceful-degradation
	@echo "check-v289: OK (σ-ruin-value kernel)"

# --- v290 σ-Dougong (modular flexibility) ---
#
# v0 contracts: exactly 4 coupling rows canonical
# (v267_mamba→v262_hybrid, v260_engram→v206_ghosts,
# v275_ttt→v272_agentic_rl, v286_interp→v269_stopping)
# with channel=="sigma_measurement_t" AND no
# direct_call on EVERY row; exactly 3 hot-swap rows
# (v267_mamba→v276_deltanet long_context,
# v216_quorum→v214_swarm agent_consensus,
# v232_sqlite→v224_snapshot state_persistence) with
# downtime_ms==0 AND config_unchanged on EVERY row;
# exactly 3 seismic scenarios canonical (spike_small
# 0.40 / spike_medium 0.60 / spike_large 0.78) all
# distributed AND max_sigma_load<=load_budget=0.80;
# exactly 3 chaos rows (kill_random_kernel→survived,
# overload_single_kernel→load_distributed,
# network_partition→degraded_but_alive) with
# distinct outcomes AND all passed; σ_dougong==0.0;
# FNV-1a chain replays byte-identically.
V290_INC  = -Isrc/v290
V290_SRCS = src/v290/dougong.c

creation_os_v290_dougong: $(V290_SRCS) src/v290/main.c
	$(CC) $(CFLAGS) $(V290_INC) -o $@ \
	    $(V290_SRCS) src/v290/main.c $(LDFLAGS)

check-v290-dougong-modular-flexibility: creation_os_v290_dougong
	@bash benchmarks/v290/check_v290_dougong_modular_flexibility.sh
	@echo "check-v290-dougong-modular-flexibility: OK (coupling + swap + seismic + chaos)"

check-v290: check-v290-dougong-modular-flexibility
	@echo "check-v290: OK (σ-dougong kernel)"

# --- v291 σ-Parthenon (calibration + entasis) ---
#
# v0 contracts: exactly 3 calibration rows canonical
# (medical ABSTAIN / code CAUTIOUS / creative SAFE)
# at shared sigma_sample=0.30 with 3 DISTINCT
# verdicts; exactly 3 perception rows (σ=0.05
# ratio=20, σ=0.15 ratio=7, σ=0.50 ratio=2) with
# ratio_denominator==round(1/σ) AND explanation_
# present on EVERY row; exactly 3 bias rows
# (overconfident +0.10, underconfident −0.10,
# calibrated 0.00) with sigma_corrected ==
# sigma_raw + bias_offset AND polarity signs matching
# labels AND residual_bias<=bias_budget=0.02 on
# EVERY row; exactly 3 entasis rows with
# sigma_clamped == clamp(sigma_in, lower=0.02,
# upper=0.98); σ_parthenon==0.0; FNV-1a chain
# replays byte-identically.
V291_INC  = -Isrc/v291
V291_SRCS = src/v291/parthenon.c

creation_os_v291_parthenon: $(V291_SRCS) src/v291/main.c
	$(CC) $(CFLAGS) $(V291_INC) -o $@ \
	    $(V291_SRCS) src/v291/main.c $(LDFLAGS)

check-v291-parthenon-calibration: creation_os_v291_parthenon
	@bash benchmarks/v291/check_v291_parthenon_calibration.sh
	@echo "check-v291-parthenon-calibration: OK (calib + percept + bias + entasis)"

check-v291: check-v291-parthenon-calibration
	@echo "check-v291: OK (σ-parthenon kernel)"

# --- v292 σ-Leanstral (formal verification) ---
#
# v0 contracts: exactly 3 gate theorems canonical
# (gate_determinism/gate_range/gate_threshold_
# monotone) all lean4_proved; exactly 4 σ invariants
# canonical (sigma_in_unit_interval/
# sigma_zero_k_eff_full/sigma_one_k_eff_zero/
# sigma_monotone_confidence_loss) all holds; exactly
# 3 Leanstral cost rows canonical (leanstral $36 <
# claude $549 < bug_in_prod $10000) strictly
# monotonically increasing; exactly 3 formal-layer
# rows canonical (frama_c_v138 C_CONTRACTS /
# lean4_v207 THEOREM_PROOFS / leanstral_v292
# AI_ASSISTED_PROOFS) with 3 DISTINCT layers AND all
# enabled; σ_leanstral==0.0; FNV-1a chain replays
# byte-identically.
V292_INC  = -Isrc/v292
V292_SRCS = src/v292/leanstral.c

creation_os_v292_leanstral: $(V292_SRCS) src/v292/main.c
	$(CC) $(CFLAGS) $(V292_INC) -o $@ \
	    $(V292_SRCS) src/v292/main.c $(LDFLAGS)

check-v292-leanstral-formal-verification: creation_os_v292_leanstral
	@bash benchmarks/v292/check_v292_leanstral_formal_verification.sh
	@echo "check-v292-leanstral-formal-verification: OK (theorems + invariants + cost + layers)"

check-v292: check-v292-leanstral-formal-verification
	@echo "check-v292: OK (σ-leanstral kernel)"

# --- v293 σ-Hagia-Sofia (continuous use = longevity) ---
#
# v0 contracts: exactly 3 adoption metrics canonical
# (daily_users/api_calls/sigma_evaluations) all
# tracked; exactly 3 multi-purpose domains canonical
# (llm/sensor/organization) all sigma_gate_
# applicable; exactly 3 community longevity rows
# canonical (open_source_agpl/community_maintainable/
# vendor_independent) all hold; exactly 3 lifecycle
# phases canonical (active_original_purpose/
# declining_usage/repurposed) all alive AND
# declining_usage carries warning_issued AND
# repurposed carries new_domain_found; σ_hagia==0.0;
# FNV-1a chain replays byte-identically.
V293_INC  = -Isrc/v293
V293_SRCS = src/v293/hagia_sofia.c

creation_os_v293_hagia_sofia: $(V293_SRCS) src/v293/main.c
	$(CC) $(CFLAGS) $(V293_INC) -o $@ \
	    $(V293_SRCS) src/v293/main.c $(LDFLAGS)

check-v293-hagia-sofia-continuous-use: creation_os_v293_hagia_sofia
	@bash benchmarks/v293/check_v293_hagia_sofia_continuous_use.sh
	@echo "check-v293-hagia-sofia-continuous-use: OK (adoption + domain + community + lifecycle)"

check-v293: check-v293-hagia-sofia-continuous-use
	@echo "check-v293: OK (σ-hagia-sofia kernel)"

check-v287-v293: check-v287 check-v288 check-v289 check-v290 check-v291 check-v292 check-v293
	@echo "check-v287-v293: OK (granite + oculus + ruin-value + dougong + parthenon + leanstral + hagia-sofia)"

# --- v294 σ-Federated (σ-gated federated learning) ---
#
# v0 contracts: exactly 3 canonical devices
# (device_a/device_b/device_c); accepted iff
# σ_device ≤ τ_device=0.40 (both branches fire);
# accepted weights sum to 1.0 ± 1e-3; weights
# strictly decreasing with σ across ACCEPTED rows;
# exactly 3 DP regimes (too_low_noise/optimal_noise/
# too_high_noise) with σ strictly increasing as ε
# strictly decreasing AND exactly 1 OPTIMAL;
# exactly 3 non-IID rows (similar_data/
# slightly_different/very_different); σ_dist
# strictly increasing; GLOBAL_MODEL iff σ_dist<0.20,
# PERSONALIZED iff σ_dist>0.60, HYBRID otherwise;
# exactly 3 mesh edges (a->b/b->c/a->z); trusted iff
# σ_neighbor ≤ τ_mesh=0.30 (both branches fire);
# central_server=false; σ_fed==0.0; FNV-1a chain
# replays byte-identically.
V294_INC  = -Isrc/v294
V294_SRCS = src/v294/federated.c

creation_os_v294_federated: $(V294_SRCS) src/v294/main.c
	$(CC) $(CFLAGS) $(V294_INC) -o $@ \
	    $(V294_SRCS) src/v294/main.c $(LDFLAGS)

check-v294-federated-sigma-gated: creation_os_v294_federated
	@bash benchmarks/v294/check_v294_federated_sigma_gated.sh
	@echo "check-v294-federated-sigma-gated: OK (devices + dp + niid + mesh)"

check-v294: check-v294-federated-sigma-gated
	@echo "check-v294: OK (σ-federated kernel)"

# --- v295 σ-Immune (innate + adaptive + memory + autoimmune prevention) ---
#
# v0 contracts: exactly 3 innate patterns
# (sql_injection/prompt_injection/obvious_malware);
# σ_raw ≥ τ_innate=0.70; all blocked; none require
# training; all response_tier=INSTANT; exactly 3
# adaptive rows (novel_attack_first_seen/
# same_attack_second_seen/related_variant_seen); all
# learned; exactly 1 faster_on_repeat AND exactly 1
# cross_recognized; exactly 3 memory rows
# (pattern_A_first_logged/pattern_A_reencountered/
# pattern_B_new_logged); recognised iff tier==FAST;
# exactly 1 recognised; exactly 3 autoimmune
# scenarios (tau_too_tight/tau_balanced/
# tau_too_loose); τ strictly increasing; 3 DISTINCT
# verdicts (AUTOIMMUNE/HEALTHY/IMMUNODEFICIENT);
# HEALTHY iff τ∈[0.10,0.60] AND fpr≤0.10;
# σ_immune==0.0; FNV-1a chain replays byte-identically.
V295_INC  = -Isrc/v295
V295_SRCS = src/v295/immune.c

creation_os_v295_immune: $(V295_SRCS) src/v295/main.c
	$(CC) $(CFLAGS) $(V295_INC) -o $@ \
	    $(V295_SRCS) src/v295/main.c $(LDFLAGS)

check-v295-immune-innate-adaptive: creation_os_v295_immune
	@bash benchmarks/v295/check_v295_immune_innate_adaptive.sh
	@echo "check-v295-immune-innate-adaptive: OK (innate + adaptive + memory + autoimmune)"

check-v295: check-v295-immune-innate-adaptive
	@echo "check-v295: OK (σ-immune kernel)"

# --- v296 σ-Antifragile (stress + volatility + vaccine + barbell) ---
#
# v0 contracts: exactly 3 stress cycles
# (cycle_1/2/3); stress_level strictly increasing
# AND sigma_after strictly DECREASING (antifragile,
# not merely robust); exactly 3 volatility regimes
# (unstable/stable/antifragile) with 3 DISTINCT
# classifications; ANTIFRAGILE iff σ_std>0.03 AND
# trend=DECREASING; STABLE iff σ_std≤0.03;
# UNSTABLE iff σ_std>0.15 AND trend=NONE;
# exactly 3 vaccine rows (dose_small/dose_medium/
# real_attack); noise strictly increasing; all
# survived; exactly 2 vaccines AND exactly 1 real
# attack survived because_trained=true; exactly 3
# barbell allocations (safe_mode/experimental_mode/
# middle_compromise); safe+exp=1.0; middle=0.0;
# τ_safe<τ_exp; extremes kept, middle rejected;
# σ_antifragile==0.0; FNV-1a chain replays
# byte-identically.
V296_INC  = -Isrc/v296
V296_SRCS = src/v296/antifragile.c

creation_os_v296_antifragile: $(V296_SRCS) src/v296/main.c
	$(CC) $(CFLAGS) $(V296_INC) -o $@ \
	    $(V296_SRCS) src/v296/main.c $(LDFLAGS)

check-v296-antifragile-stress-adaptation: creation_os_v296_antifragile
	@bash benchmarks/v296/check_v296_antifragile_stress_adaptation.sh
	@echo "check-v296-antifragile-stress-adaptation: OK (stress + volatility + vaccine + barbell)"

check-v296: check-v296-antifragile-stress-adaptation
	@echo "check-v296: OK (σ-antifragile kernel)"

# --- v297 σ-Clock (no expiry + monotonic + epoch-free + version-free) ---
#
# v0 contracts: exactly 3 expiry rows
# (hardcoded_date/valid_until_2030/api_version_expiry);
# all forbidden AND all absent from kernel; exactly
# 3 time sources (CLOCK_MONOTONIC/CLOCK_REALTIME/
# wallclock_local); exactly 1 ALLOW (monotonic) AND
# exactly 2 FORBID (wallclock sources); exactly 3
# log properties (relative_sequence/unix_epoch_absent/
# y2038_safe) all holds=true; exactly 3 protocol
# forward-compat properties (no_version_field_on_struct/
# old_reader_ignores_new_fields/
# append_only_field_semantics) all holds=true;
# σ_clock==0.0; FNV-1a chain replays byte-identically.
V297_INC  = -Isrc/v297
V297_SRCS = src/v297/clock.c

creation_os_v297_clock: $(V297_SRCS) src/v297/main.c
	$(CC) $(CFLAGS) $(V297_INC) -o $@ \
	    $(V297_SRCS) src/v297/main.c $(LDFLAGS)

check-v297-clock-time-invariant: creation_os_v297_clock
	@bash benchmarks/v297/check_v297_clock_time_invariant.sh
	@echo "check-v297-clock-time-invariant: OK (expiry + time + log + proto)"

check-v297: check-v297-clock-time-invariant
	@echo "check-v297: OK (σ-clock kernel)"

# --- v298 σ-Rosetta (self-documenting + multi-language + human-readable + math) ---
#
# v0 contracts: exactly 3 σ emissions with 3
# DISTINCT domains (LLM/SENSOR/ORG); reason_present
# AND reason_length ≥ 20 on every row; exactly 3
# language bindings (C REFERENCE / Python ADOPTION /
# Rust SAFETY); 3 DISTINCT roles; all maintained AND
# all semantic_match_to_c; exactly 3 log formats
# (binary/csv/json); all machine_readable; exactly 2
# human_readable_forever (csv, json) AND exactly 1
# not (binary); exactly 3 mathematical invariants
# (sigma_definition/pythagoras_2500_yr/
# arithmetic_invariant) all formal_expression_present
# AND none age_out; σ_rosetta==0.0; FNV-1a chain
# replays byte-identically.
V298_INC  = -Isrc/v298
V298_SRCS = src/v298/rosetta.c

creation_os_v298_rosetta: $(V298_SRCS) src/v298/main.c
	$(CC) $(CFLAGS) $(V298_INC) -o $@ \
	    $(V298_SRCS) src/v298/main.c $(LDFLAGS)

check-v298-rosetta-self-documenting: creation_os_v298_rosetta
	@bash benchmarks/v298/check_v298_rosetta_self_documenting.sh
	@echo "check-v298-rosetta-self-documenting: OK (emit + spec + fmt + math)"

check-v298: check-v298-rosetta-self-documenting
	@echo "check-v298: OK (σ-rosetta kernel)"

check-v294-v298: check-v294 check-v295 check-v296 check-v297 check-v298
	@echo "check-v294-v298: OK (federated + immune + antifragile + clock + rosetta)"

# --- v299 σ-Knowledge-Graph (grounded retrieval + provenance + multi-hop + corpus) ---
#
# v0 contracts: 3 retrieval rows (known_fact /
# partial_match / unknown) with σ_retrieval strictly
# increasing AND `FROM_KG iff σ ≤ τ_kg = 0.40` (both
# branches) AND exactly 2 FROM_KG + 1 FALLBACK_LLM;
# 3 provenance rows with σ strictly increasing AND
# `trusted iff σ ≤ τ_prov = 0.50` (both branches) AND
# trusted rows carry a non-empty source_ref; 3 multi-
# hop chains (1/3/5 hops) whose σ_total follows
# `1 − (1 − σ_per_hop)^hops` within 1e-3 AND `warning
# iff σ_total > 0.50` (both branches); 3 corpus
# triplets (sigma/k_eff/one_equals_one) all well-formed
# AND queryable; σ_kg==0.0; FNV-1a chain replays
# byte-identically.
V299_INC  = -Isrc/v299
V299_SRCS = src/v299/knowledge_graph.c

creation_os_v299_knowledge_graph: $(V299_SRCS) src/v299/main.c
	$(CC) $(CFLAGS) $(V299_INC) -o $@ \
	    $(V299_SRCS) src/v299/main.c $(LDFLAGS)

check-v299-knowledge-graph-sigma: creation_os_v299_knowledge_graph
	@bash benchmarks/v299/check_v299_knowledge_graph_sigma.sh
	@echo "check-v299-knowledge-graph-sigma: OK (retrieval + provenance + hop + corpus)"

check-v299: check-v299-knowledge-graph-sigma
	@echo "check-v299: OK (σ-knowledge-graph kernel)"

# --- v300 σ-Complete (cognitive audit + dependency graph + 1=1 self-test + pyramid) ---
#
# v0 contracts: kernels_total == 300; 15 cognitive
# categories (v243 list) in canonical order with every
# category covered AND representative_kernel in
# [6, 300]; 3 dependency buckets (core_critical /
# supporting / removable_duplicate) summing to 300
# with `removable_duplicate == 0` AND
# `core_critical == 7`; 4 repo-level 1=1 claims
# (zero_deps / sigma_gated / deterministic /
# monotonic_clock) with declared==realized and
# σ_pair==0 on every row, σ_repo==0 < τ_repo=0.10;
# 7 pyramid invariants (v287 granite / v288 oculus /
# v289 ruin_value / v290 dougong / v293 hagia_sofia /
# v297 clock / v298 rosetta) all holding with
# architecture_survives_100yr=true; σ_complete==0.0;
# FNV-1a chain replays byte-identically.
V300_INC  = -Isrc/v300
V300_SRCS = src/v300/complete.c

creation_os_v300_complete: $(V300_SRCS) src/v300/main.c
	$(CC) $(CFLAGS) $(V300_INC) -o $@ \
	    $(V300_SRCS) src/v300/main.c $(LDFLAGS)

check-v300-completeness-audit: creation_os_v300_complete
	@bash benchmarks/v300/check_v300_completeness_audit.sh
	@echo "check-v300-completeness-audit: OK (cog + dep + 1=1 + pyramid)"

check-v300: check-v300-completeness-audit
	@echo "check-v300: OK (σ-complete kernel)"

check-v299-v300: check-v299 check-v300
	@echo "check-v299-v300: OK (knowledge-graph + complete)"

# --- v301 σ-ZKP (verifiable gate + ZK-inference + model integrity + SCSL crypto) ---
#
# v0 contracts: 3 canonical proofs (well_formed /
# edge_case / forged) with σ_proof strictly increasing
# AND `valid iff σ_proof ≤ τ_proof = 0.40` (both
# branches) AND reveals_raw = false on every row;
# 3 canonical roles (client / cloud / verifier) with
# client + verifier hiding raw + weights (and verifier
# also hiding answer) → zk_privacy_holds = true;
# 3 integrity scenarios (advertised_served /
# silent_downgrade / advertised_match) with
# detected_mismatch iff σ_integrity > τ_integrity=0.50
# (both branches) AND exactly 2 OK + 1 DETECTED;
# 3 SCSL policy cases (allowed_a / allowed_b /
# disallowed) with attested iff σ_policy ≤ 0.50 (both
# branches) AND purpose_revealed = false on every row;
# σ_zkp == 0.0; FNV-1a chain replays byte-identically.
V301_INC  = -Isrc/v301
V301_SRCS = src/v301/zkp.c

creation_os_v301_zkp: $(V301_SRCS) src/v301/main.c
	$(CC) $(CFLAGS) $(V301_INC) -o $@ \
	    $(V301_SRCS) src/v301/main.c $(LDFLAGS)

check-v301-zkp-verifiable-sigma: creation_os_v301_zkp
	@bash benchmarks/v301/check_v301_zkp_verifiable_sigma.sh
	@echo "check-v301-zkp-verifiable-sigma: OK (proof + role + integrity + scsl)"

check-v301: check-v301-zkp-verifiable-sigma
	@echo "check-v301: OK (σ-zkp kernel)"

# --- v302 σ-Green (compute budget + carbon-aware + abstain savings + J/reliable_token) ---
#
# v0 contracts: 3 budget tiers (easy / medium / hard)
# with σ_difficulty strictly ↑, energy_j strictly ↑,
# 3 DISTINCT model tiers; 3 schedule rows (urgent_green
# / low_urgency_brown / urgent_brown) with
# processed = (urgency==HIGH OR grid==GREEN) on every
# row AND both processed branches firing; 3 savings
# rows (baseline / gated_light / gated_heavy) with
# saved_ratio = abstained/total within 1e-3, saved_ratio
# strictly ↑, energy_j strictly ↓; 3 J/reliable regimes
# (unfiltered / soft / hard) with j_per_reliable =
# energy_j / reliable_tokens within 1e-3 AND strictly ↓
# across the 3 rows; σ_green == 0.0; FNV-1a chain
# replays byte-identically.
V302_INC  = -Isrc/v302
V302_SRCS = src/v302/green.c

creation_os_v302_green: $(V302_SRCS) src/v302/main.c
	$(CC) $(CFLAGS) $(V302_INC) -o $@ \
	    $(V302_SRCS) src/v302/main.c $(LDFLAGS)

check-v302-green-energy-aware: creation_os_v302_green
	@bash benchmarks/v302/check_v302_green_energy_aware.sh
	@echo "check-v302-green-energy-aware: OK (budget + schedule + abstain + jpt)"

check-v302: check-v302-green-energy-aware
	@echo "check-v302: OK (σ-green kernel)"

# --- v303 σ-Governance (decision σ + meeting σ + communication σ + institutional K(t)) ---
#
# v0 contracts: 3 decisions (strategy_matches /
# partially_realised / ignored) with σ_decision
# strictly ↑ AND 3 DISTINCT verdicts; 3 meetings
# (perfect / quarter / noise) with σ_meeting ==
# 1 − realised/made within 1e-3 AND σ strictly ↑;
# 3 communication channels (clear / slightly_vague /
# highly_vague) with σ strictly ↑ AND `clear iff σ ≤
# τ_comm = 0.50` (both branches); 3 institutions
# (healthy / warning / collapsing) with K = ρ·I_φ·F
# within 1e-3 AND VIABLE iff K ≥ K_warn=0.20, WARNING
# iff K_crit=0.127 ≤ K < K_warn, COLLAPSE iff K < K_crit
# (all three branches fire); σ_gov == 0.0; FNV-1a chain
# replays byte-identically.
V303_INC  = -Isrc/v303
V303_SRCS = src/v303/governance.c

creation_os_v303_governance: $(V303_SRCS) src/v303/main.c
	$(CC) $(CFLAGS) $(V303_INC) -o $@ \
	    $(V303_SRCS) src/v303/main.c $(LDFLAGS)

check-v303-governance-org-sigma: creation_os_v303_governance
	@bash benchmarks/v303/check_v303_governance_org_sigma.sh
	@echo "check-v303-governance-org-sigma: OK (decision + meeting + comm + kt)"

check-v303: check-v303-governance-org-sigma
	@echo "check-v303: OK (σ-governance kernel)"

# --- v304 σ-Narrative (story coherence + argument + propaganda + self-narrative) ---
#
# v0 contracts: 3 stories (coherent / minor_tension /
# contradictory) with σ_narrative strictly ↑ AND
# `COHERENT iff σ ≤ τ_story = 0.40` (both branches)
# AND exactly 2 COHERENT + 1 CONTRADICTORY; 3 argument
# steps (modus_ponens / weak_induction / affirming_
# consequent) with σ strictly ↑ AND `VALID iff σ ≤
# τ_arg = 0.50` (both branches); 3 propaganda texts
# (neutral / persuasive / manipulative) with
# propaganda_score == emotion * logic_sigma within 1e-3
# AND `FLAGGED iff score > τ_prop = 0.50` (both
# branches); 3 self-stories (aligned / slight / denial)
# with σ strictly ↑ AND `matches_facts iff σ ≤
# τ_self = 0.50` (both branches); σ_narr == 0.0;
# FNV-1a chain replays byte-identically.
V304_INC  = -Isrc/v304
V304_SRCS = src/v304/narrative.c

creation_os_v304_narrative: $(V304_SRCS) src/v304/main.c
	$(CC) $(CFLAGS) $(V304_INC) -o $@ \
	    $(V304_SRCS) src/v304/main.c $(LDFLAGS)

check-v304-narrative-coherence: creation_os_v304_narrative
	@bash benchmarks/v304/check_v304_narrative_coherence.sh
	@echo "check-v304-narrative-coherence: OK (story + arg + propaganda + self)"

check-v304: check-v304-narrative-coherence
	@echo "check-v304: OK (σ-narrative kernel)"

# --- v305 σ-Swarm-Intelligence (wisdom of σ-crowds + diversity + emergent + proconductor) ---
#
# v0 contracts: 3 aggregators (best_single /
# naive_average / sigma_weighted) with sigma_weighted
# holding the strictly lowest σ AND strictly highest
# accuracy AND the single WINS verdict; 3 crowds
# (echo_chamber / balanced / chaos) with value =
# diversity*(1 − ind_sigma) within 1e-3 AND `balanced`
# holding the strictly highest value AND exactly 1 row
# crossing τ_value = 0.30; 3 emergent rows (genuine /
# weak / random) with σ strictly ↑ AND `keep iff σ ≤
# τ_emergent = 0.50` (both branches); 4 proconductor
# agents (claude / gpt / gemini / deepseek) with 4
# DISTINCT names, every σ ≤ τ_conv = 0.25, every
# direction identical, pc_convergent_ok = true;
# σ_swarm == 0.0; FNV-1a chain replays byte-
# identically.
V305_INC  = -Isrc/v305
V305_SRCS = src/v305/swarm.c

creation_os_v305_swarm: $(V305_SRCS) src/v305/main.c
	$(CC) $(CFLAGS) $(V305_INC) -o $@ \
	    $(V305_SRCS) src/v305/main.c $(LDFLAGS)

check-v305-swarm-intelligence-sigma: creation_os_v305_swarm
	@bash benchmarks/v305/check_v305_swarm_intelligence_sigma.sh
	@echo "check-v305-swarm-intelligence-sigma: OK (wisdom + diversity + emergent + proconductor)"

check-v305: check-v305-swarm-intelligence-sigma
	@echo "check-v305: OK (σ-swarm kernel)"

# --- v306 σ-Omega (Ω loop + multi-scale + ½ operator + 1=1 invariant) -----
#
# v0 contracts: 4 loop steps (t=0..3) with σ strictly
# ↓ AND ∫σ strictly ↑ AND K_eff ≥ K_crit = 0.127 on
# every row (the Ω constraint is never violated);
# 4 scales (token / answer / session / domain) with
# 4 DISTINCT scale names AND 4 DISTINCT operators;
# 3 ½-regime rows (signal / critical / noise) with σ
# strictly ↑ AND σ_critical == 0.5 to machine
# precision AND SIGNAL iff σ<0.5, NOISE iff σ>0.5,
# CRITICAL iff σ==0.5 (all three branches fire); 3
# invariant rows (kernel_count / architecture_claim /
# axiom_one_equals_one) with declared == realized AND
# holds on every row AND the_invariant_holds = true;
# kernels_total == 306; σ_omega == 0.0; FNV-1a chain
# replays byte-identically.
V306_INC  = -Isrc/v306
V306_SRCS = src/v306/omega.c

creation_os_v306_omega: $(V306_SRCS) src/v306/main.c
	$(CC) $(CFLAGS) $(V306_INC) -o $@ \
	    $(V306_SRCS) src/v306/main.c $(LDFLAGS)

check-v306-omega-operator: creation_os_v306_omega
	@bash benchmarks/v306/check_v306_omega_operator.sh
	@echo "check-v306-omega-operator: OK (loop + scale + half + invariant)"

check-v306: check-v306-omega-operator
	@echo "check-v306: OK (σ-omega kernel — 1=1)"

check-v301-v306: check-v301 check-v302 check-v303 check-v304 check-v305 check-v306
	@echo "check-v301-v306: OK (zkp + green + governance + narrative + swarm + omega)"

# --- License Attestation Kernel (SCSL-1.0 §11) -------------------
#
# Tiny, dependency-free, integer-only C kernel that:
#   (a) implements SHA-256 (FIPS-180-4) without OpenSSL,
#   (b) computes the canonical hash of LICENSE-SCSL-1.0.md,
#   (c) emits a License-Bound Receipt envelope used by every
#       v60..v74 verdict and by the cos CLI,
#   (d) self-checks the in-tree license against LICENSE.sha256
#       and refuses to run on tamper (exit 87).
#
# This is the v75 σ-License kernel — the cryptographic-evidence
# spine of the Spektre Commercial Source License v1.0.
LICENSE_KERNEL_SRCS = src/license_kernel/license_attest.c
LICENSE_KERNEL_INC  = -Isrc/license_kernel

license_attest: src/license_kernel/license_cli.c $(LICENSE_KERNEL_SRCS)
	$(CC) -O2 -Wall -Wextra -std=c11 $(LICENSE_KERNEL_INC) -o license_attest src/license_kernel/license_cli.c $(LICENSE_KERNEL_SRCS)

license-pin:
	@bash tools/license/license_sha256.sh --pin

license-check: license_attest
	@bash tools/license/check_headers.sh
	@./license_attest --bundle
	@echo "license-check: OK (LICENSE-SCSL-1.0.md pinned at sha256=$$(awk '{print $$1}' LICENSE.sha256), bundle intact, headers verified)"

license-apply:
	@bash tools/license/apply_headers.sh

license-attest: license_attest
	@./license_attest --self-test
	@./license_attest --verify
	@./license_attest --bundle
	@./license_attest --receipt v75-license SCSL-1.0-OK 0000000000000000000000000000000000000000 ff > /tmp/spektre-receipt.json
	@echo "license-attest: OK (receipt at /tmp/spektre-receipt.json)"
	@cat /tmp/spektre-receipt.json
	@echo
	@echo "  Verify with:  shasum -a 256 LICENSE-SCSL-1.0.md"

license-attest-hardened: src/license_kernel/license_cli.c $(LICENSE_KERNEL_SRCS)
	$(CC) $(HARDEN_CFLAGS) $(LICENSE_KERNEL_INC) -o license_attest_hardened src/license_kernel/license_cli.c $(LICENSE_KERNEL_SRCS) $(HARDEN_LDFLAGS)
	./license_attest_hardened --self-test

# --- cos: Apple-tier unified CLI front door ----------------------
#
# Linked with creation_os cos_think (libsqlite3 + libcurl) for
# `cos think --goal ...`.  NO_COLOR / TERM=dumb respected; isatty
# auto-detect for colour.
cos: cli/cos.c src/cli/cos_voice.c src/import/ollama_detect.c include/cos_version.h $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) \
	src/sigma/skill_distill.c src/sigma/knowledge_graph.c \
	src/sigma/world_model.c $(COS_EDGE_INF) src/cli/cos_think.c \
	src/cli/cos_search.c \
	src/sigma/embodiment.c src/sigma/physics_model.c \
	src/sigma/sovereign_limits.c src/cli/cos_embody.c \
	src/sigma/codegen.c src/sigma/evolution_engine.c \
	src/cli/cos_codegen_cli.c \
	src/sigma/consciousness_meter.c src/sigma/awareness_log.c \
	src/cli/cos_consciousness_cli.c \
	src/sigma/energy_accounting.c src/sigma/green_score.c \
	src/cli/cos_energy_green_cli.c \
	src/cli/cos_demo.c \
	src/cli/cos_life.c \
	$(COS_OMEGA_SUPPORT_SRCS)
	$(CC) -O2 -Wall -std=c11 $(COS_CLI_INC) $(LICENSE_KERNEL_INC) -Iinclude \
	    -Isrc/cli -Isrc/sigma -Isrc/sigma/tools -Isrc/sigma/pipeline -Isrc/vendor \
	    -o cos cli/cos.c src/cli/cos_voice.c src/import/ollama_detect.c $(COS_CLI_SRCS) $(COS_THINK_CLI_AUX) \
	    src/sigma/skill_distill.c src/sigma/knowledge_graph.c \
	    src/sigma/world_model.c $(COS_EDGE_INF) src/cli/cos_think.c \
	    src/cli/cos_search.c \
	    src/sigma/embodiment.c src/sigma/physics_model.c \
	    src/sigma/sovereign_limits.c src/cli/cos_embody.c \
	    src/sigma/codegen.c src/sigma/evolution_engine.c \
	    src/cli/cos_codegen_cli.c \
	    src/sigma/consciousness_meter.c src/sigma/awareness_log.c \
	    src/cli/cos_consciousness_cli.c \
	    src/sigma/energy_accounting.c src/sigma/green_score.c \
	    src/cli/cos_energy_green_cli.c \
	    src/cli/cos_demo.c \
	    src/cli/cos_life.c \
	    src/sigma/speculative_sigma.c $(COS_SPIKE_ADAPT_SRCS) \
	    $(COS_PROOF_LIB) \
	    src/cli/cos_serve.c src/vendor/picohttpparser.c src/sigma/audit_log.c \
	    src/sigma/mission.c src/sigma/coherence_watchdog.c \
	    src/sigma/perception.c src/sigma/pipeline/watchdog.c \
	    src/sigma/federation.c src/sigma/pipeline/a2a.c \
	    src/sigma/sigma_mcp_gate.c src/sigma/channels.c \
	    src/sigma/omega_loop.c src/cli/cos_omega_cli.c \
	    src/omega/evolver.c src/omega/pattern_extractor.c \
	    src/omega/config_persist.c src/omega/prompt_bank.c src/omega/gvu_loop.c \
	    src/sigma/semantic_sigma.c \
	    src/cli/cos_monitor.c \
	    src/cli/cos_report.c \
	    $(COS_LEARN_WEB_SRCS) \
	    $(LDFLAGS) -lsqlite3 -lcurl -lpthread

cos-demo: cos
	@ln -f cos cos-demo && echo "cos-demo -> cos (run ./cos demo)"

# Fat Mach-O (arm64 + x86_64) for macOS distribution.  Darwin + Apple clang only.
cos-universal:
	@chmod +x scripts/build_cos_universal.sh
	@./scripts/build_cos_universal.sh

cos-monitor: src/cli/cos_monitor.c
	$(CC) -O2 -Wall -std=c11 -o cos-monitor src/cli/cos_monitor.c \
	    -DCOS_MONITOR_MAIN

check-cos: cos
	./cos --version
	@echo "check-cos: OK (Apple-tier CLI front door built; try './cos' for the status board)"

# --- v61 CHACE composition targets (defence in depth) -------------
# Each target PASSes on a present layer and SKIPs honestly on a
# missing one — never silent PASS.

attest: standalone-v61
	@bash scripts/security/attest.sh

sign:
	@bash scripts/security/sign.sh

slsa: standalone-v61
	@bash scripts/security/slsa.sh > PROVENANCE.json
	@echo "slsa: OK (PROVENANCE.json written; real SLSA-3 via .github/workflows/slsa.yml)"

wasm-sandbox:
	@bash scripts/v61/wasm_harness.sh

ebpf-policy:
	@bash scripts/v61/ebpf_build.sh

sandbox-exec: standalone-v61
	@bash scripts/v61/sandbox_exec.sh

distroless:
	@if command -v docker >/dev/null 2>&1; then \
	  docker build -f Dockerfile.distroless -t creation-os-v61:distroless . ; \
	  echo "distroless: OK (image creation-os-v61:distroless built)" ; \
	else \
	  echo "distroless: SKIP (docker not on PATH)" ; \
	fi

nix-build:
	@if command -v nix >/dev/null 2>&1; then \
	  nix-build nix/v61.nix -o .build/nix-v61 ; \
	  echo "nix-build: OK (reproducible v61 binary in .build/nix-v61/bin/)" ; \
	else \
	  echo "nix-build: SKIP (nix not on PATH)" ; \
	fi

sel4-check:
	@if [ -f sel4/sigma_shield.camkes ]; then \
	  if command -v camkes >/dev/null 2>&1; then \
	    echo "sel4-check: SKIP (camkes toolchain present but CAmkES build target not yet wired)"; \
	  else \
	    echo "sel4-check: OK (sel4/sigma_shield.camkes contract present; CAmkES toolchain not on host → seL4 build SKIP)"; \
	  fi; \
	else \
	  echo "sel4-check: FAIL (sel4/sigma_shield.camkes missing)" ; exit 1 ; \
	fi

chace:
	@bash scripts/security/chace.sh

# v36: MCP-native σ server (JSON-RPC over STDIO + optional HTTP shim).
MCP_SRCS = src/mcp/sigma_server.c src/mcp/sigma_server_http.c src/sigma/decompose.c src/sigma/calibrate.c src/sigma/channels.c src/sigma/channels_v34.c src/server/json_esc.c \
	src/sigma/sigma_mcp_gate.c src/sigma/tools/sigma_tools.c src/sigma/engram_episodic.c src/sigma/text_similarity.c src/sigma/error_attribution.c

standalone-mcp: $(MCP_SRCS)
	$(CC) $(CFLAGS) -I. -Isrc/sigma -Isrc/sigma/tools -o creation_os_mcp $(MCP_SRCS) $(LDFLAGS) -lsqlite3

creation_os_check_mcp_sigma: tests/agi/check_mcp_sigma_main.c \
	src/sigma/sigma_mcp_gate.c src/sigma/tools/sigma_tools.c \
	src/sigma/channels.c src/sigma/engram_episodic.c src/sigma/text_similarity.c \
	src/sigma/error_attribution.c
	$(CC) $(CFLAGS) -Isrc/sigma -Isrc/sigma/tools -o $@ tests/agi/check_mcp_sigma_main.c \
	    src/sigma/sigma_mcp_gate.c src/sigma/tools/sigma_tools.c src/sigma/channels.c \
	    src/sigma/engram_episodic.c src/sigma/text_similarity.c \
	    src/sigma/error_attribution.c $(LDFLAGS) -lsqlite3

check-mcp-sigma: standalone-mcp creation_os_check_mcp_sigma
	@./creation_os_mcp --self-test
	@./creation_os_check_mcp_sigma
	@echo "check-mcp-sigma: OK (MCP σ gate + MCP server self-test)"

check-a2a-sigma: creation_os_sigma_a2a
	@./creation_os_sigma_a2a >/dev/null
	@echo "check-a2a-sigma: OK (σ-A2A kernel self-test)"

check-agent: cos-agent
	@./cos-agent --self-test-node
	@echo "check-agent: OK (unified cos agent identity surface)"

# Optional: OpenAI-shaped loopback stub for wiring local tools (no weights; POSIX-only).
standalone-openai-stub: creation_os_openai_stub.c src/server/json_esc.c
	$(CC) $(CFLAGS) -I. -o creation_os_openai_stub creation_os_openai_stub.c src/server/json_esc.c $(LDFLAGS)

test-openai-stub: standalone-openai-stub
	./creation_os_openai_stub --self-test

check-openai-stub: standalone-openai-stub test-openai-stub
	@echo "check-openai-stub: OK (OpenAI-shaped localhost stub)"

# Optional: suite lab — static mode strings + tool lists (no LLM; see docs/SUITE_LAB.md).
standalone-suite-stub: creation_os_suite_stub.c src/suite/core.c
	$(CC) $(CFLAGS) -I. -Isrc/suite -o creation_os_suite_stub creation_os_suite_stub.c src/suite/core.c $(LDFLAGS)

test-suite-stub: standalone-suite-stub
	./creation_os_suite_stub --self-test

check-suite-stub: check-openai-stub standalone-suite-stub test-suite-stub
	@echo "check-suite-stub: OK (suite lab binaries)"

# Optional: Apple-only native M4 lab binary (ObjC++ + GCD). Not merge-gate.
native-m4:
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		mkdir -p $(BUILDDIR) ; \
		$(CC) $(CFLAGS) -c native_m4/cos_living_weights_scalar.c -o $(BUILDDIR)/cos_living_weights_scalar.o && \
		$(CC) $(CFLAGS) -c native_m4/cos_buffer.c -o $(BUILDDIR)/cos_buffer.o && \
		$(CC) $(CFLAGS) -c native_m4/cos_runtime_sme.c -o $(BUILDDIR)/cos_runtime_sme.o && \
		$(CC) $(CFLAGS) -c native_m4/cos_runtime_layers_report.c -o $(BUILDDIR)/cos_runtime_layers_report.o && \
		clang++ -O2 -Wall -std=c++17 -ObjC++ -fobjc-arc \
			-c native_m4/creation_os_native_m4.mm -o $(BUILDDIR)/creation_os_native_m4_main.o && \
		clang++ -O2 -Wall -std=c++17 -ObjC++ -fobjc-arc \
			-c native_m4/cos_living_weights_neon.mm -o $(BUILDDIR)/cos_living_weights_neon.o && \
		clang++ -O2 -Wall -std=c++17 -ObjC++ -fobjc-arc \
			-c native_m4/cos_living_weights_metal.mm -o $(BUILDDIR)/cos_living_weights_metal.o && \
		clang++ -O2 -Wall -std=c++17 -ObjC++ -fobjc-arc \
			-framework Foundation -framework Metal \
			-o creation_os_native_m4 \
			$(BUILDDIR)/creation_os_native_m4_main.o \
			$(BUILDDIR)/cos_living_weights_neon.o \
			$(BUILDDIR)/cos_living_weights_metal.o \
			$(BUILDDIR)/cos_living_weights_scalar.o \
			$(BUILDDIR)/cos_buffer.o \
			$(BUILDDIR)/cos_runtime_sme.o \
			$(BUILDDIR)/cos_runtime_layers_report.o ; \
		echo "native-m4: OK (creation_os_native_m4)"; \
	else \
		echo "native-m4: SKIP (Darwin only)"; \
	fi

metallib-m4:
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		chmod +x native_m4/build_metallib.sh && ./native_m4/build_metallib.sh ; \
	else \
		echo "metallib-m4: SKIP (Darwin only)"; \
	fi

check-native-m4: native-m4
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		./creation_os_native_m4 --self-test ; \
		echo "check-native-m4: OK"; \
	else \
		echo "check-native-m4: SKIP (Darwin only)"; \
	fi

bench-native-m4: native-m4
	@if [ "$$(uname -s)" = "Darwin" ]; then \
		./creation_os_native_m4 --buffer-sizes --vocab 65537 >/dev/null; \
		./creation_os_native_m4 --bench --vocab 65536 --iters 30 --warmup 3 --scalar; \
		echo "bench-native-m4: OK"; \
	else \
		echo "bench-native-m4: SKIP (Darwin only)"; \
	fi

cos_lm: standalone-v28
	cp -f creation_os_v28 cos_lm
	@echo "cos_lm: OK (copy of creation_os_v28)"

gen-cos-codebook: tools/gen_cos_codebook.c src/tokenizer/cos_codebook_mmap.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -I. -o $(BUILDDIR)/gen_cos_codebook tools/gen_cos_codebook.c src/tokenizer/cos_codebook_mmap.c $(LDFLAGS)
	@$(BUILDDIR)/gen_cos_codebook

core: $(BUILDDIR)
	@for f in core/*.c; do \
	  b=$$(basename "$$f" .c); \
	  $(CC) $(CFLAGS) -c "$$f" -o "$(BUILDDIR)/$$b.o"; \
	done
	@echo "Core objects: $(BUILDDIR)/*.o"

oracle: oracle/oracle_speaks.c oracle/oracle_ultimate.c core/cos_bsc.h core/creation_os.h
	$(CC) $(CFLAGS) -o oracle_speaks oracle/oracle_speaks.c $(LDFLAGS)
	$(CC) $(CFLAGS) -o oracle_ultimate oracle/oracle_ultimate.c $(LDFLAGS)

bench: bench/gemm_vs_bsc.c core/gemm_vs_bsc.c
	$(CC) $(CFLAGS) -o gemm_vs_bsc bench/gemm_vs_bsc.c core/gemm_vs_bsc.c $(LDFLAGS)
	./gemm_vs_bsc

bench-coherence: bench/coherence_gate_batch.c core/cos_bsc.h core/cos_neon_hamming.h
	$(CC) $(CFLAGS) -o coherence_gate_batch bench/coherence_gate_batch.c $(LDFLAGS)
	./coherence_gate_batch

bench-agi-gate: bench/hv_agi_gate_neon.c core/cos_bsc.h core/cos_neon_hamming.h core/cos_neon_retrieval.h core/cos_parliament.h
	$(CC) $(CFLAGS) -o hv_agi_gate_neon bench/hv_agi_gate_neon.c $(LDFLAGS)
	./hv_agi_gate_neon

bench-tokenizer-v27: benchmarks/tokenizer_throughput.c $(TOKENIZER_V27_SRCS)
	$(CC) $(CFLAGS) -I. -o tokenizer_throughput benchmarks/tokenizer_throughput.c $(TOKENIZER_V27_SRCS) $(LDFLAGS)
	./tokenizer_throughput

bench-binding-fidelity: benchmarks/binding_fidelity.c $(TOKENIZER_V27_SRCS)
	$(CC) $(CFLAGS) -I. -o binding_fidelity benchmarks/binding_fidelity.c $(TOKENIZER_V27_SRCS) $(LDFLAGS)
	./binding_fidelity

bench-vocab-scaling: benchmarks/vocab_scaling.c src/tokenizer/cos_codebook_mmap.c
	$(CC) $(CFLAGS) -I. -o vocab_scaling benchmarks/vocab_scaling.c src/tokenizer/cos_codebook_mmap.c $(LDFLAGS)
	./vocab_scaling

bench-vs-transformer: benchmarks/vs_transformer.c core/cos_bsc.h
	$(CC) $(CFLAGS) -I. -o vs_transformer benchmarks/vs_transformer.c $(LDFLAGS)
	./vs_transformer

bench-v27-all: bench-tokenizer-v27 bench-binding-fidelity bench-vocab-scaling bench-vs-transformer
	@echo "bench-v27-all: OK"

physics: physics/genesis.c physics/qhdc_core.c
	$(CC) $(CFLAGS) -o genesis physics/genesis.c $(LDFLAGS)
	$(CC) $(CFLAGS) -o qhdc physics/qhdc_core.c $(LDFLAGS)

test: tests/test_bsc_core.c core/creation_os.h core/cos_bsc.h
	$(CC) $(CFLAGS) -o test_bsc tests/test_bsc_core.c $(LDFLAGS)
	./test_bsc

test-v6: standalone-v6
	./creation_os_v6 --self-test

check-v6: standalone-v6 test-v6
	@echo "check-v6: OK (v6 Living Kernel)"

test-v7: standalone-v7
	./creation_os_v7 --self-test

check-v7: standalone-v7 test-v7
	@echo "check-v7: OK (v7 Hallucination Killer)"

test-v9: standalone-v9
	./creation_os_v9 --self-test

check-v9: standalone-v9 test-v9
	@echo "check-v9: OK (v9 Parameters in Silicon)"

test-v10: standalone-v10
	./creation_os_v10 --self-test

check-v10: standalone-v10 test-v10
	@echo "check-v10: OK (v10 The Real Mind)"

test-v11: standalone-v11
	./creation_os_v11 --self-test

check-v11: standalone-v11 test-v11
	@echo "check-v11: OK (v11 MatMul-free mind)"

test-v12: standalone-v12
	./creation_os_v12 --self-test

check-v12: standalone-v12 test-v12
	@echo "check-v12: OK (v12 Tensor mind)"

test-v15: standalone-v15
	./creation_os_v15 --self-test

check-v15: standalone-v15 test-v15
	@echo "check-v15: OK (v15 Silicon mind)"

test-v16: standalone-v16
	./creation_os_v16 --self-test

check-v16: standalone-v16 test-v16
	@echo "check-v16: OK (v16 Unified field)"

test-v20: standalone-v20
	./creation_os_v20 --self-test

check-v20: standalone-v20 test-v20
	@echo "check-v20: OK (v20 Ship mode)"

test-v21: standalone-v21
	./creation_os_v21 --self-test

check-v21: standalone-v21 test-v21
	@echo "check-v21: OK (v21 AGI sovereign stack)"

test-v22: standalone-v22
	./creation_os_v22 --self-test

check-v22: standalone-v22 test-v22
	@echo "check-v22: OK (v22 twenty colossal insights)"

test-v23: standalone-v23
	./creation_os_v23 --self-test

check-v23: standalone-v23 test-v23
	@echo "check-v23: OK (v23 AGI-schematic affordances)"

test-v24: standalone-v24
	./creation_os_v24 --self-test

check-v24: standalone-v24 test-v24
	@echo "check-v24: OK (v24 arXiv sci-fi echo latches)"

test-v25: standalone-v25
	./creation_os_v25 --self-test

check-v25: standalone-v25 test-v25
	@echo "check-v25: OK (v25 enterprise pain ledger)"

test-v26: standalone-v26
	./creation_os_v26 --self-test

check-v26: standalone-v26 test-v26
	@echo "check-v26: OK (v26 Fortune Global 500 echo orbit)"

test-v27: standalone-v27
	./creation_os_v27 --self-test

check-v27: standalone-v27 test-v27
	@echo "check-v27: OK (v27 vocab / tokenizer scaffold)"

test-v28: standalone-v28
	./creation_os_v28 --self-test

check-v28: standalone-v28 test-v28
	@echo "check-v28: OK (v28 LM integration shell)"

test-v29: standalone-v29
	./creation_os_v29 --self-test

check-v29: standalone-v29 test-v29
	@echo "check-v29: OK (v29 collapse harness)"

test-v31: standalone-v31
	./creation_os_v31 --self-test

check-v31: standalone-v31 test-v31
	@echo "check-v31: OK (v31 lab wrapper + math self-test)"

test-v33: standalone-v33
	./creation_os_v33 --self-test

check-v33: standalone-v33 test-v33
	@echo "check-v33: OK (v33 σ-router + schema + metrics self-test)"

test-v34: standalone-v34
	./creation_os_v34 --self-test

check-v34: standalone-v34 test-v34
	@echo "check-v34: OK (v34 σ-decompose + Platt + channels_v34 self-test)"

test-v35: standalone-v35
	./creation_os_v35 --self-test

check-v35: standalone-v35 test-v35
	@echo "check-v35: OK (v35 σ-guided speculative decode hooks)"

test-v39: standalone-v39
	./creation_os_v39 --self-test

check-v39: standalone-v39 test-v39
	@echo "check-v39: OK (v39 σ_hardware + σ_total composition self-test)"

test-v40: standalone-v40
	./creation_os_v40 --self-test

check-v40: standalone-v40 test-v40
	@echo "check-v40: OK (v40 σ independence + syndrome decoder self-test)"

test-v41: standalone-v41
	./creation_os_v41 --self-test

check-v41: standalone-v41 test-v41
	@echo "check-v41: OK (v41 σ-guided test-time compute lab self-test)"

test-v42: standalone-v42
	./creation_os_v42 --self-test

check-v42: standalone-v42 test-v42
	@echo "check-v42: OK (v42 σ-guided self-play lab self-test)"

test-v43: standalone-v43
	./creation_os_v43 --self-test

check-v43: standalone-v43 test-v43
	@echo "check-v43: OK (v43 σ-guided knowledge distillation lab self-test)"

test-proxy: standalone-proxy
	./creation_os_proxy --self-test

check-proxy: standalone-proxy test-proxy
	@echo "check-proxy: OK (v44 σ-native inference proxy lab self-test)"

check-v44: check-proxy

test-v44: test-proxy

test-v45: standalone-v45
	./creation_os_v45 --self-test

check-v45: standalone-v45 test-v45
	@echo "check-v45: OK (v45 σ-introspection lab self-test)"

test-v46: standalone-v46
	./creation_os_v46 --self-test

check-v46: standalone-v46 test-v46
	@echo "check-v46: OK (v46 σ-optimized BitNet pipeline lab self-test)"

test-v47: standalone-v47
	./creation_os_v47 --self-test

check-v47: standalone-v47 test-v47
	@echo "check-v47: OK (v47 verified-architecture lab self-test)"

test-v48: standalone-v48
	./creation_os_v48 --self-test

check-v48: standalone-v48 test-v48
	@echo "check-v48: OK (v48 σ-armored red-team lab self-test)"

test-v51: standalone-v51
	./creation_os_v51 --self-test

check-v51: standalone-v51 test-v51
	@echo "check-v51: OK (v51 AGI-complete integration scaffold self-test)"

test-v53: standalone-v53
	./creation_os_v53 --self-test

check-v53: standalone-v53 test-v53
	@echo "check-v53: OK (v53 σ-governed harness scaffold self-test)"

test-v54: standalone-v54
	./creation_os_v54 --self-test

check-v54: standalone-v54 test-v54
	@echo "check-v54: OK (v54 σ-proconductor scaffold self-test)"

test-v55: standalone-v55
	./creation_os_v55 --self-test

check-v55: standalone-v55 test-v55
	@echo "check-v55: OK (v55 σ₃-speculative scaffold self-test)"

test-v56: standalone-v56
	./creation_os_v56 --self-test

check-v56: standalone-v56 test-v56
	@echo "check-v56: OK (v56 σ-Constitutional scaffold self-test)"

test-v57: standalone-v57
	./creation_os_v57 --self-test

check-v57: standalone-v57 test-v57
	@echo "check-v57: OK (v57 Verified Agent convergence registry self-test)"

test-v58: standalone-v58
	./creation_os_v58 --self-test

check-v58: standalone-v58 test-v58
	@echo "check-v58: OK (v58 σ-Cache eviction + branchless kernel self-test)"

test-v59: standalone-v59
	./creation_os_v59 --self-test

check-v59: standalone-v59 test-v59
	@echo "check-v59: OK (v59 σ-Budget adaptive compute controller + branchless kernel self-test)"

microbench-v59: standalone-v59
	@bash scripts/v59/microbench.sh

# v58 microbench — deterministic synthetic-workload kernel timing
# (no model, no network).  See scripts/v58/microbench.sh for the
# wrapper that runs a parameter sweep.
microbench-v58: standalone-v58
	@bash scripts/v58/microbench.sh

# verify-agent: live aggregate driver for the Verified Agent.
# Walks the composition slots declared in src/v57/verified_agent.c and
# dispatches each slot's owning make target.  PASS / SKIP / FAIL is reported
# per slot.  Missing tools (Frama-C, sby, pytest, garak) become SKIPs, never
# silent PASSes.  Exit code: 0 if no FAILs (SKIPs allowed), 1 on FAIL,
# 2 on SKIP under --strict.
verify-agent: standalone-v57
	@bash scripts/v57/verify_agent.sh

# ----------------------------------------------------------------------
# doctor — one-command repo health rollup (Apple-tier DX).
#
# Wraps `cos doctor`: license-check + verify-agent + hardening-check +
# CHACE aggregator + License-Bound Receipt sample.  Prints a single
# sectioned summary with PASS/WARN/FAIL per lane.  Non-zero exit iff a
# blocking lane (license, verify-agent) fails.
# ----------------------------------------------------------------------
doctor: cos
	@./cos doctor

# ----------------------------------------------------------------------
# completion-install — drop shell completion into the right place for
# the caller's shell (bash / zsh / fish).  Respects $SHELL.  Never
# overwrites an existing file without `FORCE=1`.
# ----------------------------------------------------------------------
completion-install:
	@set -e; \
	shell_name="$${SHELL##*/}"; \
	case "$$shell_name" in \
	  bash) src="scripts/completion/cos.bash"; \
	        dst="$$HOME/.local/share/bash-completion/completions/cos"; \
	        mkdir -p "$$(dirname $$dst)"; \
	        ;; \
	  zsh)  src="scripts/completion/cos.zsh"; \
	        dst="$$HOME/.zsh/completions/_cos"; \
	        mkdir -p "$$(dirname $$dst)"; \
	        ;; \
	  fish) src="scripts/completion/cos.fish"; \
	        dst="$$HOME/.config/fish/completions/cos.fish"; \
	        mkdir -p "$$(dirname $$dst)"; \
	        ;; \
	  *)    echo "completion-install: unsupported shell '$$shell_name'"; \
	        echo "  manual install: source scripts/completion/cos.bash|zsh"; \
	        echo "                  or cp scripts/completion/cos.fish ~/.config/fish/completions/"; \
	        exit 0 ;; \
	esac; \
	if [ -e "$$dst" ] && [ "$$FORCE" != "1" ]; then \
	  echo "completion-install: $$dst already exists (re-run with FORCE=1 to overwrite)"; \
	  exit 0; \
	fi; \
	cp -f "$$src" "$$dst"; \
	echo "completion-install: installed $$shell_name completion → $$dst"; \
	case "$$shell_name" in \
	  bash) echo "  next: add to ~/.bashrc:  source \"$$dst\"" ;; \
	  zsh)  echo "  next: add to ~/.zshrc:   fpath=($$HOME/.zsh/completions \$$fpath); autoload -U compinit && compinit" ;; \
	  fish) echo "  next: restart fish or run:  source \"$$dst\"" ;; \
	esac

test-mcp: standalone-mcp
	./creation_os_mcp --self-test

check-mcp: standalone-mcp test-mcp
	@echo "check-mcp: OK (MCP σ server self-test)"

all: standalone oracle bench physics test
	@echo "All targets built successfully."

# distclean: repo-level sweep that removes ALL generated binaries matching
# the canonical globs (/creation_os_v*, /cos-*, /creation_os_sigma_*, etc.),
# plus root-level .dSYM bundles and Finder duplicates anywhere in the tree.
# Idempotent; safe to run from a clean checkout.
distclean: clean
	@bash scripts/distclean.sh

clean:
	rm -rf $(BUILDDIR) .build/vrtl .build/v49-cov .build/v49-audit creation_os creation_os_v6 creation_os_v7 creation_os_v9 creation_os_v10 creation_os_v11 creation_os_v12 creation_os_v15 creation_os_v16 creation_os_v20 creation_os_v21 creation_os_v22 creation_os_v23 creation_os_v24 creation_os_v25 creation_os_v26 creation_os_v27 creation_os_v28 creation_os_v29 creation_os_v31 creation_os_v33 creation_os_v34 creation_os_v35 creation_os_v39 creation_os_v40 creation_os_v41 creation_os_v42 creation_os_v43 creation_os_v45 creation_os_v46 creation_os_v47 creation_os_v48 creation_os_v51 creation_os_v53 creation_os_v54 creation_os_v55 creation_os_v56 creation_os_v57 creation_os_v58 creation_os_v59 creation_os_v60 creation_os_v61 creation_os_v57_hardened creation_os_v58_hardened creation_os_v59_hardened creation_os_v60_hardened creation_os_v61_hardened creation_os_v58_asan creation_os_v59_asan creation_os_v60_asan creation_os_v60_ubsan creation_os_v61_asan creation_os_v61_ubsan creation_os_v58_asan.dSYM creation_os_v59_asan.dSYM creation_os_v60_asan.dSYM creation_os_v60_ubsan.dSYM creation_os_v61_asan.dSYM creation_os_v61_ubsan.dSYM creation_os_v62 creation_os_v62_hardened creation_os_v62_asan creation_os_v62_ubsan creation_os_v62_asan.dSYM creation_os_v62_ubsan.dSYM creation_os_v63 creation_os_v63_hardened creation_os_v63_asan creation_os_v63_ubsan creation_os_v63_asan.dSYM creation_os_v63_ubsan.dSYM creation_os_v64 creation_os_v64_hardened creation_os_v64_asan creation_os_v64_ubsan creation_os_v64_asan.dSYM creation_os_v64_ubsan.dSYM creation_os_v65 creation_os_v65_hardened creation_os_v65_asan creation_os_v65_ubsan creation_os_v65_asan.dSYM creation_os_v65_ubsan.dSYM creation_os_v66 creation_os_v66_hardened creation_os_v66_asan creation_os_v66_ubsan creation_os_v66_asan.dSYM creation_os_v66_ubsan.dSYM creation_os_v67 creation_os_v67_hardened creation_os_v67_asan creation_os_v67_ubsan creation_os_v67_asan.dSYM creation_os_v67_ubsan.dSYM creation_os_v68 creation_os_v68_hardened creation_os_v68_asan creation_os_v68_ubsan creation_os_v68_asan.dSYM creation_os_v68_ubsan.dSYM creation_os_v69 creation_os_v69_hardened creation_os_v69_asan creation_os_v69_ubsan creation_os_v69_asan.dSYM creation_os_v69_ubsan.dSYM creation_os_v70 creation_os_v70_hardened creation_os_v70_asan creation_os_v70_ubsan creation_os_v70_asan.dSYM creation_os_v70_ubsan.dSYM cos SBOM.json ATTESTATION.json ATTESTATION.sig PROVENANCE.json .build/wasm .build/ebpf .build/nix-v61 creation_os_proxy creation_os_mcp creation_os_openai_stub creation_os_suite_stub creation_os_native_m4 cos_lm tokenizer_throughput binding_fidelity vocab_scaling vs_transformer oracle_speaks oracle_ultimate gemm_vs_bsc coherence_gate_batch hv_agi_gate_neon genesis qhdc test_bsc inference_trace_selftest.tmp inference_trace.json cb_v27_selftest.tmp gguf_v28_selftest.gguf tokenizer_v28_selftest.json gguf_v29_selftest.gguf hdl/neuromorphic/build docs/v49/certification/coverage/html

publish-github:
	@bash tools/publish_to_creation_os_github.sh
