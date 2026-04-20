/*
 * σ-mesh3 — three-node live-mesh simulation.
 *
 * In-process model of the Tailscale-topology Creation OS mesh:
 *
 *     A  (M3 Air, Helsinki)      — coordinator, holds the τ budget
 *     B  (RPi5, Kerava)          — leaf, answers cheap queries,
 *                                   escalates when local σ is high
 *     C  (Linux VM, cloud)       — federator, holds the DP aggregate
 *
 * Every message rides a deterministic envelope:
 *     from, to, kind, payload, signature (Ed25519 over the
 *     canonical envelope; per-node keypairs are derived from
 *     fixed 32-byte seeds so the trace is reproducible across
 *     hosts).  Verify() rejects tampered bytes, wrong signer,
 *     and unknown keys.  Same contract as the WAN path in
 *     cos-mesh-node; no shared-secret MAC anywhere.
 *
 * The kernel runs the canonical script:
 *   1.  A → B  query           (σ measured locally at B)
 *   2.  B → A  answer          (if σ_local > τ_escalate: no answer, ESCALATE)
 *   3.  A → C  escalate        (on B's ESCALATE)
 *   4.  C → A  answer_final
 *   5.  C → {A, B} federation  (DP-noised weight delta)
 *
 * It produces a reproducible trace CI can diff.  The real
 * `cos network` CLI runs the same protocol over sockets; when no
 * network is available (or the runner is offline), `make
 * check-sigma-mesh3` runs exactly this in-process simulator so
 * the invariant gets exercised on every merge.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_MESH3_H
#define COS_SIGMA_MESH3_H

#include <stddef.h>
#include <stdint.h>

#include "protocol.h"   /* Ed25519 keypair + signature sizes */

#ifdef __cplusplus
extern "C" {
#endif

#define COS_MESH3_NODES              3
#define COS_MESH3_NAME_MAX          24
#define COS_MESH3_PAYLOAD_MAX      192
#define COS_MESH3_MAX_MESSAGES      32
#define COS_MESH3_DP_DIM             8
#define COS_MESH3_SIG_LEN           COS_ED25519_SIG_LEN

enum cos_mesh3_kind {
    COS_MESH3_QUERY        = 1,
    COS_MESH3_ANSWER       = 2,
    COS_MESH3_ESCALATE     = 3,
    COS_MESH3_ANSWER_FINAL = 4,
    COS_MESH3_FEDERATION   = 5,
};

typedef struct {
    int         from;       /* index into mesh.nodes */
    int         to;
    int         kind;
    char        payload[COS_MESH3_PAYLOAD_MAX];
    float       sigma;
    uint8_t     signature[COS_MESH3_SIG_LEN];
    int         verified;
    int         dropped;    /* dropped because signature failed  */
} cos_mesh3_msg_t;

typedef struct {
    char     name[COS_MESH3_NAME_MAX];
    char     role[COS_MESH3_NAME_MAX];
    /* Ed25519 keypair derived from a fixed per-node seed.  sign_key
     * = priv(64) || pub(32), verify_key = pub(32). */
    uint8_t  sign_key[COS_ED25519_PRIV_LEN + COS_ED25519_PUB_LEN];
    uint8_t  verify_key[COS_ED25519_PUB_LEN];
    float    local_sigma_mean;
    int      queries_seen;
    int      answers_given;
    int      escalated_out;
    int      federation_updates_applied;
    float    federation_weight[COS_MESH3_DP_DIM]; /* applied weights */
} cos_mesh3_node_t;

typedef struct {
    cos_mesh3_node_t  nodes[COS_MESH3_NODES];
    cos_mesh3_msg_t   log[COS_MESH3_MAX_MESSAGES];
    int               n_messages;
    float             tau_escalate;    /* leaf escalates above this */
    float             dp_noise_sigma;  /* DP noise stddev (mock)    */
    int               script_ok;       /* 1 iff full script executed */
    int               escalations;
    int               federations;
    int               signatures_rejected;
} cos_mesh3_mesh_t;

/* ==================================================================
 * Lifecycle
 * ================================================================== */
void cos_mesh3_init(cos_mesh3_mesh_t *mesh);

/* Run the canonical five-step script for a single `query` string.
 * Returns the final σ reported to A from either B (answer) or C
 * (answer_final after escalate).  On any signature failure the
 * offending message is recorded with dropped=1 and the script is
 * aborted with script_ok=0. */
int cos_mesh3_run_query(cos_mesh3_mesh_t *mesh, const char *query,
                        float *out_final_sigma);

/* Exercise the tamper-detection branch: a single message whose
 * signature is flipped.  Returns 1 if the mesh correctly refused
 * the bad message (signatures_rejected bumps), 0 otherwise. */
int cos_mesh3_tamper_probe(cos_mesh3_mesh_t *mesh);

int cos_mesh3_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_MESH3_H */
