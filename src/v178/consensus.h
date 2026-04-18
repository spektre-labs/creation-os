/*
 * v178 σ-Consensus — Byzantine-tolerant mesh truth without an
 * authority.
 *
 *   Given a 5-node mesh with up to f = 1 malicious nodes,
 *   v178 executes one round of σ-Byzantine agreement:
 *
 *     round(q):
 *       each honest node i emits σ_i for claim q.
 *       each byzantine node may emit an arbitrary σ_i.
 *       reputation-weighted vote:
 *         if 2/3+ weight agrees "true" (σ < θ) → ACCEPT
 *         if 2/3+ weight agrees "false"       → REJECT
 *         otherwise                            → ABSTAIN
 *       update reputations: accepted-with-majority → +1,
 *         against-majority → −1 (sybil-resistance: new
 *         nodes have weight 1 and cannot saturate).
 *     append σ-merkle leaf → rebuild root.
 *
 *   "Resonance not consensus": no single node picks truth.
 *   Truth = convergence of σ signals above quorum; mesh
 *   abstains when it cannot converge.
 *
 * v178.0 (this file) ships deterministic fixtures for:
 *   - 5 nodes: N1..N3 honest, N4 young (sybil weight = 1),
 *     N5 byzantine (flips for every claim)
 *   - 4 claims with baked honest-σ ensuring 3 accepts,
 *     1 abstain (split → no-quorum)
 *   - reputation init 3.0 (mature) / 1.0 (young) / 0.5 (suspect)
 *   - SHA-256 merkle tree over σ-decision leaves
 *
 * v178.1 (named, not shipped):
 *   - live mesh (v128) socket transport
 *   - real signed messages
 *   - streaming merkle root anchoring to v72
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V178_CONSENSUS_H
#define COS_V178_CONSENSUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V178_N_NODES   5
#define COS_V178_N_CLAIMS  4
#define COS_V178_HASH_LEN  32
#define COS_V178_MAX_STR   96

typedef enum {
    COS_V178_DEC_ACCEPT  = 0,
    COS_V178_DEC_REJECT  = 1,
    COS_V178_DEC_ABSTAIN = 2,
} cos_v178_decision_t;

typedef struct {
    int   id;
    char  name[COS_V178_MAX_STR];
    bool  byzantine;
    float reputation;
    int   n_correct;
    int   n_incorrect;
} cos_v178_node_t;

typedef struct {
    int                 claim_id;
    char                label[COS_V178_MAX_STR];
    float               sigma[COS_V178_N_NODES];   /* per-node σ */
    bool                vote_true[COS_V178_N_NODES]; /* σ < theta */
    float               weight_true;
    float               weight_false;
    cos_v178_decision_t decision;
    uint8_t             leaf_hash[COS_V178_HASH_LEN];
} cos_v178_claim_t;

typedef struct {
    float               theta_sigma;      /* claim=true iff σ < this */
    float               quorum;           /* weight fraction, default 2/3 */
    float               rep_newbie;       /* default 1.0 */
    float               rep_mature;       /* default 3.0 */
    float               rep_suspect;      /* default 0.5 */

    cos_v178_node_t     nodes[COS_V178_N_NODES];
    int                 n_claims;
    cos_v178_claim_t    claims[COS_V178_N_CLAIMS];
    uint8_t             merkle_root[COS_V178_HASH_LEN];

    int                 n_accept;
    int                 n_reject;
    int                 n_abstain;

    uint64_t            seed;
} cos_v178_state_t;

void cos_v178_init(cos_v178_state_t *s, uint64_t seed);

/* Load the baked fixture + execute the full agreement round. */
void cos_v178_run(cos_v178_state_t *s);

int  cos_v178_verify_merkle(const cos_v178_state_t *s);
void cos_v178_merkle_root_hex(const cos_v178_state_t *s,
                              char out[2 * COS_V178_HASH_LEN + 1]);

size_t cos_v178_to_json(const cos_v178_state_t *s, char *buf, size_t cap);

int cos_v178_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V178_CONSENSUS_H */
