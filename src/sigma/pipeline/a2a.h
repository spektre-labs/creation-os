/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-A2A — agent-to-agent protocol with σ-trust.
 *
 * Google's Agent2Agent (A2A) standard lets agents discover each
 * other via an agent card at /.well-known/agent.json and exchange
 * capability-gated task requests.  σ-A2A is Creation OS's C-kernel
 * implementation of the protocol semantics with one addition:
 * every peer carries a σ_trust score that moves on each exchange.
 *
 *   Fresh peer                    → σ_trust = 0.50  (neutral)
 *   Good answer (σ_response low)  → σ_trust decreases (trusted)
 *   Bad answer (σ_response high)  → σ_trust increases (suspect)
 *   σ_trust > τ_block             → peer is blocklisted
 *
 * This is the peer-level analogue of the node-level mesh sentiment
 * logic in D1.  It is transport-agnostic: real deployments run A2A
 * over HTTPS + Ed25519; the kernel here runs the state machine in
 * process so CI can pin every verdict deterministically.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_A2A_H
#define COS_SIGMA_A2A_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_A2A_ID_MAX               48
#define COS_A2A_CAP_MAX              32
#define COS_A2A_CAPS_PER_PEER         8
#define COS_A2A_MAX_PEERS            32
#define COS_A2A_URL_MAX             128

#define COS_A2A_TRUST_INIT           0.50f
#define COS_A2A_LR                   0.20f   /* EMA learning rate */
#define COS_A2A_TAU_BLOCK            0.90f
#define COS_A2A_TAU_WARN             0.70f

#define COS_A2A_Q_PENDING            0
#define COS_A2A_Q_APPROVED           1
#define COS_A2A_Q_REJECTED           2

#define COS_A2A_QUARANTINE_CAP       64

#define COS_A2A_TAU_CONTENT_DEFAULT  0.65f
#define COS_A2A_TAU_TRUST_CAP_DEFAULT 0.85f /* reject when peer.sigma_trust exceeds this */

enum cos_a2a_status {
    COS_A2A_OK          =  0,
    COS_A2A_ERR_ARG     = -1,
    COS_A2A_ERR_FULL    = -2,
    COS_A2A_ERR_BLOCKED = -3,
    COS_A2A_ERR_NOPEER  = -4,
    COS_A2A_ERR_CAP     = -5,
    COS_A2A_ERR_QUARANTINE = -6,
};

struct cos_a2a_quarantine_entry {
    char    sender_id[COS_A2A_ID_MAX];
    char    message[4096];
    float   sigma_content;
    float   trust_sender;
    char    reject_reason[256];
    int64_t timestamp_ms;
    int     status;
};

typedef struct {
    char  agent_id[COS_A2A_ID_MAX];
    char  agent_card_url[COS_A2A_URL_MAX];
    char  capabilities[COS_A2A_CAPS_PER_PEER][COS_A2A_CAP_MAX];
    int   n_capabilities;
    int   sigma_gate_declared;     /* 1 if peer's card advertises
                                    * sigma_gate=true               */
    int   scsl_compliant;
    float sigma_trust;              /* 0..1 (low=trusted)            */
    int   exchanges_total;
    int   exchanges_blocked;
    int   blocklisted;
} cos_a2a_peer_t;

typedef struct {
    cos_a2a_peer_t peers[COS_A2A_MAX_PEERS];
    int            n_peers;
    char           self_id[COS_A2A_ID_MAX];
    float          tau_block;
    float          lr;
} cos_a2a_network_t;

/* ==================================================================
 * Lifecycle
 * ================================================================== */
void cos_a2a_init(cos_a2a_network_t *net, const char *self_id);

/* Register (or overwrite) a peer from its agent-card fields. */
int  cos_a2a_peer_register(cos_a2a_network_t *net,
                           const char *agent_id,
                           const char *agent_card_url,
                           const char *const *capabilities,
                           int n_capabilities,
                           int sigma_gate_declared,
                           int scsl_compliant);

cos_a2a_peer_t *cos_a2a_peer_find(cos_a2a_network_t *net,
                                  const char *agent_id);

/* ==================================================================
 * Exchange
 *
 * cos_a2a_request delivers an outcome of a single A2A call:
 *    agent_id:         peer we called
 *    capability:       capability used
 *    sigma_response:   σ we measured on the reply (0..1)
 * Side-effects:
 *    - updates peer.sigma_trust via EMA (trust += lr*(σ_resp - trust))
 *    - bumps exchanges_total; if peer.sigma_trust crosses τ_block,
 *      sets blocklisted=1 and returns COS_A2A_ERR_BLOCKED on this
 *      and all subsequent calls to that peer.
 * ================================================================== */
int cos_a2a_request(cos_a2a_network_t *net,
                    const char *agent_id,
                    const char *capability,
                    float sigma_response);

/* Incoming message σ-gate + optional quarantine queue (PROCESS-local). */
int cos_a2a_incoming_message(cos_a2a_network_t *net,
                             const char *sender_id,
                             const char *message,
                             float tau_content,
                             float tau_trust_cap,
                             int *quarantined_out,
                             struct cos_a2a_quarantine_entry *detail_out_or_null);

int cos_a2a_quarantine_count(void);

int cos_a2a_quarantine_get(int index,
                             struct cos_a2a_quarantine_entry *out);

int cos_a2a_quarantine_resolve(cos_a2a_network_t *net,
                               int index,
                               int approve);

void cos_a2a_quarantine_print(FILE *fp);

void cos_a2a_init_default_taus(float *tau_content, float *tau_trust_cap);

/* Serialise the network-visible "own" agent card. */
int cos_a2a_self_card(const cos_a2a_network_t *net,
                      const char *const *capabilities,
                      int n_capabilities,
                      char *buf, size_t cap);

int cos_a2a_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_A2A_H */
