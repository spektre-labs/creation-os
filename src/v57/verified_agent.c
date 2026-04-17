/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v57 Verified Agent — composition data.
 *
 * The slot table below is the authoritative listing of which
 * Creation OS subsystem owns which agent-runtime slot, what
 * deterministic make target verifies it, and the tier that target
 * achieves today.
 *
 * Tier rules (mirrored from invariants.c):
 *   M = the make target runs a deterministic self-test that returns
 *       0 iff the slot's claim holds in this build
 *   F = a formal proof artifact discharges the slot's claim
 *   I = the slot is documented and reasoned about, but not yet
 *       checked mechanically
 *   P = the slot is planned; the make target is a placeholder
 */
#include "verified_agent.h"

#include <string.h>

static const v57_slot_t k_slots[] = {
    {
        .slot           = "execution_sandbox",
        .owner_versions = "v48",
        .make_target    = "make check-v48",
        .best_tier      = V57_TIER_M,
        .summary        = "Capability sandbox + 7-layer defense; "
                          "deny-by-default tool admission",
    },
    {
        .slot           = "sigma_kernel_surface",
        .owner_versions = "v47",
        .make_target    = "make verify-c",
        .best_tier      = V57_TIER_F,
        .summary        = "Frama-C / WP ACSL contracts on σ-kernel "
                          "(SKIPs honestly without Frama-C installed)",
    },
    {
        .slot           = "harness_loop",
        .owner_versions = "v53",
        .make_target    = "make check-v53",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-TAOR loop with σ-abstention and "
                          "creation.md invariants",
    },
    {
        .slot           = "multi_llm_routing",
        .owner_versions = "v54",
        .make_target    = "make check-v54",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-proconductor: σ-profiled subagents, "
                          "σ-triangulation, disagreement abstention",
    },
    {
        .slot           = "speculative_decode",
        .owner_versions = "v55",
        .make_target    = "make check-v55",
        .best_tier      = V57_TIER_M,
        .summary        = "σ₃-speculative: per-channel σ "
                          "decomposition + EARS commit policy",
    },
    {
        .slot           = "constitutional_self_modification",
        .owner_versions = "v56",
        .make_target    = "make check-v56",
        .best_tier      = V57_TIER_M,
        .summary        = "VPRM verifier + σ-gated IP-TTT + "
                          "grokking commutator-σ + ANE layout",
    },
    {
        .slot           = "do178c_assurance_pack",
        .owner_versions = "v49",
        .make_target    = "make certify",
        .best_tier      = V57_TIER_I,
        .summary        = "DO-178C-aligned HLR/LLR/PSAC/SCMP/SDD/SDP/"
                          "SQAP/SVP artifacts (NOT FAA/EASA "
                          "certification)",
    },
    {
        .slot           = "red_team_suite",
        .owner_versions = "v48 + v49",
        .make_target    = "make red-team",
        .best_tier      = V57_TIER_I,
        .summary        = "Garak + DeepTeam + σ-property red-team "
                          "(aggregator; SKIPs without tooling)",
    },
    {
        .slot           = "kv_cache_eviction",
        .owner_versions = "v58",
        .make_target    = "make check-v58",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Cache: σ-decomposed KV-cache eviction + "
                          "per-token precision tier + branchless NEON "
                          "kernel (deterministic self-test + microbench)",
    },
    {
        .slot           = "adaptive_compute_budget",
        .owner_versions = "v59",
        .make_target    = "make check-v59",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Budget: σ-decomposed adaptive test-time "
                          "compute budget controller + four-valued "
                          "CONTINUE/EARLY_EXIT/EXPAND/ABSTAIN kernel "
                          "(branchless + NEON 4-accum SoA; ≥60 tests)",
    },
    {
        .slot           = "runtime_security_kernel",
        .owner_versions = "v60",
        .make_target    = "make check-v60",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Shield: first cap-kernel with σ=(ε,α) "
                          "intent gate + TOCTOU-free arg-hash + "
                          "code-page integrity + sticky-deny "
                          "(constant-time; branchless; 81 tests; "
                          "refuses α-dominated intent regardless of caps)",
    },
    {
        .slot           = "defense_in_depth_stack",
        .owner_versions = "v61",
        .make_target    = "make check-v61",
        .best_tier      = V57_TIER_M,
        .summary        = "Σ-Citadel: BLP + Biba + MLS-compartment "
                          "lattice (branchless) + deterministic "
                          "256-bit attestation quote (BLAKE2b via "
                          "libsodium opt-in) + composition with v60; "
                          "ships seL4 CAmkES + Wasmtime + eBPF + "
                          "sandbox-exec + pledge + Nix + distroless "
                          "+ Sigstore + SLSA-v1.0 stubs under "
                          "`make chace` (PASS on present layers, "
                          "SKIP honestly on missing ones)",
    },
    {
        .slot           = "reasoning_fabric",
        .owner_versions = "v62",
        .make_target    = "make check-v62",
        .best_tier      = V57_TIER_M,
        .summary        = "Reasoning Fabric: 2026 frontier in one C "
                          "kernel — Coconut latent CoT (arXiv:2412.06769) "
                          "+ Energy-Based Transformer verifier "
                          "(arXiv:2507.02092, ICLR 2026) + Hierarchical "
                          "Reasoning Model H/L loop (arXiv:2506.21734) + "
                          "Native Sparse Attention 3-branch attend "
                          "(arXiv:2502.11089) + DeepSeek-V3 Multi-Token "
                          "Predictor draft (arXiv:2412.19437) + ARKV "
                          "adaptive-precision KV manager (arXiv:2603.08727); "
                          "branchless on M4 NEON, 64-byte aligned, mmap-"
                          "friendly; composes with v60 σ-Shield + v61 "
                          "Σ-Citadel as a 3-bit decision (≥62 tests)",
    },
    {
        .slot           = "e2e_encrypted_fabric",
        .owner_versions = "v63",
        .make_target    = "make check-v63",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Cipher: end-to-end encrypted fabric — "
                          "BLAKE2b-256 (RFC 7693) + HKDF (RFC 5869) + "
                          "ChaCha20-Poly1305 AEAD (RFC 8439) + X25519 "
                          "(RFC 7748) + constant-time equality + "
                          "attestation-bound sealed envelope (key = "
                          "HKDF(v61 quote || context)) + forward-secret "
                          "symmetric ratchet + IK-like session "
                          "handshake with BLAKE2b chaining key; "
                          "dependency-free C, all vectors official RFC; "
                          "composes with v60+v61+v62 as a 4-bit "
                          "branchless decision "
                          "(cos_v63_compose_decision); libsodium / "
                          "liboqs ML-KEM-768 hybrid opt-ins "
                          "(COS_V63_LIBSODIUM, COS_V63_LIBOQS); ≥144 "
                          "tests under ASAN+UBSAN",
    },
    {
        .slot           = "agentic_intellect",
        .owner_versions = "v64",
        .make_target    = "make check-v64",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Intellect: agentic AGI kernel — "
                          "MCTS-σ PUCT search (Empirical-MCTS, "
                          "arXiv:2602.04248; rStar-Math) + skill "
                          "library with σ-weighted Hamming retrieval "
                          "(EvoSkill, arXiv:2603.02766; Voyager) + "
                          "TOCTOU-free schema-typed tool authz "
                          "(Dynamic ReAct, arXiv:2509.20386) + "
                          "Reflexion ratchet (ERL, arXiv:2603.24639; "
                          "ReflexiCoder, arXiv:2603.05863) + "
                          "AlphaEvolve-σ ternary mutation "
                          "(BitNet-b1.58 layout, arXiv:2402.17764) + "
                          "per-token MoD-σ depth routing "
                          "(arXiv:2404.02258; MoDA 2026 "
                          "arXiv:2603.15619; A-MoD "
                          "arXiv:2412.20875); branchless Q0.15 "
                          "integer math, no FP on hot path, "
                          "aligned_alloc(64) arenas, constant-time "
                          "signatures; composes with v60+v61+v62+v63 "
                          "as a 5-bit branchless decision "
                          "(cos_v64_compose_decision); "
                          "~500M tool-authz decisions/s, ~1.4M skill "
                          "retrieves/s, ~670k MCTS iters/s, ~5.1 GB/s "
                          "MoD routing on M4; ≥260 deterministic "
                          "tests under ASAN+UBSAN",
    },
    {
        .slot           = "hyperdimensional_cortex",
        .owner_versions = "v65",
        .make_target    = "make check-v65",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Hypercortex: hyperdimensional "
                          "neurosymbolic kernel — bipolar hypervectors "
                          "at D=16384 bits (2048 B, 32 M4 cache lines) "
                          "with VSA bind (XOR, self-inverse), "
                          "threshold-majority bundle, cyclic permute, "
                          "Q0.15 similarity = (D − 2·Hamming)·(32768/D); "
                          "constant-time cleanup memory (Holographic "
                          "Invariant Storage, arXiv:2603.13558); "
                          "role-filler records + analogy + "
                          "position-permuted sequence memory "
                          "(OpenMem 2026; VaCoAl, arXiv:2604.11665; "
                          "VSA-ARC, arXiv:2511.08747; "
                          "Attention-as-Binding AAAI 2026; "
                          "Hyperdimensional Probe arXiv:2509.25045); "
                          "HVL — 9-opcode integer bytecode ISA for "
                          "VSA programs with per-program cost budget "
                          "and integrated 6-bit gate (LOAD/BIND/BUNDLE/"
                          "PERM/LOOKUP/SIM/CMPGE/GATE/HALT); popcount-"
                          "native (NEON `cnt` + horizontal add), "
                          "aligned_alloc(64) arenas, zero FP on hot "
                          "path; composes with v60+v61+v62+v63+v64 as "
                          "a 6-bit branchless AND "
                          "(cos_v65_compose_decision); "
                          "~10M hamming/s @ 41 GB/s, ~31M bind/s @ "
                          "192 GB/s, ~10M proto·cmps/s cleanup, "
                          "~5.7M HVL progs/s @ 40M ops/s on M4; "
                          "≥500 deterministic tests under ASAN+UBSAN",
    },
    {
        .slot           = "matrix_substrate",
        .owner_versions = "v66",
        .make_target    = "make check-v66",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Silicon: matrix substrate that turns "
                          "v60-v65 thought into actual MAC ops on "
                          "actual matrix hardware — runtime CPU "
                          "feature detection (NEON / dotprod / i8mm / "
                          "bf16 / SVE / SME / SME2 via sysctl on "
                          "Darwin, getauxval on Linux, cached after "
                          "first call); INT8 GEMV with 4-accumulator "
                          "NEON inner loop + 64-byte prefetch + "
                          "vmull/vpadalq long-add accumulation, "
                          "saturated Q0.15 output, bit-identical "
                          "scalar fallback (MpGEMM, "
                          "arXiv:2512.21473; Hello SME, "
                          "arXiv:2409.18779); ternary GEMV (BitNet "
                          "b1.58, packed 2-bits-per-weight) with "
                          "branchless table-lookup unpacker, constant "
                          "time per row (arXiv:2410.16144); "
                          "NativeTernary self-delimiting wire decoder "
                          "at exactly 2.0 bpw (arXiv:2604.11665 → "
                          "arXiv:2604.03336) with defensive 11→0 "
                          "code clamp; CFC conformal-abstention gate "
                          "(arXiv:2603.27403) — per-group Q0.15 "
                          "threshold table, streaming quantile "
                          "update with ratio-preserving overflow "
                          "shift (mirrors v64 Reflexion ratchet), "
                          "branchless integer comparison; HSL — "
                          "8-opcode integer bytecode ISA for matrix "
                          "programs (HALT/LOAD/GEMV_I8/GEMV_T/"
                          "DECODE_NTW/ABSTAIN/CMPGE/GATE) with per-"
                          "instruction MAC-cost accounting and an "
                          "integrated 7-bit GATE that writes v66_ok; "
                          "default build NEVER emits an SME "
                          "instruction (avoids SIGILL on M1/M2/M3); "
                          "COS_V66_SME=1 reserves the SME path once "
                          "a streaming-mode shim lands; composes "
                          "with v60+v61+v62+v63+v64+v65 as a 7-bit "
                          "branchless AND "
                          "(cos_v66_compose_decision); ~50 Gops/s "
                          "INT8 GEMV (256x1024), ~3.4 Gops/s ternary "
                          "GEMV (512x1024), ~3 GB/s NTW decode, "
                          "~40M HSL progs/s on M3 NEON path; "
                          "1705 deterministic tests under ASAN+UBSAN",
    },
    {
        .slot           = "deliberative_cognition",
        .owner_versions = "v67",
        .make_target    = "make check-v67",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Noesis: deliberative reasoning engine + "
                          "AGI knowledge-retrieval kernel that turns "
                          "v60..v66 control + matrix plane into "
                          "structured cognition with receipts — BM25 "
                          "sparse retrieval (integer Q0.15 IDF "
                          "surrogate, CSR postings + tf parallel "
                          "array, top-K via branchless insertion); "
                          "256-bit dense-signature retrieval (Hamming, "
                          "popcount-native, mirrors v65 HDC algebra); "
                          "bounded graph walker (CSR + visited bitset "
                          "with __builtin_popcountll, saturating Q0.15 "
                          "weight accumulation); hybrid rescore (Q0.15 "
                          "weights normalised to 32768); fixed-width "
                          "deliberation beam with Q0.15 verifier "
                          "(AlphaProof / o1-o3 deliberative reasoning, "
                          "Graph-of-Thoughts arXiv:2308.09687, "
                          "Tree-of-Thoughts arXiv:2305.10601); "
                          "dual-process gate (System-1 vs System-2 "
                          "via single branchless margin compare; "
                          "Soar/ACT-R/LIDA synthesis 2026); "
                          "metacognitive confidence (top1 − mean_rest "
                          "in Q0.15, monotonic in absolute gap); "
                          "tactic library (branchless priority cascade "
                          "over witness score); NBL — 9-opcode integer "
                          "bytecode ISA (HALT/RECALL/EXPAND/RANK/"
                          "DELIBERATE/VERIFY/CONFIDE/CMPGE/GATE) with "
                          "per-instruction reasoning-unit cost "
                          "accounting and a GATE opcode that writes "
                          "v67_ok iff cost ≤ budget AND score ≥ "
                          "threshold AND evidence_count ≥ 1 AND NOT "
                          "abstained (AlphaFold 3-style evidence "
                          "receipts per step); composes with "
                          "v60+v61+v62+v63+v64+v65+v66 as an 8-bit "
                          "branchless AND (cos_v67_compose_decision); "
                          "~54M dense Hamming cmps/s, ~800k beam "
                          "steps/s, ~64M NBL progs/s (320M ops/s), "
                          "~9k BM25 queries/s on D=1024 T=16; 2593 "
                          "deterministic tests under ASAN+UBSAN",
    },
    {
        .slot           = "continual_learning_memory",
        .owner_versions = "v68",
        .make_target    = "make check-v68",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Mnemos: continual-learning + episodic-"
                          "memory + online-adaptation kernel that "
                          "turns one-shot deliberation (v60..v67) "
                          "into a system that remembers, evolves, "
                          "and learns across calls — bipolar HV "
                          "episodic store at D=8192 bits "
                          "(hippocampal pattern separation + "
                          "completion via __builtin_popcountll "
                          "Hamming + XOR bind); Titans-style 2025 "
                          "surprise gate (write iff |pred − obs| ≥ "
                          "thresh, single branchless compare); "
                          "ACT-R activation decay (saturating Q0.15 "
                          "linear, no log/no float); content-"
                          "addressable recall with rehearsal "
                          "(top-K nearest by Hamming → Q0.15 sim, "
                          "branchless top-K); Hebbian online "
                          "adapter (Q0.15 outer-product, "
                          "saturating per-cell, TTT-style "
                          "arXiv:2407.04620) under an EWC-style "
                          "anti-catastrophic-forgetting rate "
                          "ratchet (never grows between sleeps); "
                          "sleep replay / consolidation (offline "
                          "majority-XOR bundle of high-activation "
                          "episodes into a long-term HV; "
                          "Diekelmann & Born 2010 + 2024 systems "
                          "extensions); branchless forgetting "
                          "controller (drop activation < thresh "
                          "under a forget budget); MML — Mnemonic "
                          "Memory Language — 10-opcode integer "
                          "bytecode ISA (HALT/SENSE/SURPRISE/STORE/"
                          "RECALL/HEBB/CONSOLIDATE/FORGET/CMPGE/"
                          "GATE) with per-instruction memory-unit "
                          "cost accounting and a GATE opcode that "
                          "writes v68_ok iff cost ≤ budget AND "
                          "recall ≥ threshold AND forget_count ≤ "
                          "forget_budget AND NOT abstained; "
                          "composes with v60..v67 as a 9-bit "
                          "branchless AND "
                          "(cos_v68_compose_decision); ~38 M HV "
                          "Hamming cmps/s, ~110 k recall/s on "
                          "N=256 D=8192, ~24 M Hebb upd/s on 16x16 "
                          "adapter, ~3.8 k full sleep "
                          "consolidations/s, ~38 M MML progs/s "
                          "(~192 M ops/s); 2669 deterministic "
                          "tests under ASAN+UBSAN",
    },
    {
        .slot           = "distributed_orchestration",
        .owner_versions = "v69",
        .make_target    = "make check-v69",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Constellation: distributed-orchestration "
                          "+ parallel-decoding + multi-agent "
                          "consensus kernel that turns a single-node "
                          "continual-learner (v60..v68) into a fleet "
                          "— EAGLE-3-style tree speculative decoding "
                          "(branchless XOR-match per level + popcount "
                          "acceptance; longest-prefix selection via "
                          "branchless sel); Council Mode + FREE-MAD "
                          "multi-agent debate with anti-conformity "
                          "bonus and abstain-on-low-margin default "
                          "(arXiv:2604.02923v1, 2026); PBFT-style "
                          "2f+1 Byzantine quorum (branchless "
                          "popcount-style count, single compare "
                          "against quorum); MaxScore MoE top-K "
                          "routing (arXiv:2508.12801, branchless "
                          "bubble, integer load-balance counter); "
                          "depth-limited draft tree expansion / "
                          "prune with Q0.15 cumulative path "
                          "acceptance; Lamport / Fidge vector clocks "
                          "for causal ordering (branchless max "
                          "merge); flash-style chunked attention "
                          "dot product with softmax-free integer "
                          "max-tracker; AlphaZero-lineage self-play "
                          "Elo + UCB arm selection (saturating Q0.15 "
                          "update, branchless argmax); KV-cache "
                          "deduplication via 512-bit bipolar "
                          "popcount sketch (Hamming-neighbour "
                          "collapse); CL — Constellation Language — "
                          "10-opcode integer bytecode ISA "
                          "(HALT/DRAFT/VERIFY/DEBATE/VOTE/ROUTE/"
                          "GOSSIP/ELO/DEDUP/GATE) with per-"
                          "instruction orchestration-unit cost "
                          "accounting and a GATE opcode that writes "
                          "v69_ok iff cost ≤ budget AND vote margin "
                          "≥ threshold AND Byzantine faults ≤ "
                          "byzantine budget AND NOT abstained; "
                          "composes with v60..v68 as a 10-bit "
                          "branchless AND "
                          "(cos_v69_compose_decision); ~58 M tree "
                          "verifies/s depth=8, ~129 M Byzantine "
                          "votes/s N=64, ~9.3 M debate aggs/s "
                          "N=32, ~2.1 M MoE routes/s N=64 K=8, "
                          "~6.9 M dedup inserts/s on 64-slot "
                          "table, ~7.4 M chunked-dots/s N=1024, "
                          "~69 M Elo+UCB updates/s 16 arms, "
                          "~9.9 M CL 9-inst progs/s (~89 M "
                          "ops/s), ~361 M 10-bit composed "
                          "decisions/s; 3226 deterministic tests "
                          "under ASAN+UBSAN",
    },
};

static const int k_slot_count =
    (int)(sizeof(k_slots) / sizeof(k_slots[0]));

int v57_slot_count(void)
{
    return k_slot_count;
}

const v57_slot_t *v57_slot_get(int index)
{
    if (index < 0 || index >= k_slot_count) return NULL;
    return &k_slots[index];
}

const v57_slot_t *v57_slot_find(const char *slot)
{
    if (!slot) return NULL;
    for (int i = 0; i < k_slot_count; ++i) {
        const char *cand = k_slots[i].slot;
        if (cand && strcmp(cand, slot) == 0) return &k_slots[i];
    }
    return NULL;
}

void v57_report_build(v57_report_t *out)
{
    if (!out) return;

    for (int i = 0; i <= V57_TIER_N; ++i) {
        out->slot_counts[i]      = 0;
        out->invariant_counts[i] = 0;
    }

    for (int i = 0; i < k_slot_count; ++i) {
        v57_tier_t t = k_slots[i].best_tier;
        if (t >= 0 && t <= V57_TIER_N) out->slot_counts[t]++;
    }
    out->n_slots = k_slot_count;

    int inv_n = v57_invariant_count();
    for (int i = 0; i < inv_n; ++i) {
        v57_tier_t t = v57_invariant_best_tier(v57_invariant_get(i));
        if (t >= 0 && t <= V57_TIER_N) out->invariant_counts[t]++;
    }
    out->n_invariants = inv_n;
}
