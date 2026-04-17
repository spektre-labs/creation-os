/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v73 — σ-Omnimodal (the Creator) — implementation.
 *
 * Branchless integer primitives; libc-only hot path.  Every HV token
 * is 256 bits = 4 × uint64.  Arenas are 64-byte aligned.
 */

#include "omnimodal.h"

#include <stdlib.h>
#include <string.h>

const char cos_v73_version[] = "v73.0 omnimodal creator · chain wormhole hyperscale";

/* ====================================================================
 *  Internal helpers.
 * ==================================================================== */

static void *cos_v73__aligned_alloc(size_t bytes) {
    /* Round up to 64-byte multiple (aligned_alloc requires size % align == 0). */
    size_t rounded = (bytes + 63u) & ~((size_t)63u);
    void  *p       = aligned_alloc(64, rounded);
    if (p) memset(p, 0, rounded);
    return p;
}

static uint64_t cos_v73__splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Count bits in a 64-bit word without relying on __builtin_popcountll
 * semantics on unsigned-long overload; the GCC/Clang builtin is fine
 * on both Apple clang and portable GCC. */
static inline uint32_t cos_v73__popcount64(uint64_t x) {
    return (uint32_t)__builtin_popcountll(x);
}

/* ====================================================================
 *  HV primitives.
 * ==================================================================== */

void cos_v73_hv_zero(cos_v73_hv_t *dst) {
    for (uint32_t i = 0; i < COS_V73_HV_WORDS; ++i) dst->w[i] = 0;
}

void cos_v73_hv_copy(cos_v73_hv_t *dst, const cos_v73_hv_t *src) {
    for (uint32_t i = 0; i < COS_V73_HV_WORDS; ++i) dst->w[i] = src->w[i];
}

void cos_v73_hv_xor(cos_v73_hv_t *dst, const cos_v73_hv_t *a, const cos_v73_hv_t *b) {
    for (uint32_t i = 0; i < COS_V73_HV_WORDS; ++i) dst->w[i] = a->w[i] ^ b->w[i];
}

void cos_v73_hv_from_seed(cos_v73_hv_t *dst, uint64_t seed) {
    uint64_t s = seed ^ 0xD1B54A32D192ED03ULL;
    for (uint32_t i = 0; i < COS_V73_HV_WORDS; ++i) dst->w[i] = cos_v73__splitmix64(&s);
}

static const uint64_t COS_V73_MOD_SALT[8] = {
    0x0000000000000000ULL,   /* TEXT  */
    0xA5A5A5A5A5A5A5A5ULL,   /* IMAGE */
    0x5A5A5A5A5A5A5A5AULL,   /* AUDIO */
    0xF0F0F0F0F0F0F0F0ULL,   /* VIDEO */
    0x0F0F0F0F0F0F0F0FULL,   /* CODE  */
    0xCCCCCCCCCCCCCCCCULL,   /* 3D    */
    0x3333333333333333ULL,   /* WF    */
    0x9696969696969696ULL,   /* GAME  */
};

void cos_v73_hv_permute(cos_v73_hv_t       *dst,
                        const cos_v73_hv_t *src,
                        cos_v73_modality_t  m)
{
    uint32_t mi  = (uint32_t)m & 7u;
    uint64_t salt = COS_V73_MOD_SALT[mi];
    /* Cyclic rotate words by m + XOR with modality salt; deterministic. */
    for (uint32_t i = 0; i < COS_V73_HV_WORDS; ++i) {
        uint32_t j = (i + mi) & (COS_V73_HV_WORDS - 1u);
        uint64_t x = src->w[j];
        /* Rotate within word by 7*mi bits (mod 64). */
        uint32_t r = (7u * mi) & 63u;
        uint64_t rot = (r == 0) ? x : ((x << r) | (x >> ((64u - r) & 63u)));
        dst->w[i] = rot ^ salt ^ ((uint64_t)i * 0x9E3779B97F4A7C15ULL);
    }
}

uint32_t cos_v73_hv_hamming(const cos_v73_hv_t *a, const cos_v73_hv_t *b) {
    uint32_t d = 0;
    for (uint32_t i = 0; i < COS_V73_HV_WORDS; ++i) {
        d += cos_v73__popcount64(a->w[i] ^ b->w[i]);
    }
    return d;
}

/* ====================================================================
 *  1.  Universal modality tokenizer.
 * ==================================================================== */

cos_v73_codebook_t *cos_v73_codebook_new(uint32_t n,
                                         cos_v73_modality_t m,
                                         uint64_t           seed)
{
    if (n == 0 || n > COS_V73_CODEBOOK_MAX) return NULL;
    cos_v73_codebook_t *cb = (cos_v73_codebook_t *)calloc(1, sizeof(*cb));
    if (!cb) return NULL;
    cb->n        = n;
    cb->modality = (uint32_t)m;
    cb->book     = (cos_v73_hv_t *)cos_v73__aligned_alloc((size_t)n * sizeof(cos_v73_hv_t));
    if (!cb->book) { free(cb); return NULL; }
    uint64_t s = seed ^ ((uint64_t)m * 0xBF58476D1CE4E5B9ULL);
    for (uint32_t i = 0; i < n; ++i) {
        cos_v73_hv_from_seed(&cb->book[i], cos_v73__splitmix64(&s));
    }
    return cb;
}

void cos_v73_codebook_free(cos_v73_codebook_t *cb) {
    if (!cb) return;
    free(cb->book);
    free(cb);
}

uint32_t cos_v73_tokenize(const cos_v73_codebook_t *cb,
                          const cos_v73_hv_t       *q,
                          cos_v73_hv_t             *out)
{
    uint32_t best_idx  = 0;
    uint32_t best_dist = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < cb->n; ++i) {
        uint32_t d  = cos_v73_hv_hamming(&cb->book[i], q);
        uint32_t lt = (uint32_t)(-(int32_t)(d < best_dist));
        best_idx    = (best_idx  & ~lt) | (i  & lt);
        best_dist   = (best_dist & ~lt) | (d  & lt);
    }
    if (out) cos_v73_hv_copy(out, &cb->book[best_idx]);
    return best_idx;
}

uint32_t cos_v73_tokenize_rvq(const cos_v73_codebook_t *cb,
                              const cos_v73_hv_t       *q,
                              uint32_t                 *out_rvq,
                              cos_v73_hv_t             *out_reconstruction)
{
    cos_v73_hv_t stage1_hv;
    uint32_t stage1 = cos_v73_tokenize(cb, q, &stage1_hv);

    cos_v73_hv_t residual;
    cos_v73_hv_xor(&residual, q, &stage1_hv);

    cos_v73_hv_t stage2_hv;
    uint32_t stage2 = cos_v73_tokenize(cb, &residual, &stage2_hv);

    if (out_rvq) *out_rvq = stage2;
    if (out_reconstruction) cos_v73_hv_xor(out_reconstruction, &stage1_hv, &stage2_hv);
    return stage1;
}

/* ====================================================================
 *  2.  Rectified-flow integer scheduler.
 * ==================================================================== */

void cos_v73_flow_init(cos_v73_flow_t *f, uint32_t steps, uint32_t budget, uint64_t seed) {
    if (steps > COS_V73_FLOW_STEPS_MAX) steps = COS_V73_FLOW_STEPS_MAX;
    f->steps  = steps;
    f->budget = budget;
    f->_pad0  = 0;
    f->_pad1  = 0;
    uint64_t s = seed ^ 0xC3A5C85C97CB3127ULL;
    for (uint32_t i = 0; i < COS_V73_FLOW_STEPS_MAX; ++i) {
        f->sched[i] = cos_v73__splitmix64(&s);
    }
}

uint8_t cos_v73_flow_run(const cos_v73_flow_t *f,
                         const cos_v73_hv_t   *cond,
                         cos_v73_hv_t         *out,
                         uint32_t             *used_units)
{
    cos_v73_hv_t state;
    cos_v73_hv_copy(&state, cond);
    uint32_t units = 0;
    for (uint32_t k = 0; k < f->steps; ++k) {
        /* Rectified-flow integer step: state ← state ⊕ rot(sched[k]).
         * Deterministic, constant-time per step, no data-dependent
         * branches. */
        uint64_t m = f->sched[k];
        state.w[0] ^= m;
        state.w[1] ^= (m << 7)  | (m >> 57);
        state.w[2] ^= (m << 17) | (m >> 47);
        state.w[3] ^= (m << 31) | (m >> 33);
        units += 1u;
    }
    if (out) cos_v73_hv_copy(out, &state);
    if (used_units) *used_units = units;
    return (uint8_t)((units <= f->budget) & 0x1u);
}

/* ====================================================================
 *  3.  VINO cross-modal binding.
 * ==================================================================== */

void cos_v73_bind(cos_v73_hv_t              *out,
                  const cos_v73_mod_input_t *inputs,
                  uint32_t                   n)
{
    cos_v73_hv_zero(out);
    for (uint32_t i = 0; i < n; ++i) {
        cos_v73_hv_t p;
        cos_v73_hv_permute(&p, inputs[i].hv, inputs[i].mod);
        cos_v73_hv_xor(out, out, &p);
    }
}

uint8_t cos_v73_bind_verify(const cos_v73_mod_input_t *inputs,
                            uint32_t                   n,
                            const cos_v73_hv_t        *claim,
                            uint32_t                   tol)
{
    cos_v73_hv_t b;
    cos_v73_bind(&b, inputs, n);
    uint32_t d = cos_v73_hv_hamming(&b, claim);
    return (uint8_t)((d <= tol) & 0x1u);
}

/* ====================================================================
 *  4.  MOVA video+audio co-synth lock.
 * ==================================================================== */

uint8_t cos_v73_av_lock_verify(const cos_v73_av_lock_t *lock,
                               uint32_t                *out_lip_lag,
                               uint32_t                *out_fx_worst,
                               uint32_t                *out_tempo_err)
{
    uint32_t worst_fx     = 0;
    uint64_t tempo_accum  = 0;
    uint32_t lip_lag      = 0;
    for (uint32_t i = 0; i < lock->reel_len; ++i) {
        uint32_t d = cos_v73_hv_hamming(&lock->video[i], &lock->audio[i]);
        worst_fx = (d > worst_fx) ? d : worst_fx;
        tempo_accum += (uint64_t)d;
        /* Lip-lag proxy: popcount difference between frame and neighbour. */
        if (i > 0) {
            uint32_t d2 = cos_v73_hv_hamming(&lock->video[i - 1], &lock->audio[i]);
            uint32_t diff = (d2 > d) ? (d2 - d) : (d - d2);
            if (diff > lip_lag) lip_lag = diff;
        }
    }
    uint32_t tempo_err = (lock->reel_len == 0)
        ? 0u
        : (uint32_t)(tempo_accum / (uint64_t)lock->reel_len);

    if (out_lip_lag)    *out_lip_lag    = lip_lag;
    if (out_fx_worst)   *out_fx_worst   = worst_fx;
    if (out_tempo_err)  *out_tempo_err  = tempo_err;

    uint8_t lip_ok   = (uint8_t)((lip_lag    <= lock->lip_lag_max)    & 0x1u);
    uint8_t fx_ok    = (uint8_t)((worst_fx   <= lock->fx_thresh)      & 0x1u);
    uint8_t tempo_ok = (uint8_t)((tempo_err  <= lock->tempo_err_max)  & 0x1u);
    return (uint8_t)(lip_ok & fx_ok & tempo_ok);
}

/* ====================================================================
 *  5.  Matrix-Game world-model substrate.
 * ==================================================================== */

cos_v73_world_t *cos_v73_world_new(uint32_t n_tiles, uint64_t seed) {
    if (n_tiles == 0 || n_tiles > COS_V73_WORLD_TILES_MAX) return NULL;
    cos_v73_world_t *w = (cos_v73_world_t *)calloc(1, sizeof(*w));
    if (!w) return NULL;
    w->n_tiles   = n_tiles;
    w->camera_idx = 0;
    w->tiles     = (cos_v73_hv_t *)cos_v73__aligned_alloc((size_t)n_tiles * sizeof(cos_v73_hv_t));
    if (!w->tiles) { free(w); return NULL; }
    uint64_t s = seed ^ 0x85EBCA6B5A7C5A5AULL;
    for (uint32_t i = 0; i < n_tiles; ++i) {
        cos_v73_hv_from_seed(&w->tiles[i], cos_v73__splitmix64(&s));
    }
    cos_v73_hv_from_seed(&w->camera_tag, cos_v73__splitmix64(&s));
    cos_v73_hv_zero(&w->memory_digest);
    return w;
}

void cos_v73_world_free(cos_v73_world_t *w) {
    if (!w) return;
    free(w->tiles);
    free(w);
}

void cos_v73_world_step(cos_v73_world_t      *w,
                        const cos_v73_hv_t   *action,
                        uint32_t              next_camera_idx)
{
    if (next_camera_idx >= w->n_tiles) next_camera_idx = w->n_tiles - 1u;
    cos_v73_hv_t perm_cam;
    cos_v73_hv_permute(&perm_cam, &w->camera_tag, COS_V73_MOD_GAME);

    cos_v73_hv_t new_tag;
    cos_v73_hv_xor(&new_tag, &w->tiles[next_camera_idx], action);
    cos_v73_hv_xor(&new_tag, &new_tag, &perm_cam);
    cos_v73_hv_copy(&w->tiles[next_camera_idx], &new_tag);

    /* Update memory digest = XOR bundle of visited tiles. */
    cos_v73_hv_xor(&w->memory_digest, &w->memory_digest, &new_tag);

    /* New camera tag = XOR of old camera + action (cheap update). */
    cos_v73_hv_xor(&w->camera_tag, &w->camera_tag, action);
    w->camera_idx = next_camera_idx;
}

uint32_t cos_v73_world_retrieve(const cos_v73_world_t *w,
                                const cos_v73_hv_t    *q,
                                uint32_t              *out_dist)
{
    uint32_t best_idx  = 0;
    uint32_t best_dist = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < w->n_tiles; ++i) {
        uint32_t d  = cos_v73_hv_hamming(&w->tiles[i], q);
        uint32_t lt = (uint32_t)(-(int32_t)(d < best_dist));
        best_idx    = (best_idx  & ~lt) | (i & lt);
        best_dist   = (best_dist & ~lt) | (d & lt);
    }
    if (out_dist) *out_dist = best_dist;
    return best_idx;
}

/* ====================================================================
 *  6.  DiReCT physics-coherence gate.
 * ==================================================================== */

uint8_t cos_v73_physics_gate(int32_t  delta_momentum,
                             int32_t  momentum_tol,
                             uint32_t collision_energy,
                             uint32_t collision_tol)
{
    int32_t  abs_dm = delta_momentum < 0 ? -delta_momentum : delta_momentum;
    uint8_t  mom_ok = (uint8_t)((abs_dm           <= momentum_tol) & 0x1);
    uint8_t  col_ok = (uint8_t)((collision_energy <= collision_tol) & 0x1);
    return (uint8_t)(mom_ok & col_ok);
}

/* ====================================================================
 *  7.  Workflow DAG executor.
 * ==================================================================== */

static uint32_t cos_v73__row_words(uint32_t n) {
    return (n + 63u) / 64u;
}

cos_v73_workflow_t *cos_v73_workflow_new(uint32_t n_nodes) {
    if (n_nodes == 0 || n_nodes > COS_V73_WORKFLOW_NODES_MAX) return NULL;
    cos_v73_workflow_t *wf = (cos_v73_workflow_t *)calloc(1, sizeof(*wf));
    if (!wf) return NULL;
    wf->n          = n_nodes;
    wf->row_words  = cos_v73__row_words(n_nodes);
    wf->faults     = 0;
    wf->adj   = (uint64_t *)cos_v73__aligned_alloc((size_t)n_nodes * wf->row_words * sizeof(uint64_t));
    wf->tags  = (cos_v73_hv_t *)cos_v73__aligned_alloc((size_t)n_nodes * sizeof(cos_v73_hv_t));
    wf->ready = (uint64_t *)cos_v73__aligned_alloc((size_t)wf->row_words * sizeof(uint64_t));
    wf->done  = (uint64_t *)cos_v73__aligned_alloc((size_t)wf->row_words * sizeof(uint64_t));
    if (!wf->adj || !wf->tags || !wf->ready || !wf->done) {
        cos_v73_workflow_free(wf);
        return NULL;
    }
    for (uint32_t i = 0; i < n_nodes; ++i) {
        cos_v73_hv_from_seed(&wf->tags[i], (uint64_t)i * 0x9E3779B97F4A7C15ULL + 1);
    }
    return wf;
}

void cos_v73_workflow_free(cos_v73_workflow_t *wf) {
    if (!wf) return;
    free(wf->adj);
    free(wf->tags);
    free(wf->ready);
    free(wf->done);
    free(wf);
}

void cos_v73_workflow_add_edge(cos_v73_workflow_t *wf, uint32_t from, uint32_t to) {
    if (from >= wf->n || to >= wf->n) return;
    /* adj[from] has bit `to` set: meaning `from → to`. */
    wf->adj[(size_t)from * wf->row_words + (to / 64u)] |= ((uint64_t)1u << (to % 64u));
}

void cos_v73_workflow_set_tag(cos_v73_workflow_t *wf, uint32_t i, const cos_v73_hv_t *tag) {
    if (i >= wf->n) return;
    cos_v73_hv_copy(&wf->tags[i], tag);
}

/* indeg[i] = count of predecessors of i (bits set in column i across all rows). */
static uint32_t cos_v73__indeg(const cos_v73_workflow_t *wf, uint32_t i) {
    uint32_t count = 0;
    uint64_t mask  = (uint64_t)1u << (i % 64u);
    uint32_t word  = i / 64u;
    for (uint32_t r = 0; r < wf->n; ++r) {
        if (wf->adj[(size_t)r * wf->row_words + word] & mask) count++;
    }
    return count;
}

uint8_t cos_v73_workflow_execute(cos_v73_workflow_t *wf,
                                 uint32_t            max_rounds,
                                 uint32_t            fault_budget,
                                 uint32_t           *score)
{
    memset(wf->ready, 0, (size_t)wf->row_words * sizeof(uint64_t));
    memset(wf->done,  0, (size_t)wf->row_words * sizeof(uint64_t));
    uint32_t *indeg = (uint32_t *)calloc(wf->n, sizeof(uint32_t));
    if (!indeg) return 0;
    for (uint32_t i = 0; i < wf->n; ++i) {
        indeg[i] = cos_v73__indeg(wf, i);
        if (indeg[i] == 0) wf->ready[i / 64u] |= ((uint64_t)1u << (i % 64u));
        if (score) score[i] = 0;
    }

    uint32_t done_count = 0;
    uint32_t rounds     = 0;
    for (rounds = 0; rounds < max_rounds; ++rounds) {
        uint32_t wave_count = 0;
        for (uint32_t i = 0; i < wf->n; ++i) {
            uint64_t rmask = ((uint64_t)1u << (i % 64u));
            if (wf->ready[i / 64u] & rmask) {
                wf->done[i / 64u] |= rmask;
                wf->ready[i / 64u] &= ~rmask;
                done_count++;
                wave_count++;
                /* For each successor j, decrement indeg[j]; if it hits 0 mark ready. */
                for (uint32_t rw = 0; rw < wf->row_words; ++rw) {
                    uint64_t row = wf->adj[(size_t)i * wf->row_words + rw];
                    while (row) {
                        uint32_t b = (uint32_t)__builtin_ctzll(row);
                        row &= row - 1u;
                        uint32_t j = rw * 64u + b;
                        if (j < wf->n && !(wf->done[j / 64u] & ((uint64_t)1u << (j % 64u)))) {
                            if (indeg[j] > 0) indeg[j]--;
                            if (indeg[j] == 0) {
                                wf->ready[j / 64u] |= ((uint64_t)1u << (j % 64u));
                            }
                            if (score) score[j] += 1u;
                        }
                    }
                }
            }
        }
        if (wave_count == 0) break;
    }
    /* Fault count: nodes not done. */
    uint32_t faults = wf->n - done_count;
    wf->faults = faults;
    free(indeg);
    uint8_t all_done  = (uint8_t)((done_count == wf->n) & 0x1u);
    uint8_t under_f   = (uint8_t)((faults <= fault_budget) & 0x1u);
    uint8_t in_rounds = (uint8_t)((rounds <= max_rounds) & 0x1u);
    return (uint8_t)(all_done & under_f & in_rounds);
}

/* ====================================================================
 *  8.  Code-edit planner.
 * ==================================================================== */

cos_v73_plan_graph_t *cos_v73_plan_new(uint32_t n_files) {
    if (n_files == 0 || n_files > COS_V73_PLAN_FILES_MAX) return NULL;
    cos_v73_plan_graph_t *g = (cos_v73_plan_graph_t *)calloc(1, sizeof(*g));
    if (!g) return NULL;
    g->n_files   = n_files;
    g->row_words = cos_v73__row_words(n_files);
    g->dep       = (uint64_t *)cos_v73__aligned_alloc((size_t)n_files * g->row_words * sizeof(uint64_t));
    if (!g->dep) { free(g); return NULL; }
    return g;
}

void cos_v73_plan_free(cos_v73_plan_graph_t *g) {
    if (!g) return;
    free(g->dep);
    free(g);
}

void cos_v73_plan_add_dep(cos_v73_plan_graph_t *g, uint32_t from, uint32_t to) {
    if (from >= g->n_files || to >= g->n_files) return;
    g->dep[(size_t)from * g->row_words + (to / 64u)] |= ((uint64_t)1u << (to % 64u));
}

/* Cycle detection via bounded DFS marking. */
static int cos_v73__plan_dfs(const cos_v73_plan_graph_t *g,
                             uint32_t                    v,
                             uint8_t                    *state,
                             uint32_t                    depth,
                             uint32_t                    depth_budget)
{
    if (depth > depth_budget) return -1;
    if (state[v] == 2) return 0;
    if (state[v] == 1) return -1;   /* cycle */
    state[v] = 1;
    for (uint32_t rw = 0; rw < g->row_words; ++rw) {
        uint64_t row = g->dep[(size_t)v * g->row_words + rw];
        while (row) {
            uint32_t b = (uint32_t)__builtin_ctzll(row);
            row &= row - 1u;
            uint32_t u = rw * 64u + b;
            if (u < g->n_files) {
                int r = cos_v73__plan_dfs(g, u, state, depth + 1u, depth_budget);
                if (r != 0) return r;
            }
        }
    }
    state[v] = 2;
    return 0;
}

uint8_t cos_v73_plan_verify(const cos_v73_plan_graph_t *g,
                            const cos_v73_plan_step_t  *steps,
                            uint32_t                    n_steps,
                            uint32_t                    repair_budget,
                            const cos_v73_hv_t         *manifest_claim,
                            uint32_t                    manifest_tol)
{
    /* (a) bound check + repair count */
    uint32_t repairs = 0;
    for (uint32_t i = 0; i < n_steps; ++i) {
        if (steps[i].file_idx >= g->n_files) return 0;
        repairs += steps[i].repair_count;
    }
    if (repairs > repair_budget) return 0;

    /* (b) cycle-free dep graph */
    uint8_t *state = (uint8_t *)calloc(g->n_files, sizeof(uint8_t));
    if (!state) return 0;
    int cycle = 0;
    for (uint32_t v = 0; v < g->n_files && cycle == 0; ++v) {
        if (state[v] == 0) {
            if (cos_v73__plan_dfs(g, v, state, 0, g->n_files + 1u) != 0) cycle = 1;
        }
    }
    free(state);
    if (cycle) return 0;

    /* (c) XOR manifest */
    cos_v73_hv_t manifest;
    cos_v73_hv_zero(&manifest);
    for (uint32_t i = 0; i < n_steps; ++i) {
        cos_v73_hv_xor(&manifest, &manifest, &steps[i].diff_tag);
    }
    uint32_t d = cos_v73_hv_hamming(&manifest, manifest_claim);
    return (uint8_t)((d <= manifest_tol) & 0x1u);
}

/* ====================================================================
 *  9.  Asset navigation (GTA-tier small-world Hamming descent).
 * ==================================================================== */

uint8_t cos_v73_asset_navigate(const cos_v73_world_t *w,
                               const cos_v73_hv_t    *anchor,
                               const cos_v73_hv_t    *target,
                               uint32_t               hop_budget,
                               cos_v73_hv_t          *out_path_hv,
                               uint32_t              *out_hops)
{
    cos_v73_hv_t cur;
    cos_v73_hv_copy(&cur, anchor);
    cos_v73_hv_t path;
    cos_v73_hv_zero(&path);
    uint32_t hops = 0;
    uint32_t cur_dist = cos_v73_hv_hamming(&cur, target);
    for (hops = 0; hops < hop_budget && cur_dist > 0; ++hops) {
        /* Walk to the tile closest to target. */
        uint32_t best_idx  = 0;
        uint32_t best_dist = 0xFFFFFFFFu;
        for (uint32_t i = 0; i < w->n_tiles; ++i) {
            uint32_t d = cos_v73_hv_hamming(&w->tiles[i], target);
            if (d < best_dist) { best_dist = d; best_idx = i; }
        }
        cos_v73_hv_xor(&path, &path, &w->tiles[best_idx]);
        cos_v73_hv_copy(&cur, &w->tiles[best_idx]);
        cur_dist = cos_v73_hv_hamming(&cur, target);
        if (best_dist == 0) { hops++; break; }
        /* If no improvement, break. */
        if (best_dist >= cos_v73_hv_hamming(anchor, target)) break;
    }
    if (out_path_hv) cos_v73_hv_copy(out_path_hv, &path);
    if (out_hops)    *out_hops = hops;
    uint8_t hop_ok = (uint8_t)((hops <= hop_budget) & 0x1u);
    uint8_t arrive = (uint8_t)((cur_dist == 0) & 0x1u);
    return (uint8_t)(hop_ok & arrive);
}

/* ====================================================================
 * 10.  OML bytecode VM.
 * ==================================================================== */

struct cos_v73_oml_state {
    uint64_t units;
    uint8_t  flow_ok;
    uint8_t  bind_ok;
    uint8_t  lock_ok;
    uint8_t  physics_ok;
    uint8_t  plan_ok;
    uint8_t  workflow_ok;
    uint8_t  abstained;
    uint8_t  v73_ok;
};

cos_v73_oml_state_t *cos_v73_oml_new(void) {
    cos_v73_oml_state_t *s = (cos_v73_oml_state_t *)calloc(1, sizeof(*s));
    return s;
}

void cos_v73_oml_free(cos_v73_oml_state_t *s) { free(s); }

void cos_v73_oml_reset(cos_v73_oml_state_t *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

uint64_t cos_v73_oml_units   (const cos_v73_oml_state_t *s) { return s ? s->units : 0; }
uint8_t  cos_v73_oml_flow_ok     (const cos_v73_oml_state_t *s) { return s ? s->flow_ok     : 0; }
uint8_t  cos_v73_oml_bind_ok     (const cos_v73_oml_state_t *s) { return s ? s->bind_ok     : 0; }
uint8_t  cos_v73_oml_lock_ok     (const cos_v73_oml_state_t *s) { return s ? s->lock_ok     : 0; }
uint8_t  cos_v73_oml_physics_ok  (const cos_v73_oml_state_t *s) { return s ? s->physics_ok  : 0; }
uint8_t  cos_v73_oml_plan_ok     (const cos_v73_oml_state_t *s) { return s ? s->plan_ok     : 0; }
uint8_t  cos_v73_oml_workflow_ok (const cos_v73_oml_state_t *s) { return s ? s->workflow_ok : 0; }
uint8_t  cos_v73_oml_abstained   (const cos_v73_oml_state_t *s) { return s ? s->abstained   : 0; }
uint8_t  cos_v73_oml_v73_ok      (const cos_v73_oml_state_t *s) { return s ? s->v73_ok      : 0; }

static const uint32_t COS_V73_OP_COST[10] = { 1, 2, 8, 2, 4, 16, 4, 4, 8, 1 };

int cos_v73_oml_exec(cos_v73_oml_state_t      *s,
                     const cos_v73_oml_inst_t *prog,
                     uint32_t                  n,
                     cos_v73_oml_ctx_t        *ctx,
                     uint64_t                  unit_budget)
{
    if (!s || !prog || !ctx) return -1;
    cos_v73_oml_reset(s);
    s->abstained = ctx->abstain;

    for (uint32_t pc = 0; pc < n; ++pc) {
        uint8_t op = prog[pc].op;
        if (op >= 10u) return -1;
        s->units += COS_V73_OP_COST[op];
        if (s->units > unit_budget) return -2;

        switch ((cos_v73_op_t)op) {
            case COS_V73_OP_HALT:
                /* no-op */
                return 0;
            case COS_V73_OP_TOKENIZE:
                if (ctx->cb && ctx->tok_input) {
                    (void)cos_v73_tokenize(ctx->cb, ctx->tok_input, NULL);
                }
                break;
            case COS_V73_OP_FLOW:
                if (ctx->flow && ctx->flow_cond) {
                    uint32_t used = 0;
                    cos_v73_hv_t out;
                    s->flow_ok = cos_v73_flow_run(ctx->flow, ctx->flow_cond, &out, &used);
                }
                break;
            case COS_V73_OP_BIND:
                if (ctx->bind_inputs && ctx->bind_claim) {
                    s->bind_ok = cos_v73_bind_verify(ctx->bind_inputs,
                                                     ctx->bind_n,
                                                     ctx->bind_claim,
                                                     ctx->bind_tol);
                }
                break;
            case COS_V73_OP_LOCK:
                if (ctx->lock) {
                    s->lock_ok = cos_v73_av_lock_verify(ctx->lock, NULL, NULL, NULL);
                }
                break;
            case COS_V73_OP_WORLD:
                if (ctx->world && ctx->world_action) {
                    cos_v73_world_step(ctx->world, ctx->world_action, ctx->world_next_cam);
                }
                break;
            case COS_V73_OP_PHYSICS:
                s->physics_ok = cos_v73_physics_gate(ctx->phys_dmom,
                                                      ctx->phys_dmom_tol,
                                                      ctx->phys_ce,
                                                      ctx->phys_ce_tol);
                break;
            case COS_V73_OP_WORKFLOW:
                if (ctx->workflow) {
                    s->workflow_ok = cos_v73_workflow_execute(ctx->workflow,
                                                               ctx->workflow_rounds,
                                                               ctx->workflow_faults,
                                                               NULL);
                }
                break;
            case COS_V73_OP_PLAN:
                if (ctx->plan_graph && ctx->plan_steps && ctx->plan_claim) {
                    s->plan_ok = cos_v73_plan_verify(ctx->plan_graph,
                                                      ctx->plan_steps,
                                                      ctx->plan_n,
                                                      ctx->plan_repair,
                                                      ctx->plan_claim,
                                                      ctx->plan_tol);
                }
                break;
            case COS_V73_OP_GATE: {
                uint8_t units_ok = (uint8_t)((s->units <= unit_budget) & 0x1u);
                s->v73_ok = (uint8_t)(units_ok
                                    & s->flow_ok
                                    & s->bind_ok
                                    & s->lock_ok
                                    & s->physics_ok
                                    & s->plan_ok
                                    & s->workflow_ok
                                    & (uint8_t)(1u - (s->abstained & 0x1u)));
                break;
            }
        }
    }
    return 0;
}

/* ====================================================================
 *  Composed 14-bit branchless decision.
 * ==================================================================== */

cos_v73_decision_t cos_v73_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok,
                                            uint8_t v69_ok,
                                            uint8_t v70_ok,
                                            uint8_t v71_ok,
                                            uint8_t v72_ok,
                                            uint8_t v73_ok)
{
    cos_v73_decision_t d;
    d.v60_ok = (uint8_t)(v60_ok & 0x1u);
    d.v61_ok = (uint8_t)(v61_ok & 0x1u);
    d.v62_ok = (uint8_t)(v62_ok & 0x1u);
    d.v63_ok = (uint8_t)(v63_ok & 0x1u);
    d.v64_ok = (uint8_t)(v64_ok & 0x1u);
    d.v65_ok = (uint8_t)(v65_ok & 0x1u);
    d.v66_ok = (uint8_t)(v66_ok & 0x1u);
    d.v67_ok = (uint8_t)(v67_ok & 0x1u);
    d.v68_ok = (uint8_t)(v68_ok & 0x1u);
    d.v69_ok = (uint8_t)(v69_ok & 0x1u);
    d.v70_ok = (uint8_t)(v70_ok & 0x1u);
    d.v71_ok = (uint8_t)(v71_ok & 0x1u);
    d.v72_ok = (uint8_t)(v72_ok & 0x1u);
    d.v73_ok = (uint8_t)(v73_ok & 0x1u);
    d.allow  = (uint8_t)(d.v60_ok & d.v61_ok & d.v62_ok & d.v63_ok
                       & d.v64_ok & d.v65_ok & d.v66_ok & d.v67_ok
                       & d.v68_ok & d.v69_ok & d.v70_ok & d.v71_ok
                       & d.v72_ok & d.v73_ok);
    d._pad = 0;
    return d;
}
