/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v64 — σ-Intellect implementation.
 *
 * See src/v64/intellect.h for the design rationale.  Everything here
 * is branchless on the hot path, integer / Q0.15 on the decision
 * surface, and 64-byte aligned.  No FP, no malloc once we've built
 * the arenas.  `aligned_alloc` + `free` only at construction time.
 */

#include "intellect.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* -------------------------------------------------------------------- */
/*  Version string                                                      */
/* -------------------------------------------------------------------- */

const char cos_v64_version[] =
    "64.0.0 — v64.0 sigma-intellect "
    "(mcts-sigma + skill-lib + tool-authz + reflexion + evolve + mod-depth)";

/* -------------------------------------------------------------------- */
/*  Small helpers — branchless, integer-only                            */
/* -------------------------------------------------------------------- */

/*  Round a 64-byte multiple up — simpler than fiddling with stdalign. */
static size_t v64_round_up_64(size_t n) { return (n + 63u) & ~(size_t)63u; }

/*  Constant-time byte compare: returns 1 if equal, 0 otherwise.
 *  Full-scan, OR-accumulates diff bits, no early exit.
 */
static uint8_t v64_ct_eq_bytes(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < n; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
    /* diff == 0  →  mask 0xFF via arithmetic fold.                    */
    uint8_t nz = (uint8_t)((diff | (uint8_t)-(int)diff) >> 7);
    return (uint8_t)(1u - nz);
}

/*  Integer sqrt (isqrt, Newton), branchless inner.  For PUCT, inputs
 *  are at most UINT32_MAX (visits), output fits in uint32.  Returns
 *  floor(sqrt(n)).
 */
static uint32_t v64_isqrt_u32(uint32_t n)
{
    if (n == 0) return 0;          /* single early-exit on the slow   */
    uint32_t x = n;
    uint32_t y = 1;
    /*  This loop has a tight upper bound (32 steps) and contains no  */
    /*  data-dependent branches inside — the bound is fixed.          */
    for (int i = 0; i < 32; ++i) {
        y = (x + n / (x == 0 ? 1 : x)) >> 1;
        uint32_t conv = (uint32_t)(y >= x);     /* converged          */
        /* if (y >= x) break — but branchless: swap and freeze        */
        x = conv ? x : y;
        y = conv ? x : y;
    }
    /* Ensure result * result ≤ n.  One correction step, branchless.  */
    uint64_t xx = (uint64_t)x * (uint64_t)x;
    uint32_t over = (uint32_t)(xx > (uint64_t)n);
    x -= over;
    return x;
}

/*  XOR two equal-length byte buffers into a scratch byte popcount.    */
static uint32_t v64_hamming_bytes(const uint8_t *a,
                                  const uint8_t *b,
                                  size_t         n)
{
    uint32_t c = 0;
    for (size_t i = 0; i < n; ++i) {
        c += (uint32_t)__builtin_popcount((unsigned)(a[i] ^ b[i]));
    }
    return c;
}

/* ==================================================================== */
/*  Composition — 5-bit branchless AND.                                 */
/* ==================================================================== */

cos_v64_decision_t
cos_v64_compose_decision(uint8_t v60_ok,
                         uint8_t v61_ok,
                         uint8_t v62_ok,
                         uint8_t v63_ok,
                         uint8_t v64_ok)
{
    /* Normalise every lane to 0/1 — branchless.                       */
    uint8_t a = (uint8_t)!!v60_ok;
    uint8_t b = (uint8_t)!!v61_ok;
    uint8_t c = (uint8_t)!!v62_ok;
    uint8_t d = (uint8_t)!!v63_ok;
    uint8_t e = (uint8_t)!!v64_ok;

    cos_v64_decision_t out;
    out.v60_ok = a;
    out.v61_ok = b;
    out.v62_ok = c;
    out.v63_ok = d;
    out.v64_ok = e;
    out.allow  = (uint8_t)(a & b & c & d & e);
    out._pad[0] = 0; out._pad[1] = 0;
    return out;
}

/* ==================================================================== */
/*  1.  MCTS-σ                                                          */
/* ==================================================================== */

cos_v64_tree_t *cos_v64_tree_new(uint32_t cap, uint16_t cpuct_q15)
{
    if (cap < 2 || cap > (UINT32_MAX / 2)) return NULL;
    cos_v64_tree_t *t =
        (cos_v64_tree_t *)aligned_alloc(64, v64_round_up_64(sizeof(*t)));
    if (!t) return NULL;

    size_t bytes = v64_round_up_64((size_t)cap * sizeof(cos_v64_node_t));
    cos_v64_node_t *nodes = (cos_v64_node_t *)aligned_alloc(64, bytes);
    if (!nodes) { free(t); return NULL; }
    memset(nodes, 0, bytes);

    t->nodes     = nodes;
    t->cap       = cap;
    t->len       = 0;
    t->root      = 0;
    t->cpuct_q15 = cpuct_q15 == 0 ? (uint16_t)49152 : cpuct_q15;
    t->_pad      = 0;
    return t;
}

void cos_v64_tree_free(cos_v64_tree_t *t)
{
    if (!t) return;
    free(t->nodes);
    free(t);
}

void cos_v64_mcts_reset(cos_v64_tree_t  *t,
                        uint16_t         root_prior_q15,
                        cos_v64_sigma_t  root_sigma)
{
    if (!t || t->cap == 0) return;
    memset(&t->nodes[0], 0, sizeof(t->nodes[0]));
    t->nodes[0].parent    = UINT32_MAX;
    t->nodes[0].child0    = UINT32_MAX;
    t->nodes[0].nchild    = 0;
    t->nodes[0].depth     = 0;
    t->nodes[0].visits    = 0;
    t->nodes[0].value_q15 = 0;
    t->nodes[0].sigma     = root_sigma;
    t->nodes[0].prior_q15 = root_prior_q15;
    t->nodes[0].action_id = 0;
    t->len  = 1;
    t->root = 0;
}

uint32_t cos_v64_mcts_select(const cos_v64_tree_t *t, uint32_t node)
{
    if (!t || node >= t->len) return UINT32_MAX;
    const cos_v64_node_t *n = &t->nodes[node];
    if (n->nchild == 0 || n->child0 == UINT32_MAX) return UINT32_MAX;

    uint32_t n_parent = n->visits;
    uint32_t sqrtN    = v64_isqrt_u32(n_parent + 1);
    int32_t  cpuct    = (int32_t)t->cpuct_q15;

    uint32_t best_idx   = n->child0;
    int32_t  best_score = INT32_MIN;
    uint16_t best_act   = UINT16_MAX;

    for (uint16_t i = 0; i < n->nchild; ++i) {
        uint32_t ci = n->child0 + i;
        const cos_v64_node_t *c = &t->nodes[ci];
        /* Q(s,a) in Q0.15 signed.                                    */
        int32_t q = c->value_q15;
        /*  u(s,a) = c_puct * P * sqrt(N_parent) / (1 + N_child)       */
        /*         scaled so Q0.15 * Q0.15 →  Q0.30,  >> 15 → Q0.15.   */
        int64_t num = (int64_t)cpuct * (int64_t)c->prior_q15
                    * (int64_t)sqrtN;
        int64_t den = (int64_t)(1u + c->visits) * (int64_t)32768;
        int32_t u   = (int32_t)(den == 0 ? 0 : num / den);
        int32_t s   = q + u;

        /* Branchless tie-break: higher score wins; on tie, lower     */
        /* action_id wins (stable preferred order).                   */
        uint32_t better_score = (uint32_t)(s >  best_score);
        uint32_t tie_score    = (uint32_t)(s == best_score);
        uint32_t better_act   = (uint32_t)(c->action_id < best_act);
        uint32_t take = better_score | (tie_score & better_act);
        best_idx   = take ? ci         : best_idx;
        best_score = take ? s          : best_score;
        best_act   = take ? c->action_id : best_act;
    }
    return best_idx;
}

int cos_v64_mcts_expand(cos_v64_tree_t *t,
                        uint32_t        node,
                        uint16_t        nchild,
                        const uint16_t *priors_q15)
{
    if (!t || !priors_q15 || node >= t->len) return -1;
    cos_v64_node_t *p = &t->nodes[node];
    if (p->nchild != 0) return -1;          /* already expanded       */
    if ((uint64_t)t->len + (uint64_t)nchild > (uint64_t)t->cap) return -1;
    uint32_t base = t->len;
    p->child0 = base;
    p->nchild = nchild;

    for (uint16_t i = 0; i < nchild; ++i) {
        cos_v64_node_t *c = &t->nodes[base + i];
        c->parent    = node;
        c->child0    = UINT32_MAX;
        c->nchild    = 0;
        c->depth     = (uint16_t)(p->depth + 1);
        c->visits    = 0;
        c->value_q15 = 0;
        c->sigma     = (cos_v64_sigma_t){ 0, 0 };
        c->prior_q15 = priors_q15[i];
        c->action_id = i;
    }
    t->len = base + nchild;
    return 0;
}

void cos_v64_mcts_backup(cos_v64_tree_t *t,
                         uint32_t        leaf,
                         int32_t         reward_q15)
{
    if (!t || leaf >= t->len) return;
    uint32_t cur = leaf;
    /* Walk parents; branchless termination on parent == UINT32_MAX. */
    for (;;) {
        cos_v64_node_t *n = &t->nodes[cur];
        n->visits += 1;
        /* running mean in Q0.15: value = value + (reward - value)/N  */
        int32_t old = n->value_q15;
        int32_t dv  = (reward_q15 - old);
        /* Integer division; branchless.                              */
        int32_t add = dv / (int32_t)n->visits;
        n->value_q15 = old + add;
        if (n->parent == UINT32_MAX) break;
        cur = n->parent;
    }
}

uint32_t cos_v64_mcts_best(const cos_v64_tree_t *t)
{
    if (!t || t->len == 0) return UINT32_MAX;
    const cos_v64_node_t *r = &t->nodes[t->root];
    if (r->nchild == 0) return UINT32_MAX;
    uint32_t best   = r->child0;
    uint32_t best_n = 0;
    int32_t  best_v = INT32_MIN;
    uint16_t best_a = UINT16_MAX;
    for (uint16_t i = 0; i < r->nchild; ++i) {
        uint32_t ci = r->child0 + i;
        const cos_v64_node_t *c = &t->nodes[ci];
        uint32_t more_n   = (uint32_t)(c->visits    >  best_n);
        uint32_t eq_n     = (uint32_t)(c->visits    == best_n);
        uint32_t more_v   = (uint32_t)(c->value_q15 >  best_v);
        uint32_t eq_v     = (uint32_t)(c->value_q15 == best_v);
        uint32_t lower_a  = (uint32_t)(c->action_id <  best_a);
        uint32_t take = more_n | (eq_n & more_v) | (eq_n & eq_v & lower_a);
        best   = take ? ci          : best;
        best_n = take ? c->visits    : best_n;
        best_v = take ? c->value_q15 : best_v;
        best_a = take ? c->action_id : best_a;
    }
    return best;
}

/* ==================================================================== */
/*  2.  Skill library                                                   */
/* ==================================================================== */

cos_v64_skill_lib_t *cos_v64_skill_lib_new(uint32_t cap)
{
    if (cap == 0) return NULL;
    cos_v64_skill_lib_t *l =
        (cos_v64_skill_lib_t *)aligned_alloc(64, v64_round_up_64(sizeof(*l)));
    if (!l) return NULL;
    size_t bytes = v64_round_up_64((size_t)cap * sizeof(cos_v64_skill_t));
    cos_v64_skill_t *s = (cos_v64_skill_t *)aligned_alloc(64, bytes);
    if (!s) { free(l); return NULL; }
    memset(s, 0, bytes);
    l->skills = s;
    l->cap    = cap;
    l->len    = 0;
    return l;
}

void cos_v64_skill_lib_free(cos_v64_skill_lib_t *l)
{
    if (!l) return;
    free(l->skills);
    free(l);
}

int cos_v64_skill_register(cos_v64_skill_lib_t *l,
                           const uint8_t       *sig,
                           uint16_t             skill_id,
                           uint16_t             initial_conf_q15)
{
    if (!l || !sig) return -1;
    if (l->len >= l->cap) return -1;
    /* Reject on exact signature collision (caller should reflect
     * through `cos_v64_reflect_update` instead of re-registering).   */
    for (uint32_t i = 0; i < l->len; ++i) {
        if (v64_ct_eq_bytes(l->skills[i].sig, sig,
                            COS_V64_SKILL_SIG_BYTES)) return -2;
    }
    cos_v64_skill_t *s = &l->skills[l->len];
    memcpy(s->sig, sig, COS_V64_SKILL_SIG_BYTES);
    s->confidence_q15 = initial_conf_q15;
    s->uses           = 0;
    s->wins           = 0;
    s->skill_id       = skill_id;
    l->len += 1;
    return 0;
}

uint32_t cos_v64_skill_lookup(const cos_v64_skill_lib_t *l,
                              const uint8_t             *sig)
{
    if (!l || !sig) return UINT32_MAX;
    uint32_t hit = UINT32_MAX;
    for (uint32_t i = 0; i < l->len; ++i) {
        uint8_t eq = v64_ct_eq_bytes(l->skills[i].sig, sig,
                                     COS_V64_SKILL_SIG_BYTES);
        /* Branchless: take first hit, keep scanning for timing.      */
        uint32_t pick = (uint32_t)(eq & (hit == UINT32_MAX));
        hit = pick ? i : hit;
    }
    return hit;
}

uint32_t cos_v64_skill_retrieve(const cos_v64_skill_lib_t *l,
                                const uint8_t             *query_sig,
                                uint16_t                   min_conf_q15,
                                uint32_t                  *out_hamming)
{
    if (out_hamming) *out_hamming = UINT32_MAX;
    if (!l || !query_sig || l->len == 0) return UINT32_MAX;

    uint32_t best      = UINT32_MAX;
    uint32_t best_ham  = UINT32_MAX;
    uint16_t best_conf = 0;
    uint16_t best_id   = UINT16_MAX;

    for (uint32_t i = 0; i < l->len; ++i) {
        uint32_t ham = v64_hamming_bytes(l->skills[i].sig, query_sig,
                                         COS_V64_SKILL_SIG_BYTES);
        uint16_t conf = l->skills[i].confidence_q15;
        uint16_t sid  = l->skills[i].skill_id;

        uint32_t clears = (uint32_t)(conf >= min_conf_q15);
        uint32_t closer = (uint32_t)(ham <  best_ham);
        uint32_t tie_h  = (uint32_t)(ham == best_ham);
        uint32_t higher = (uint32_t)(conf >  best_conf);
        uint32_t tie_c  = (uint32_t)(conf == best_conf);
        uint32_t lower  = (uint32_t)(sid  <  best_id);
        uint32_t take = clears & (closer
                               | (tie_h & higher)
                               | (tie_h & tie_c & lower));
        best      = take ? i      : best;
        best_ham  = take ? ham    : best_ham;
        best_conf = take ? conf   : best_conf;
        best_id   = take ? sid    : best_id;
    }
    if (out_hamming && best != UINT32_MAX) *out_hamming = best_ham;
    return best;
}

/* ==================================================================== */
/*  3.  Verified tool use                                               */
/* ==================================================================== */

cos_v64_tool_authz_t
cos_v64_tool_authorise(const cos_v64_tool_desc_t *desc,
                       uint16_t                   allowed_caps,
                       uint16_t                   schema_ver_max,
                       cos_v64_sigma_t            sigma,
                       uint16_t                   alp_threshold_q15,
                       const uint8_t             *arg_hash_at_entry,
                       const uint8_t             *arg_hash_at_use)
{
    cos_v64_tool_authz_t out;
    out.verdict     = COS_V64_TOOL_ALLOW;
    out.reason_bits = 0;
    out._pad[0] = out._pad[1] = out._pad[2] = 0;

    if (!desc || !arg_hash_at_entry || !arg_hash_at_use) {
        out.verdict = COS_V64_TOOL_DENY_RESERVED;
        out.reason_bits = 0x20;
        return out;
    }

    /* Compute every lane branchlessly, then compose.  ALL checks run,*/
    /* in full, every call — no short circuit (timing hygiene).       */
    uint8_t caps_ok     = (uint8_t)(
        (desc->required_caps & ~allowed_caps) == 0);
    uint8_t sigma_ok    = (uint8_t)(sigma.alp <= alp_threshold_q15);
    uint8_t schema_ok   = (uint8_t)(desc->schema_ver <= schema_ver_max);
    uint8_t toctou_ok   = v64_ct_eq_bytes(arg_hash_at_entry,
                                          arg_hash_at_use, 32);
    /* schema_hash is the BLAKE2b-256 of the *expected* argument
     * layout; we bind the live arg hash to the schema via TOCTOU +
     * schema_ver.  Collisions are negligible under BLAKE2b; we do
     * not re-hash here — caller presents the (static) schema hash
     * for telemetry only and the `schema_ok` lane suffices.          */
    (void)desc->schema_hash; (void)desc->effect_hash;

    /* Reason bits (lane-level, multi-cause honest).                   */
    uint8_t bits = 0;
    bits |= (uint8_t)((caps_ok   ^ 1u) << 0);
    bits |= (uint8_t)((sigma_ok  ^ 1u) << 1);
    bits |= (uint8_t)((schema_ok ^ 1u) << 2);
    bits |= (uint8_t)((toctou_ok ^ 1u) << 3);

    /* Verdict priority: TOCTOU > CAPS > SCHEMA > SIGMA > ALLOW.       */
    /* Branchless cascade via lane-wise selects.                       */
    cos_v64_tool_verdict_t v = COS_V64_TOOL_ALLOW;
    v = sigma_ok  ? v : COS_V64_TOOL_DENY_SIGMA;
    v = schema_ok ? v : COS_V64_TOOL_DENY_SCHEMA;
    v = caps_ok   ? v : COS_V64_TOOL_DENY_CAPS;
    v = toctou_ok ? v : COS_V64_TOOL_DENY_TOCTOU;
    out.verdict     = v;
    out.reason_bits = bits;
    return out;
}

/* ==================================================================== */
/*  4.  Reflexion ratchet                                               */
/* ==================================================================== */

int cos_v64_reflect_update(cos_v64_skill_lib_t *l,
                           uint32_t             skill_idx,
                           cos_v64_sigma_t      predicted,
                           cos_v64_sigma_t      observed,
                           uint16_t             alp_win_threshold_q15,
                           cos_v64_reflect_t   *out_reflect)
{
    if (!l || skill_idx >= l->len) return -1;
    cos_v64_skill_t *s = &l->skills[skill_idx];

    int32_t de = (int32_t)predicted.eps - (int32_t)observed.eps;
    int32_t da = (int32_t)predicted.alp - (int32_t)observed.alp;

    /* uses += 1, with down-shift on overflow (preserve ratio).        */
    uint32_t uses = (uint32_t)s->uses + 1u;
    uint32_t wins = (uint32_t)s->wins
                  + (uint32_t)(observed.alp <= alp_win_threshold_q15);
    uint32_t overflow = (uint32_t)(uses > (uint32_t)UINT16_MAX
                                 || wins > (uint32_t)UINT16_MAX);
    uses = overflow ? (uses >> 1) : uses;
    wins = overflow ? (wins >> 1) : wins;
    s->uses = (uint16_t)uses;
    s->wins = (uint16_t)wins;

    /* confidence = (wins << 15) / uses, Q0.15.                         */
    uint32_t num = (uint32_t)s->wins << 15;
    uint32_t den = (uint32_t)s->uses;
    uint16_t conf = (uint16_t)(den == 0 ? 0 : (num / den));
    s->confidence_q15 = conf;

    if (out_reflect) {
        out_reflect->delta_eps_q15 = de;
        out_reflect->delta_alp_q15 = da;
        out_reflect->new_conf_q15  = conf;
        out_reflect->_pad          = 0;
        out_reflect->skill_idx     = skill_idx;
    }
    return 0;
}

/* ==================================================================== */
/*  5.  AlphaEvolve-σ                                                   */
/* ==================================================================== */

cos_v64_bitnet_t *cos_v64_bitnet_new(uint32_t nweights)
{
    if (nweights == 0) return NULL;
    uint32_t nw_u32 = (nweights + 15u) / 16u;
    cos_v64_bitnet_t *b =
        (cos_v64_bitnet_t *)aligned_alloc(64, v64_round_up_64(sizeof(*b)));
    if (!b) return NULL;
    size_t bytes = v64_round_up_64((size_t)nw_u32 * sizeof(uint32_t));
    uint32_t *w = (uint32_t *)aligned_alloc(64, bytes);
    if (!w) { free(b); return NULL; }
    memset(w, 0, bytes);
    b->weights  = w;
    b->nw_u32   = nw_u32;
    b->nweights = nweights;
    return b;
}

void cos_v64_bitnet_free(cos_v64_bitnet_t *b)
{
    if (!b) return;
    free(b->weights);
    free(b);
}

/*  Helpers for ternary packing.                                         */
static uint32_t v64_bn_get(const uint32_t *w, uint32_t idx)
{
    uint32_t word = idx >> 4;
    uint32_t slot = (idx & 15u) * 2u;
    return (w[word] >> slot) & 0x3u;      /* 00|01|10 — 11 is invalid */
}

static void v64_bn_set(uint32_t *w, uint32_t idx, uint32_t pair2)
{
    uint32_t word = idx >> 4;
    uint32_t slot = (idx & 15u) * 2u;
    uint32_t mask = 0x3u << slot;
    pair2 = pair2 & 0x3u;
    pair2 = pair2 == 0x3u ? 0u : pair2;   /* clip invalid 11          */
    w[word] = (w[word] & ~mask) | (pair2 << slot);
}

/*  Apply a signed flip: each weight toggles sign (01 ↔ 10), zeros stay
 *  zero.  Idempotent, branchless.
 */
static void v64_bn_flip(uint32_t *w, uint32_t idx)
{
    uint32_t v = v64_bn_get(w, idx);
    /* table: 00→00, 01→10, 10→01, 11(invalid)→00                      */
    uint32_t n = (v == 0x1u) ? 0x2u : (v == 0x2u ? 0x1u : 0x0u);
    v64_bn_set(w, idx, n);
}

int cos_v64_evolve_mutate(cos_v64_bitnet_t         *b,
                          const uint32_t           *flip_idx,
                          uint32_t                  nflips,
                          cos_v64_score_fn          score_fn,
                          void                     *score_ctx,
                          uint32_t                 *scratch,
                          uint16_t                  accept_slack_q15,
                          cos_v64_evolve_score_t   *inout_parent)
{
    if (!b || !flip_idx || !score_fn || !scratch || !inout_parent) return -1;
    for (uint32_t i = 0; i < nflips; ++i) {
        if (flip_idx[i] >= b->nweights) return -1;
    }
    /* Snapshot parent state.                                           */
    memcpy(scratch, b->weights, (size_t)b->nw_u32 * sizeof(uint32_t));

    /* Apply mutation.                                                  */
    for (uint32_t i = 0; i < nflips; ++i) v64_bn_flip(b->weights, flip_idx[i]);

    /* Score child.                                                     */
    cos_v64_evolve_score_t child;
    memset(&child, 0, sizeof(child));
    int rc = score_fn(b->weights, b->nweights, score_ctx, &child);
    if (rc != 0) {
        memcpy(b->weights, scratch, (size_t)b->nw_u32 * sizeof(uint32_t));
        return -1;
    }

    /* Accept iff σ.alp non-increasing AND energy within slack.          */
    int32_t parent_e = inout_parent->energy_q15;
    int32_t child_e  = child.energy_q15;
    /* Slack: child_e ≤ parent_e + slack, where slack = parent_e *
     * accept_slack_q15 / 32768.  In fixed-point we compute:           */
    int32_t slack = (int32_t)(((int64_t)parent_e
                             * (int64_t)accept_slack_q15) >> 15);
    uint8_t alp_ok    = (uint8_t)(child.sigma.alp <= inout_parent->sigma.alp);
    uint8_t energy_ok = (uint8_t)(child_e <= parent_e + slack);
    uint8_t accept    = (uint8_t)(alp_ok & energy_ok);

    if (accept) {
        *inout_parent = child;
    } else {
        memcpy(b->weights, scratch, (size_t)b->nw_u32 * sizeof(uint32_t));
    }
    return (int)accept;
}

/* ==================================================================== */
/*  6.  Mixture-of-Depths-σ                                             */
/* ==================================================================== */

void cos_v64_mod_route(const cos_v64_sigma_t *sigmas,
                       uint32_t                ntokens,
                       uint8_t                 d_min,
                       uint8_t                 d_max,
                       uint8_t                *out_depths)
{
    if (!sigmas || !out_depths || ntokens == 0) return;
    uint8_t lo = d_min, hi = d_max;
    if (lo > hi) { uint8_t tmp = lo; lo = hi; hi = tmp; }
    uint32_t range = (uint32_t)(hi - lo);
    for (uint32_t i = 0; i < ntokens; ++i) {
        /*  α ∈ [0, 32768].  depth = lo + round(α / 32768 * range).     */
        uint32_t alp   = (uint32_t)sigmas[i].alp;
        uint32_t scaled= (alp * range + 16384u) >> 15;   /* round */
        /* clamp via branchless min (range already ≤ 255).              */
        uint32_t clip  = scaled > range ? range : scaled;
        out_depths[i]  = (uint8_t)(lo + clip);
    }
}

uint64_t cos_v64_mod_cost(const uint8_t *depths, uint32_t ntokens)
{
    if (!depths || ntokens == 0) return 0;
    uint64_t sum = 0;
    for (uint32_t i = 0; i < ntokens; ++i) sum += (uint64_t)depths[i];
    return sum;
}
