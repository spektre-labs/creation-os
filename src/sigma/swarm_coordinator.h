/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * σ-coordinated swarm: HTTP fan-out to peer Creation OS endpoints,
 * merge votes by lowest σ / majority / weighted blend.
 */

#ifndef COS_SWARM_COORDINATOR_H
#define COS_SWARM_COORDINATOR_H

#include "error_attribution.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COS_SWARM_MAX_VOTES
#define COS_SWARM_MAX_VOTES 8
#endif

#ifndef COS_SWARM_MAX_PEERS_COORD
#define COS_SWARM_MAX_PEERS_COORD 16
#endif

enum cos_swarm_vote_strategy {
    COS_SWARM_VOTE_LOWEST_SIGMA = 0,
    COS_SWARM_VOTE_MAJORITY,
    COS_SWARM_VOTE_SIGMA_WEIGHTED,
};

struct cos_swarm_peer {
    char   peer_id[64];
    char   endpoint[256]; /* HOST:PORT or scheme://HOST:PORT */
    float  sigma_mean;
    float  trust;
    int    available;
    int64_t last_seen_ms;
    int    responses;
    int    best_count;
};

struct cos_swarm_vote {
    char peer_id[64];
    char response[4096];
    float sigma;
    enum cos_error_source attribution;
    int64_t latency_ms;
};

struct cos_swarm_result {
    struct cos_swarm_vote votes[COS_SWARM_MAX_VOTES];
    int                n_votes;
    char               best_response[4096];
    float              sigma_best;
    float              sigma_consensus;
    int                consensus;
    char               method[32]; /* lowest_sigma | majority_vote | weighted */
};

int cos_swarm_coord_init(void);
void cos_swarm_coord_shutdown(void);

int cos_swarm_coord_add_peer(const struct cos_swarm_peer *peer);
int cos_swarm_coord_remove_peer(const char *peer_id);

/** Fan-out HTTP POST /cos/swarm/query on each peer; fills result. */
int cos_swarm_broadcast(const char *prompt, enum cos_swarm_vote_strategy strat,
                        struct cos_swarm_result *result);

int cos_swarm_vote_lowest_sigma(struct cos_swarm_vote *votes, int n,
                                 struct cos_swarm_result *result);
int cos_swarm_vote_majority(struct cos_swarm_vote *votes, int n,
                              struct cos_swarm_result *result);
int cos_swarm_vote_sigma_weighted(struct cos_swarm_vote *votes, int n,
                                  struct cos_swarm_result *result);

int cos_swarm_update_trust(const char *peer_id, float sigma, int was_best);

int cos_swarm_coord_peers_list(struct cos_swarm_peer *peers, int max, int *n);

/** Ω-loop hook: if peers configured and swarm improves σ, overwrite think result. */
struct cos_think_result;
int cos_swarm_omega_rescue(const char *goal, struct cos_think_result *tr);

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_SWARM_COORD_TEST)
int cos_swarm_coord_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* COS_SWARM_COORDINATOR_H */
