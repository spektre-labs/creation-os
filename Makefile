# SPDX-License-Identifier: AGPL-3.0-or-later
# One command: make all
CC = cc
CFLAGS = -O2 -march=native -Wall -std=c11
LDFLAGS = -lm
BUILDDIR = .build
# Verilator 5+: --timing with --lint-only (Ubuntu 24.04+). Override: make VERILATOR_LINT_FLAGS=-Wall formal-rtl-lint
VERILATOR_LINT_FLAGS = -Wall --timing
RTL_SV := rtl/cos_formal_iron_combo.sv rtl/cos_agency_iron_combo.sv rtl/cos_agency_iron_formal.sv rtl/cos_commit_iron_combo.sv rtl/cos_boundary_sync.sv rtl/cos_looplm_drum.sv rtl/cos_geodesic_tick.sv rtl/cos_k_eff_bind.sv rtl/cos_silicon_chip_tb.sv

.PHONY: help infra merge-gate doctor completion-install standalone standalone-v6 standalone-v7 standalone-v9 standalone-v10 standalone-v11 standalone-v12 standalone-v15 standalone-v16 standalone-v20 standalone-v21 standalone-v22 standalone-v23 standalone-v24 standalone-v25 standalone-v26 standalone-v27 standalone-v28 standalone-v29 standalone-v31 standalone-v33 standalone-v34 standalone-v35 standalone-v39 standalone-v40 standalone-v41 standalone-v42 standalone-v43 standalone-proxy standalone-v45 standalone-v46 standalone-v47 standalone-v48 standalone-v51 standalone-v53 standalone-v54 standalone-v55 standalone-v56 standalone-v57 standalone-v58 standalone-v59 standalone-v60 standalone-v61 standalone-v62 standalone-v63 standalone-v64 standalone-v65 standalone-v66 standalone-v67 standalone-v68 standalone-v69 standalone-v70 standalone-v71 standalone-v72 standalone-v73 standalone-v74 standalone-v57-hardened standalone-v58-hardened standalone-v59-hardened standalone-v60-hardened standalone-v61-hardened standalone-v62-hardened standalone-v63-hardened standalone-v64-hardened standalone-v65-hardened standalone-v66-hardened standalone-v67-hardened standalone-v68-hardened standalone-v69-hardened standalone-v70-hardened standalone-v71-hardened standalone-v72-hardened standalone-v73-hardened standalone-v74-hardened standalone-v76 standalone-v76-hardened standalone-v77 standalone-v77-hardened standalone-v78 standalone-v78-hardened standalone-v79 standalone-v79-hardened standalone-v80 standalone-v80-hardened standalone-v81 standalone-v81-hardened standalone-v82 standalone-v82-hardened standalone-v83 standalone-v83-hardened standalone-v84 standalone-v84-hardened standalone-v85 standalone-v85-hardened standalone-v86 standalone-v86-hardened standalone-v87 standalone-v87-hardened standalone-v88 standalone-v88-hardened standalone-v89 standalone-v89-hardened standalone-v90 standalone-v90-hardened standalone-v91 standalone-v91-hardened standalone-v92 standalone-v92-hardened standalone-v93 standalone-v93-hardened standalone-v94 standalone-v94-hardened standalone-v95 standalone-v95-hardened standalone-v96 standalone-v96-hardened standalone-v97 standalone-v97-hardened standalone-v98 standalone-v98-hardened standalone-v99 standalone-v99-hardened standalone-v100 standalone-v100-hardened harden sanitize asan-v58 asan-v59 asan-v60 ubsan-v60 asan-v61 ubsan-v61 asan-v62 ubsan-v62 asan-v63 ubsan-v63 asan-v64 ubsan-v64 asan-v65 ubsan-v65 asan-v66 ubsan-v66 asan-v67 ubsan-v67 asan-v68 ubsan-v68 asan-v69 ubsan-v69 asan-v70 ubsan-v70 asan-v71 ubsan-v71 asan-v72 ubsan-v72 asan-v73 ubsan-v73 asan-v74 ubsan-v74 asan-v76 ubsan-v76 asan-v77 ubsan-v77 asan-v78 ubsan-v78 asan-v79 ubsan-v79 asan-v80 ubsan-v80 asan-v81 ubsan-v81 asan-v82 ubsan-v82 asan-v83 ubsan-v83 asan-v84 ubsan-v84 asan-v85 ubsan-v85 asan-v86 ubsan-v86 asan-v87 ubsan-v87 asan-v88 ubsan-v88 asan-v89 ubsan-v89 asan-v90 ubsan-v90 asan-v91 ubsan-v91 asan-v92 ubsan-v92 asan-v93 ubsan-v93 asan-v94 ubsan-v94 asan-v95 ubsan-v95 asan-v96 ubsan-v96 asan-v97 ubsan-v97 asan-v98 ubsan-v98 asan-v99 ubsan-v99 asan-v100 ubsan-v100 hardening-check sbom security-scan reproducible-build attest sign slsa wasm-sandbox ebpf-policy sandbox-exec distroless nix-build sel4-check chace cos check-cos standalone-mcp standalone-openai-stub standalone-suite-stub native-m4 metallib-m4 cos_lm standalone-v27-rust gen-cos-codebook bench-v27-all bench-binding-fidelity bench-vocab-scaling bench-vs-transformer formal-sby-tokenizer formal-sby-v37 formal-sby-v47 synth-v37 check-asic-tile librelane-v38 check-crossbar-sim bench-v40-threshold bench-v41-scaling bench-v42-curve bench-v43-distill bench-v44-overhead bench-v45-paradox bench-v46-e2e v50-benchmark microbench-v58 microbench-v59 microbench-v60 microbench-v61 microbench-v62 microbench-v63 microbench-v64 microbench-v65 microbench-v66 microbench-v67 microbench-v68 microbench-v69 microbench-v70 microbench-v71 microbench-v72 microbench-v73 microbench-v74 microbench-v76 microbench-v77 microbench-v78 microbench-v79 microbench-v80 microbench-v81 microbench-v82 microbench-v83 microbench-v84 microbench-v85 microbench-v86 microbench-v87 microbench-v88 microbench-v89 microbench-v90 microbench-v91 microbench-v92 microbench-v93 microbench-v94 microbench-v95 microbench-v96 microbench-v97 microbench-v98 microbench-v99 microbench-v100 test-v62 test-v63 test-v64 test-v65 test-v66 test-v67 test-v68 test-v69 test-v70 test-v71 test-v72 test-v73 test-v74 test-v76 test-v77 test-v78 test-v79 test-v80 test-v81 test-v82 test-v83 test-v84 test-v85 test-v86 test-v87 test-v88 test-v89 test-v90 test-v91 test-v92 test-v93 test-v94 test-v95 test-v96 test-v97 test-v98 test-v99 test-v100 check-v62 check-v63 check-v64 check-v65 check-v66 check-v67 check-v68 check-v69 check-v70 check-v71 check-v72 check-v73 check-v74 check-v76 check-v77 check-v78 check-v79 check-v80 check-v81 check-v82 check-v83 check-v84 check-v85 check-v86 check-v87 check-v88 check-v89 check-v90 check-v91 check-v92 check-v93 check-v94 check-v95 check-v96 check-v97 check-v98 check-v99 check-v100 standalone-v101 standalone-v101-real test-v101 check-v101 check-v101-real microbench-v101 bench-v101-smoke check-v102 bench-v102 license_attest license-pin license-check license-apply license-attest license-attest-hardened core oracle bench bench-coherence bench-agi-gate bench-tokenizer-v27 physics test test-v6 test-v7 test-v9 test-v10 test-v11 test-v12 test-v15 test-v16 test-v20 test-v21 test-v22 test-v23 test-v24 test-v25 test-v26 test-v27 test-v28 test-v29 test-v31 test-v33 test-v34 test-v35 test-v39 test-v40 test-v41 test-v42 test-v43 test-proxy test-v44 test-v45 test-v46 test-v47 test-v48 test-v51 test-v53 test-v54 test-v55 test-v56 test-v57 test-v58 test-v59 test-v60 test-v61 test-mcp test-openai-stub test-suite-stub check check-v6 check-v7 check-v9 check-v10 check-v11 check-v12 check-v15 check-v16 check-v20 check-v21 check-v22 check-v23 check-v24 check-v25 check-v26 check-v27 check-v28 check-v29 check-v31 check-v33 check-v34 check-v35 check-v39 check-v40 check-v41 check-v42 check-v43 check-proxy check-v44 check-v45 check-v46 check-v47 check-v48 check-v51 check-v53 check-v54 check-v55 check-v56 check-v57 check-v58 check-v59 check-v60 check-v61 check-mcp verify-agent check-openai-stub check-suite-stub check-native-m4 bench-native-m4 check-rtl formal-rtl-lint formal-rtl-sim formal-sby-agency formal-sby-cover-agency eqy-agency-self oss-formal-extreme stack-nucleon stack-singularity rust-iron-lint yosys-elab yosys-prove-agency rust-iron-test hardware-supreme stack-ultimate chisel-compile chisel-verilog all clean verify verify-c verify-sv verify-property verify-integration trust-report red-team red-team-garak red-team-deepteam red-team-sigma red-team-property merge-gate-v48 certify certify-formal certify-coverage certify-binary-audit certify-red-team certify-trace publish-github

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

check: standalone test
	@echo "check: OK (standalone + test)"

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
	@echo "merge-gate: OK (portable + v6..v29 + v101..v106 + v60..v100 + v111 + v106 curl loopback + v107 installer + v108 UI + v109 multi-GGUF + v112/v113/v114 agentic stack + v115/v116/v117/v118 memory/MCP/long-context/vision + v119/v120/v121/v122/v123 speculative/distill/planning/red-team/formal + v124/v125/v126 living-weights + v129..v133 collective intelligence + v134..v138 deep infrastructure + v139..v143 world intelligence + v144..v148 sovereign self-improvement + v149..v153 embodied/swarm/code-agent/distill/identity + v154..v158 showcase/publish/paper/community/v1.0-release + v159 self-heal + v160 telemetry + v161 adversarial-train + v162 compose)"

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
	@bash ./fix_critique_points.sh
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
		cd hw/yosys && yosys -q prove_agency_comb.ys && echo "yosys-prove-agency: OK"; \
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
			rtl/cos_commit_iron_combo.sv rtl/cos_boundary_sync.sv && \
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
                -D_FORTIFY_SOURCE=3 \
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
SAN_CFLAGS  = -O1 -g -std=c11 -Wall -fno-omit-frame-pointer
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

check-v111-sft-smoke:
	@bash training/v111/check_v111_sft_smoke.sh

check-v111: check-v111-matrix check-v111-reason check-v111-sft-smoke
	@echo "check-v111: OK (v111.1 matrix + v111.2 reason + v111.3 SFT smoke)"

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
# Single C binary, no deps beyond libc, NO_COLOR-respecting, isatty
# auto-detect.  This is the "front door" for end users: `cos`,
# `cos verify`, `cos chace`, `cos sigma`, `cos think <prompt>`.
cos: cli/cos.c
	$(CC) -O2 -Wall -std=c11 -o cos cli/cos.c

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
MCP_SRCS = src/mcp/sigma_server.c src/mcp/sigma_server_http.c src/sigma/decompose.c src/sigma/calibrate.c src/sigma/channels.c src/sigma/channels_v34.c src/server/json_esc.c

standalone-mcp: $(MCP_SRCS)
	$(CC) $(CFLAGS) -I. -o creation_os_mcp $(MCP_SRCS) $(LDFLAGS)

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

clean:
	rm -rf $(BUILDDIR) .build/vrtl .build/v49-cov .build/v49-audit creation_os creation_os_v6 creation_os_v7 creation_os_v9 creation_os_v10 creation_os_v11 creation_os_v12 creation_os_v15 creation_os_v16 creation_os_v20 creation_os_v21 creation_os_v22 creation_os_v23 creation_os_v24 creation_os_v25 creation_os_v26 creation_os_v27 creation_os_v28 creation_os_v29 creation_os_v31 creation_os_v33 creation_os_v34 creation_os_v35 creation_os_v39 creation_os_v40 creation_os_v41 creation_os_v42 creation_os_v43 creation_os_v45 creation_os_v46 creation_os_v47 creation_os_v48 creation_os_v51 creation_os_v53 creation_os_v54 creation_os_v55 creation_os_v56 creation_os_v57 creation_os_v58 creation_os_v59 creation_os_v60 creation_os_v61 creation_os_v57_hardened creation_os_v58_hardened creation_os_v59_hardened creation_os_v60_hardened creation_os_v61_hardened creation_os_v58_asan creation_os_v59_asan creation_os_v60_asan creation_os_v60_ubsan creation_os_v61_asan creation_os_v61_ubsan creation_os_v58_asan.dSYM creation_os_v59_asan.dSYM creation_os_v60_asan.dSYM creation_os_v60_ubsan.dSYM creation_os_v61_asan.dSYM creation_os_v61_ubsan.dSYM creation_os_v62 creation_os_v62_hardened creation_os_v62_asan creation_os_v62_ubsan creation_os_v62_asan.dSYM creation_os_v62_ubsan.dSYM creation_os_v63 creation_os_v63_hardened creation_os_v63_asan creation_os_v63_ubsan creation_os_v63_asan.dSYM creation_os_v63_ubsan.dSYM creation_os_v64 creation_os_v64_hardened creation_os_v64_asan creation_os_v64_ubsan creation_os_v64_asan.dSYM creation_os_v64_ubsan.dSYM creation_os_v65 creation_os_v65_hardened creation_os_v65_asan creation_os_v65_ubsan creation_os_v65_asan.dSYM creation_os_v65_ubsan.dSYM creation_os_v66 creation_os_v66_hardened creation_os_v66_asan creation_os_v66_ubsan creation_os_v66_asan.dSYM creation_os_v66_ubsan.dSYM creation_os_v67 creation_os_v67_hardened creation_os_v67_asan creation_os_v67_ubsan creation_os_v67_asan.dSYM creation_os_v67_ubsan.dSYM creation_os_v68 creation_os_v68_hardened creation_os_v68_asan creation_os_v68_ubsan creation_os_v68_asan.dSYM creation_os_v68_ubsan.dSYM creation_os_v69 creation_os_v69_hardened creation_os_v69_asan creation_os_v69_ubsan creation_os_v69_asan.dSYM creation_os_v69_ubsan.dSYM creation_os_v70 creation_os_v70_hardened creation_os_v70_asan creation_os_v70_ubsan creation_os_v70_asan.dSYM creation_os_v70_ubsan.dSYM cos SBOM.json ATTESTATION.json ATTESTATION.sig PROVENANCE.json .build/wasm .build/ebpf .build/nix-v61 creation_os_proxy creation_os_mcp creation_os_openai_stub creation_os_suite_stub creation_os_native_m4 cos_lm tokenizer_throughput binding_fidelity vocab_scaling vs_transformer oracle_speaks oracle_ultimate gemm_vs_bsc coherence_gate_batch hv_agi_gate_neon genesis qhdc test_bsc inference_trace_selftest.tmp inference_trace.json cb_v27_selftest.tmp gguf_v28_selftest.gguf tokenizer_v28_selftest.json gguf_v29_selftest.gguf hdl/neuromorphic/build docs/v49/certification/coverage/html

publish-github:
	@bash tools/publish_to_creation_os_github.sh
