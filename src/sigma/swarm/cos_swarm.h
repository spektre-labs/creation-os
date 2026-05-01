/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-swarm routing (HORIZON-4).
 *
 * Broadcast a prompt to N peers; each returns a scalar σ for that
 * prompt.  The orchestrator picks argmin σ (lowest uncertainty).  If
 * every σ exceeds τ_abstain the swarm abstains jointly.  After a peer
 * answers, its trust_ema is nudged toward the observed σ with the
 * same EMA rule as σ-A2A: trust ← trust + lr·(σ − trust), clipped to
 * [0,1].
 *
 * v0 does not open sockets — the caller supplies per-peer σ (from a
 * live HTTP probe in a future revision, or from the mock harness).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SWARM_H
#define COS_SIGMA_SWARM_H

#ifdef __cplusplus
extern "C" {
#endif

#define COS_SWARM_MAX_PEERS 16

typedef struct {
    char  peer_id[64];
    char  endpoint[256];
    float sigma_mean;   /* rolling summary — updated by orchestrator */
    float trust_ema;
    char  specialty[64];
} cos_swarm_peer_t;

typedef struct {
    cos_swarm_peer_t peers[COS_SWARM_MAX_PEERS];
    int   n_peers;
    float lr;           /* trust EMA step, default 0.20 */
    float tau_abstain;  /* abstain if min σ > this, default 0.90 */
} cos_swarm_net_t;

void cos_swarm_init(cos_swarm_net_t *n, float lr, float tau_abstain);

int cos_swarm_register(cos_swarm_net_t *n, const char *peer_id,
                       const char *endpoint, const char *specialty);

/* Route: `sigmas[i]` aligns with `n->peers[i]`.  Returns chosen index
 * or -1 if abstain.  Writes min σ to *out_min_sigma when not abstain. */
int cos_swarm_route(const cos_swarm_net_t *n, const float *sigmas, int n_sig,
                    float *out_min_sigma);

/* Update trust for the peer that actually responded (chosen_idx). */
void cos_swarm_trust_update(cos_swarm_net_t *n, int chosen_idx,
                            float response_sigma);

/* Deterministic uint32 hash for mock σ perturbations. */
unsigned cos_swarm_prompt_hash(const char *prompt);

/* Mock σ for peer j given prompt (deterministic).  Bases must keep a
 * strict total order so tests can pin the winner. */
float cos_swarm_mock_sigma(int peer_index, const char *prompt);

/* Harness: 3 peers, 10 prompts, winner index must always match. */
int cos_swarm_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SWARM_H */
