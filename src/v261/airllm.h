/*
 * v261 σ-AirLLM — layer-by-layer inference with σ-gate.
 *
 *   AirLLM runs 70B models on a 4 GB GPU by streaming
 *   one layer at a time: load layer N → compute →
 *   release.  v261 adds a σ per layer — so "which layer
 *   contributes most noise" is a typed, diagnosable
 *   property — and a σ-driven selective-precision rule
 *   (low σ → 4-bit is enough; high σ → 8-bit; very high
 *   σ → full precision).
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Layers (exactly 8 representative layers of a 70B
 *   model, canonical order L0..L7):
 *     Each row: `index`, `sigma_layer ∈ [0, 1]`,
 *     `precision_bits ∈ {4, 8, 16}`, `is_problem`.
 *     The problem layer is the unique argmax σ_layer
 *     (ties resolved by lowest index).  Exactly one
 *     `is_problem == true`.
 *
 *   Selective precision rule (σ-driven, strict):
 *     σ_layer ≤ 0.20                       → 4-bit
 *     0.20 <  σ_layer ≤ 0.40               → 8-bit
 *     σ_layer  >  0.40                       → 16-bit (full)
 *   Every layer's `precision_bits` MUST match the rule
 *   byte-for-byte.
 *
 *   Hardware backends (exactly 4, canonical order):
 *     cuda_4gb_gpu · cpu_only · macos_mps · linux_cuda
 *   Each row: `supported == true`, `min_ram_gb ≥ 4`.
 *   AirLLM is hardware-agnostic by design.
 *
 *   Speed / abstention tradeoff (exactly 3 regimes):
 *     slow    — AirLLM alone, 0.7 tok/s, no abstain
 *     aircos  — AirLLM + σ-gate, 0.9 tok/s effective
 *               (abstain on high σ), abstain_pct ≥ 5
 *     human   — AirLLM + manual verification, 0.3 tok/s
 *               effective (human review loop)
 *   v0 contract: `aircos` has the highest effective
 *   tokens/s AND abstain_pct > 0 (the gate has teeth).
 *
 *   σ_airllm (surface hygiene):
 *       σ_airllm = 1 −
 *         (layers_ok + precision_rule_ok +
 *          backends_ok + tradeoff_ok + problem_ok) /
 *         (8 + 8 + 4 + 3 + 1)
 *   v0 requires `σ_airllm == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 8 layers, σ_layer ∈ [0, 1], index
 *        strictly ascending from 0.
 *     2. Precision rule holds per layer (byte-match).
 *     3. Exactly one `is_problem == true`, and it is
 *        the layer with the maximum σ_layer.
 *     4. Exactly 4 hardware backends, every supported,
 *        every `min_ram_gb ≥ 4`.
 *     5. Exactly 3 tradeoff regimes; `aircos` has the
 *        strictly highest effective tokens/s AND
 *        `abstain_pct > 0`.
 *     6. σ_airllm ∈ [0, 1] AND σ_airllm == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v261.1 (named, not implemented): live AirLLM layer
 *     streamer wired through v243 pipeline, on-disk
 *     layer cache, runtime precision-switching by live
 *     σ feed, multi-backend auto-detection on boot.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V261_AIRLLM_H
#define COS_V261_AIRLLM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V261_N_LAYERS    8
#define COS_V261_N_BACKENDS  4
#define COS_V261_N_TRADEOFF  3

typedef struct {
    int   index;
    float sigma_layer;
    int   precision_bits;
    bool  is_problem;
} cos_v261_layer_t;

typedef struct {
    char  name[20];
    bool  supported;
    int   min_ram_gb;
} cos_v261_backend_t;

typedef struct {
    char  regime[12];
    float tokens_per_s;
    int   abstain_pct;
} cos_v261_tradeoff_t;

typedef struct {
    cos_v261_layer_t     layers   [COS_V261_N_LAYERS];
    cos_v261_backend_t   backends [COS_V261_N_BACKENDS];
    cos_v261_tradeoff_t  tradeoff [COS_V261_N_TRADEOFF];

    int   n_layers_ok;
    int   n_precision_ok;
    int   problem_index;
    int   n_backends_ok;
    bool  tradeoff_ok;

    float sigma_airllm;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v261_state_t;

void   cos_v261_init(cos_v261_state_t *s, uint64_t seed);
void   cos_v261_run (cos_v261_state_t *s);

size_t cos_v261_to_json(const cos_v261_state_t *s,
                         char *buf, size_t cap);

int    cos_v261_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V261_AIRLLM_H */
