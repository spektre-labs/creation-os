/*
 * v267 σ-Mamba — state-space backend with σ-gated
 *                transformer fallback.
 *
 *   Mamba / RWKV are linear-time attention (O(n)).
 *   Transformer attention is quadratic (O(n²)).
 *   Mamba runs faster but is weaker at in-context
 *   learning.  v267 measures σ per query and falls
 *   back to a transformer only when σ exceeds a
 *   threshold — paired speed and accuracy.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Backends (exactly 3, canonical order):
 *     mamba       : complexity="O(n)",  exponent=1,
 *                   throughput_rel=5.0
 *     rwkv        : complexity="O(n)",  exponent=1,
 *                   throughput_rel=4.6
 *     transformer : complexity="O(n^2)", exponent=2,
 *                   throughput_rel=1.0  (canonical baseline)
 *   Contract:
 *     * mamba.exponent == 1
 *     * rwkv.exponent  == 1
 *     * transformer.exponent == 2
 *     * mamba.throughput_rel > transformer.throughput_rel
 *     * rwkv.throughput_rel  > transformer.throughput_rel
 *
 *   σ-gated routing (exactly 4 fixtures, τ_mamba=0.40):
 *     Rule: σ_mamba ≤ τ_mamba → backend="mamba";
 *           σ_mamba >  τ_mamba → backend="transformer".
 *     Contract: ≥ 1 mamba AND ≥ 1 transformer (both
 *     branches fire).
 *
 *   Hybrid Jamba-style layer stack (exactly 8 layers):
 *     Alternating mamba / transformer (canonical), each
 *     with σ_arch ∈ [0, 1].  Contract: exactly 4 mamba
 *     + 4 transformer; σ_arch all ∈ [0, 1].
 *
 *   Task-level backend comparison (exactly 3 tasks):
 *     Each task: chosen backend + σ_task, rival σ_rival,
 *     and σ_task ≤ σ_rival for each row (σ picks the
 *     better backend per task).  Contract: chosen
 *     backends cover ≥ 2 distinct options across the 3
 *     tasks.
 *
 *   σ_mamba_kernel (surface hygiene):
 *       σ_mamba_kernel = 1 −
 *         (backends_ok + routes_ok + both_routes_ok +
 *          layers_ok + layer_split_ok + tasks_ok +
 *          task_diversity_ok) /
 *         (3 + 4 + 1 + 8 + 1 + 3 + 1)
 *   v0 requires `σ_mamba_kernel == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 3 backends in canonical order; mamba
 *        and rwkv have exponent 1; transformer has
 *        exponent 2; mamba/rwkv throughput_rel >
 *        transformer throughput_rel.
 *     2. Exactly 4 route fixtures with decision
 *        matching σ vs τ_mamba; both branches fire.
 *     3. Exactly 8 hybrid layers with alternating arch
 *        (mamba, transformer, mamba, transformer, ...);
 *        counts 4+4; σ_arch ∈ [0, 1].
 *     4. Exactly 3 tasks; σ_task ≤ σ_rival; ≥ 2
 *        distinct chosen backends.
 *     5. σ_mamba_kernel ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v267.1 (named, not implemented): live SSM /
 *     RWKV implementations via v262 hybrid engine,
 *     Jamba-style alternation running real layers,
 *     σ-gated per-query fallback to a loaded
 *     transformer kernel.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V267_MAMBA_H
#define COS_V267_MAMBA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V267_N_BACKENDS 3
#define COS_V267_N_ROUTES   4
#define COS_V267_N_LAYERS   8
#define COS_V267_N_TASKS    3

typedef struct {
    char  name[16];
    char  complexity[8]; /* "O(n)" or "O(n^2)" */
    int   exponent;
    float throughput_rel;
} cos_v267_backend_t;

typedef struct {
    char  query[16];
    float sigma_mamba;
    char  backend[16];  /* "mamba" or "transformer" */
} cos_v267_route_t;

typedef struct {
    int   idx;
    char  arch[16];     /* "mamba" or "transformer" */
    float sigma_arch;
} cos_v267_layer_t;

typedef struct {
    char  task[24];
    char  chosen[16];   /* one of backends */
    char  rival[16];
    float sigma_chosen;
    float sigma_rival;
} cos_v267_task_t;

typedef struct {
    cos_v267_backend_t  backends [COS_V267_N_BACKENDS];
    cos_v267_route_t    routes   [COS_V267_N_ROUTES];
    cos_v267_layer_t    layers   [COS_V267_N_LAYERS];
    cos_v267_task_t     tasks    [COS_V267_N_TASKS];

    float tau_mamba;

    int   n_backends_ok;
    int   n_routes_ok;
    int   n_mamba_routes;
    int   n_trans_routes;
    int   n_layers_ok;
    int   n_mamba_layers;
    int   n_trans_layers;
    int   n_tasks_ok;
    int   n_distinct_chosen;

    float sigma_mamba_kernel;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v267_state_t;

void   cos_v267_init(cos_v267_state_t *s, uint64_t seed);
void   cos_v267_run (cos_v267_state_t *s);

size_t cos_v267_to_json(const cos_v267_state_t *s,
                         char *buf, size_t cap);

int    cos_v267_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V267_MAMBA_H */
