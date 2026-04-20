/* σ-Mesh primitive — peer registry + σ-weighted routing. */

#include "mesh.h"

#include <string.h>

static float clamp01f(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

int cos_sigma_mesh_init(cos_mesh_t *m,
                        cos_mesh_node_t *storage, int cap,
                        float w_sigma, float w_latency,
                        float latency_norm_ms,
                        float tau_abstain,
                        float ewma_alpha)
{
    if (!m || !storage || cap <= 0) return -1;
    if (!(w_sigma  >= 0.0f && w_sigma  <= 1.0f)) return -2;
    if (!(w_latency >= 0.0f && w_latency <= 1.0f)) return -3;
    if (!(latency_norm_ms > 0.0f))                 return -4;
    if (!(tau_abstain > 0.0f && tau_abstain <= 1.0f)) return -5;
    if (!(ewma_alpha >= 0.0f && ewma_alpha <= 1.0f))  return -6;
    memset(m, 0, sizeof *m);
    m->slots            = storage;
    m->cap              = cap;
    m->w_sigma          = w_sigma;
    m->w_latency        = w_latency;
    m->latency_norm_ms  = latency_norm_ms;
    m->tau_abstain      = tau_abstain;
    m->ewma_alpha       = ewma_alpha;
    memset(storage, 0, sizeof(cos_mesh_node_t) * (size_t)cap);
    return 0;
}

int cos_sigma_mesh_find(const cos_mesh_t *m, const char *id, int *out_idx) {
    if (!m || !id) return -1;
    for (int i = 0; i < m->count; ++i) {
        if (strncmp(m->slots[i].node_id, id, COS_MESH_ID_CAP - 1) == 0) {
            if (out_idx) *out_idx = i;
            return 0;
        }
    }
    return -2;
}

int cos_sigma_mesh_register(cos_mesh_t *m,
                            const char *id, const char *address,
                            int is_self,
                            float initial_sigma,
                            float initial_latency_ms)
{
    if (!m || !id || !address || id[0] == '\0' || address[0] == '\0')
        return -1;
    if (m->count >= m->cap) return -2;
    if (cos_sigma_mesh_find(m, id, NULL) == 0) return -3;
    cos_mesh_node_t *n = &m->slots[m->count++];
    memset(n, 0, sizeof *n);
    strncpy(n->node_id, id, COS_MESH_ID_CAP - 1);
    strncpy(n->address, address, COS_MESH_ADDR_CAP - 1);
    n->is_self          = is_self ? 1 : 0;
    n->sigma_capability = clamp01f(initial_sigma);
    n->latency_ms       = initial_latency_ms < 0.0f ? 0.0f : initial_latency_ms;
    n->available        = 1;
    return 0;
}

int cos_sigma_mesh_set_available(cos_mesh_t *m, const char *id, int av) {
    int idx = -1;
    if (cos_sigma_mesh_find(m, id, &idx) != 0) return -1;
    m->slots[idx].available = av ? 1 : 0;
    return 0;
}

static float score_node(const cos_mesh_t *m, const cos_mesh_node_t *n) {
    float lat_norm = n->latency_ms / m->latency_norm_ms;
    if (lat_norm < 0.0f) lat_norm = 0.0f;
    /* Latency term is normalised; saturate at 2× latency budget
     * so a comically slow peer stops dominating. */
    if (lat_norm > 2.0f) lat_norm = 2.0f;
    return m->w_sigma * n->sigma_capability + m->w_latency * lat_norm;
}

const cos_mesh_node_t *cos_sigma_mesh_route(const cos_mesh_t *m,
                                            float sigma_local,
                                            float tau_local,
                                            int   allow_self)
{
    if (!m || m->count == 0) return NULL;
    const cos_mesh_node_t *best = NULL;
    float                  best_score = 1e9f;

    for (int i = 0; i < m->count; ++i) {
        const cos_mesh_node_t *n = &m->slots[i];
        if (!n->available) continue;
        if (n->is_self && !allow_self) continue;
        if (n->sigma_capability >= m->tau_abstain && !n->is_self) continue;
        float score = score_node(m, n);
        /* Self-node preference when local σ already clears the bar:
         * subtract a small bonus so a reasonable remote still wins
         * if the local σ is bad. */
        if (n->is_self && sigma_local < tau_local) {
            score -= 0.05f;
        }
        if (score < best_score) {
            best_score = score;
            best       = n;
        }
    }
    /* If the winner is a remote but its σ_capability is over
     * tau_abstain, the earlier filter already excluded it — but
     * re-check for the self case: self with σ_local ≥ tau_abstain
     * must not win either. */
    if (best && best->is_self && sigma_local >= m->tau_abstain) {
        return NULL;
    }
    return best;
}

int cos_sigma_mesh_report(cos_mesh_t *m, const char *id,
                          float sigma_result, float latency_ms,
                          double cost_eur, int success)
{
    int idx = -1;
    if (cos_sigma_mesh_find(m, id, &idx) != 0) return -1;
    cos_mesh_node_t *n = &m->slots[idx];
    float a = m->ewma_alpha;
    n->sigma_capability = clamp01f(a * n->sigma_capability
                                   + (1.0f - a) * clamp01f(sigma_result));
    if (latency_ms >= 0.0f) {
        n->latency_ms = a * n->latency_ms + (1.0f - a) * latency_ms;
    }
    if (cost_eur > 0.0) n->total_cost_eur += cost_eur;
    if (success) n->n_successes++; else n->n_failures++;
    return 0;
}

/* ---------- self-test ---------- */

static float fabsf5(float x) { return x < 0.0f ? -x : x; }

static int check_guards(void) {
    cos_mesh_t m;
    cos_mesh_node_t slots[2];
    if (cos_sigma_mesh_init(NULL,  slots, 2, 0.5f, 0.5f, 50.0f, 0.8f, 0.8f) == 0)
        return 10;
    if (cos_sigma_mesh_init(&m,    slots, 2, 1.5f, 0.5f, 50.0f, 0.8f, 0.8f) == 0)
        return 11;
    if (cos_sigma_mesh_init(&m,    slots, 2, 0.5f, 0.5f, 0.0f,  0.8f, 0.8f) == 0)
        return 12;
    if (cos_sigma_mesh_init(&m,    slots, 2, 0.5f, 0.5f, 50.0f, 0.0f, 0.8f) == 0)
        return 13;
    if (cos_sigma_mesh_init(&m,    slots, 2, 0.5f, 0.5f, 50.0f, 0.8f, 2.0f) == 0)
        return 14;
    return 0;
}

static int check_register_and_duplicate(void) {
    cos_mesh_t m;
    cos_mesh_node_t slots[3];
    cos_sigma_mesh_init(&m, slots, 3, 0.6f, 0.4f, 50.0f, 0.80f, 0.70f);
    if (cos_sigma_mesh_register(&m, "self", "127.0.0.1:0",
                                1, 0.2f, 0.0f) != 0)   return 20;
    if (cos_sigma_mesh_register(&m, "peer-a", "10.0.0.2:7000",
                                0, 0.5f, 30.0f) != 0)  return 21;
    if (cos_sigma_mesh_register(&m, "peer-a", "10.0.0.3:7000",
                                0, 0.5f, 30.0f) != -3) return 22;
    if (m.count != 2)                                   return 23;
    return 0;
}

static int check_routing(void) {
    cos_mesh_t m;
    cos_mesh_node_t slots[4];
    cos_sigma_mesh_init(&m, slots, 4, 0.70f, 0.30f, 100.0f, 0.80f, 0.80f);
    cos_sigma_mesh_register(&m, "self",   "local",      1, 0.30f,  0.0f);
    cos_sigma_mesh_register(&m, "strong", "peer-1:7000",0, 0.10f, 40.0f);
    cos_sigma_mesh_register(&m, "slow",   "peer-2:7000",0, 0.15f, 300.0f);
    cos_sigma_mesh_register(&m, "bad",    "peer-3:7000",0, 0.90f, 20.0f);

    /* [1] Local σ high (0.90) → must route to "strong" remote. */
    const cos_mesh_node_t *r = cos_sigma_mesh_route(&m, 0.90f, 0.30f, 0);
    if (!r)                                              return 30;
    if (strcmp(r->node_id, "strong") != 0)                return 31;

    /* [2] Local σ low (0.10) + allow_self → self wins. */
    r = cos_sigma_mesh_route(&m, 0.10f, 0.30f, 1);
    if (!r)                                              return 32;
    if (strcmp(r->node_id, "self") != 0)                  return 33;

    /* [3] Mark strong + self unavailable, bad quarantined by σ ≥
     * tau_abstain, slow still ok → slow wins. */
    cos_sigma_mesh_set_available(&m, "strong", 0);
    cos_sigma_mesh_set_available(&m, "self",   0);
    r = cos_sigma_mesh_route(&m, 0.90f, 0.30f, 0);
    if (!r)                                              return 34;
    if (strcmp(r->node_id, "slow") != 0)                  return 35;

    /* [4] Take slow down too → bad is over abstain floor → NULL. */
    cos_sigma_mesh_set_available(&m, "slow", 0);
    r = cos_sigma_mesh_route(&m, 0.90f, 0.30f, 0);
    if (r != NULL)                                        return 36;
    return 0;
}

static int check_reputation(void) {
    cos_mesh_t m;
    cos_mesh_node_t slots[2];
    cos_sigma_mesh_init(&m, slots, 2, 0.50f, 0.50f, 50.0f, 0.80f, 0.70f);
    cos_sigma_mesh_register(&m, "peer", "x", 0, 0.50f, 100.0f);

    /* Three successful interactions pull σ down; one is enough
     * to verify direction under α=0.70. */
    cos_sigma_mesh_report(&m, "peer", 0.05f, 80.0f, 0.001, 1);
    int idx = -1;
    cos_sigma_mesh_find(&m, "peer", &idx);
    if (!(m.slots[idx].sigma_capability < 0.50f))        return 40;
    if (!(m.slots[idx].latency_ms < 100.0f))              return 41;
    if (m.slots[idx].n_successes != 1)                    return 42;
    if (m.slots[idx].total_cost_eur <= 0.0)               return 43;

    /* Malicious burst: σ_result = 1.0 for three rounds pushes
     * σ_capability toward 1.0 fast. */
    for (int i = 0; i < 5; ++i) {
        cos_sigma_mesh_report(&m, "peer", 1.0f, 120.0f, 0.0, 0);
    }
    if (!(m.slots[idx].sigma_capability > 0.80f))        return 44;
    if (m.slots[idx].n_failures != 5)                     return 45;

    /* Route must now refuse this peer. */
    const cos_mesh_node_t *r = cos_sigma_mesh_route(&m, 0.90f, 0.30f, 0);
    if (r != NULL)                                        return 46;
    return 0;
}

static int check_ewma_math(void) {
    /* Verify the EWMA formula: σ' = α σ + (1−α) σ_result */
    cos_mesh_t m;
    cos_mesh_node_t slots[1];
    cos_sigma_mesh_init(&m, slots, 1, 0.5f, 0.5f, 50.0f, 0.90f, 0.80f);
    cos_sigma_mesh_register(&m, "x", "a", 0, 0.50f, 100.0f);
    cos_sigma_mesh_report  (&m, "x", 0.00f, 100.0f, 0.0, 1);
    int idx; cos_sigma_mesh_find(&m, "x", &idx);
    /* 0.80·0.50 + 0.20·0.00 = 0.40 */
    if (fabsf5(m.slots[idx].sigma_capability - 0.40f) > 1e-5f) return 50;
    return 0;
}

int cos_sigma_mesh_self_test(void) {
    int rc;
    if ((rc = check_guards()))                 return rc;
    if ((rc = check_register_and_duplicate())) return rc;
    if ((rc = check_routing()))                return rc;
    if ((rc = check_reputation()))             return rc;
    if ((rc = check_ewma_math()))              return rc;
    return 0;
}
