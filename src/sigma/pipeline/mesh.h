/*
 * σ-Mesh — peer-to-peer routing with σ-weighted node selection.
 *
 * Darkbloom (Apr 2026) and earlier PyTorch Distributed showed you
 * can turn a fleet of idle Apple-Silicon boxes into a single
 * inference substrate.  σ-Mesh is that substrate's router: given a
 * query the local kernel could not answer confidently (σ ≥ τ), we
 * pick a peer based on its running σ-reputation and a latency
 * estimate rather than a static weight table.
 *
 * Node registry (v0):
 *   node_id           stable identifier (caller-owned string)
 *   address           caller-opaque transport string (mDNS / IP:port)
 *   sigma_capability  EWMA over outcomes: 0.0 = perfectly reliable,
 *                     1.0 = unusable.  New peers start at 0.5.
 *   latency_ms        EWMA over recent RTTs.
 *   available         heartbeat flag; unavailable nodes are skipped.
 *   n_successes /    rolling counts feeding σ_capability updates.
 *   n_failures
 *
 * Routing score (lower is better):
 *
 *     score = w_sigma · sigma_capability
 *           + w_latency · (latency_ms / latency_norm_ms)
 *
 * where w_sigma + w_latency = 1.  Self-node gets an implicit
 * bonus (+ε) when σ_local < τ_local.  If every peer's σ_capability
 * ≥ τ_abstain, σ-Mesh returns NULL so the caller can ABSTAIN
 * rather than guessing.
 *
 * σ-reputation update (post-response):
 *
 *     σ ← clamp01( α · σ + (1−α) · σ_result )
 *
 * where σ_result is the downstream gate's reliability score for the
 * peer's answer.  Successes pull σ toward 0; failures push it toward
 * 1.  A quarantined malicious peer converges on σ ≈ 1 in a handful
 * of interactions and is never routed to again.
 *
 * Contracts (v0):
 *   1. init rejects NULL storage / cap ≤ 0.
 *   2. register rejects duplicates by node_id (−3).
 *   3. route respects `available == 0` and the τ_abstain ceiling;
 *      self-preference bonus only fires when allow_self != 0.
 *   4. report(id, σ_result, latency_ms) is the only state-mutating
 *      path from caller code and is bounded by EWMA α.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_MESH_H
#define COS_SIGMA_MESH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_MESH_ID_CAP    32
#define COS_MESH_ADDR_CAP  64

typedef struct {
    char   node_id[COS_MESH_ID_CAP];
    char   address[COS_MESH_ADDR_CAP];
    float  sigma_capability;   /* 0..1, lower = more reliable     */
    float  latency_ms;         /* EWMA                             */
    int    available;          /* 1 = heartbeat alive              */
    int    is_self;
    int    n_successes;
    int    n_failures;
    double total_cost_eur;     /* running usage cost               */
} cos_mesh_node_t;

typedef struct {
    cos_mesh_node_t *slots;
    int              cap;
    int              count;
    float            w_sigma;        /* routing weight           */
    float            w_latency;
    float            latency_norm_ms; /* normaliser              */
    float            tau_abstain;    /* refuse-to-route ceiling  */
    float            ewma_alpha;     /* σ update retention       */
} cos_mesh_t;

int   cos_sigma_mesh_init(cos_mesh_t *mesh,
                          cos_mesh_node_t *storage, int cap,
                          float w_sigma, float w_latency,
                          float latency_norm_ms,
                          float tau_abstain,
                          float ewma_alpha);

int   cos_sigma_mesh_register(cos_mesh_t *mesh,
                              const char *node_id,
                              const char *address,
                              int is_self,
                              float initial_sigma,
                              float initial_latency_ms);

int   cos_sigma_mesh_set_available(cos_mesh_t *mesh,
                                   const char *node_id, int available);

/* Pick the node with the lowest routing score.  Caller supplies
 * `sigma_local` (the local kernel's σ on this query) and
 * `tau_local`; if allow_self is set and σ_local < τ_local, the
 * self-node is preferred even when a remote has a slightly better
 * capability score.  Returns NULL if every candidate exceeds
 * `tau_abstain`. */
const cos_mesh_node_t *cos_sigma_mesh_route(const cos_mesh_t *mesh,
                                            float sigma_local,
                                            float tau_local,
                                            int   allow_self);

/* Post-response feedback: σ_result is the σ the downstream gate
 * stamped on the peer's answer.  Latency is the round-trip in ms;
 * cost_eur is added to the running tally. */
int   cos_sigma_mesh_report(cos_mesh_t *mesh,
                            const char *node_id,
                            float sigma_result,
                            float latency_ms,
                            double cost_eur,
                            int success);

int   cos_sigma_mesh_find(const cos_mesh_t *mesh,
                          const char *node_id,
                          int *out_idx);

int   cos_sigma_mesh_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_MESH_H */
