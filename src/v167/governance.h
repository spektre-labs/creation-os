/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v167 σ-Governance-API — organization-level control plane.
 *
 * Single user = single machine.  v167 turns Creation OS into
 * something an organization can govern: domain-scoped policies
 * (τ, abstain_message, require_sandbox), a fleet ledger (all
 * connected nodes + health + policy_version), an append-only
 * audit log (who asked, what the model said, what σ was,
 * whether it abstained), and RBAC (admin / user / auditor /
 * developer).
 *
 * v167.0 (this file) ships:
 *   - a baked organization "spektre-labs" with 3 domain
 *     policies (medical, creative, code).  Each policy carries:
 *     domain, τ, abstain_message (may be empty / null),
 *     require_sandbox.
 *   - a fleet of 4 nodes (lab-m3, lab-rpi5, cloud-a, cloud-b)
 *     with {health: ok|degraded|down, policy_version, adapter
 *     version}.  A policy update stamps every node with the
 *     new version; a rollback walks them back.
 *   - an append-only audit log (ring buffer, N=64) entries:
 *     {actor, role, domain, prompt, verdict (emit/abstain/
 *     revise), sigma_product, node_id, timestamp_fake}.
 *   - RBAC: 4 roles with capability sets.  A request from a
 *     role that lacks a capability is denied and *audited*
 *     with verdict = "denied".
 *   - JSON emitters: policy bundle, fleet status, audit log.
 *
 * v167.1 (named, not shipped):
 *   - real HTTP policy server (push over WebSocket → v166)
 *   - real TLS auth + GDPR / SOC2 / HIPAA export profiles
 *   - real fleet discovery via v150 swarm
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V167_GOVERNANCE_H
#define COS_V167_GOVERNANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V167_MAX_NAME         32
#define COS_V167_MAX_MSG         160
#define COS_V167_MAX_POLICIES      8
#define COS_V167_MAX_NODES         8
#define COS_V167_MAX_AUDIT        64

typedef enum {
    COS_V167_ROLE_ADMIN      = 0,
    COS_V167_ROLE_USER       = 1,
    COS_V167_ROLE_AUDITOR    = 2,
    COS_V167_ROLE_DEVELOPER  = 3,
} cos_v167_role_t;

typedef enum {
    COS_V167_CAP_CHAT                = 1 << 0,
    COS_V167_CAP_READ_AUDIT          = 1 << 1,
    COS_V167_CAP_WRITE_POLICY        = 1 << 2,
    COS_V167_CAP_INSTALL_PLUGIN      = 1 << 3,
    COS_V167_CAP_ENABLE_KERNEL       = 1 << 4,
    COS_V167_CAP_ROLLBACK_ADAPTER    = 1 << 5,
} cos_v167_cap_t;

typedef struct {
    char  domain[COS_V167_MAX_NAME];       /* "medical" */
    float tau;                              /* abstain threshold */
    char  abstain_message[COS_V167_MAX_MSG];
    bool  require_sandbox;
    bool  has_abstain_message;              /* false → null */
} cos_v167_policy_t;

typedef enum {
    COS_V167_HEALTH_OK        = 0,
    COS_V167_HEALTH_DEGRADED  = 1,
    COS_V167_HEALTH_DOWN      = 2,
} cos_v167_health_t;

typedef struct {
    char              node_id[COS_V167_MAX_NAME];
    char              location[COS_V167_MAX_NAME];   /* lab / cloud-a / ... */
    cos_v167_health_t health;
    int               policy_version;                /* stamp from host */
    char              adapter[COS_V167_MAX_NAME];
} cos_v167_node_t;

typedef enum {
    COS_V167_VERDICT_EMIT     = 0,
    COS_V167_VERDICT_ABSTAIN  = 1,
    COS_V167_VERDICT_REVISE   = 2,
    COS_V167_VERDICT_DENIED   = 3,
} cos_v167_verdict_t;

typedef struct {
    uint64_t          ts_fake;            /* monotonic counter */
    char              actor[COS_V167_MAX_NAME];
    cos_v167_role_t   role;
    char              node_id[COS_V167_MAX_NAME];
    char              domain[COS_V167_MAX_NAME];
    char              prompt[COS_V167_MAX_MSG];
    cos_v167_verdict_t verdict;
    float             sigma_product;
    char              note[COS_V167_MAX_MSG];
} cos_v167_audit_t;

typedef struct {
    char               org[COS_V167_MAX_NAME];
    int                policy_version;
    int                n_policies;
    cos_v167_policy_t  policies[COS_V167_MAX_POLICIES];
    int                n_nodes;
    cos_v167_node_t    nodes[COS_V167_MAX_NODES];
    int                n_audit;
    cos_v167_audit_t   audit[COS_V167_MAX_AUDIT];
    uint64_t           ts_counter;
} cos_v167_state_t;

void cos_v167_init(cos_v167_state_t *s, const char *org);

/* Policy ops. */
int  cos_v167_policy_upsert(cos_v167_state_t *s,
                            const cos_v167_policy_t *p);
const cos_v167_policy_t *cos_v167_policy_get(const cos_v167_state_t *s,
                                             const char *domain);

/* Fleet ops. */
int  cos_v167_fleet_add(cos_v167_state_t *s,
                        const char *node_id, const char *location,
                        const char *adapter);
int  cos_v167_fleet_set_health(cos_v167_state_t *s,
                               const char *node_id,
                               cos_v167_health_t h);
/* Push the current policy bundle to every node (advances version). */
int  cos_v167_fleet_push_policy(cos_v167_state_t *s);
/* Roll every node's adapter to `adapter`; returns count updated. */
int  cos_v167_fleet_rollback(cos_v167_state_t *s, const char *adapter);

/* RBAC map. */
uint32_t cos_v167_role_caps(cos_v167_role_t r);

/* Evaluate a user prompt against the active domain policy + RBAC.
 * Appends an audit entry and returns the verdict. */
cos_v167_verdict_t cos_v167_evaluate(cos_v167_state_t *s,
                                     const char *actor,
                                     cos_v167_role_t role,
                                     const char *node_id,
                                     const char *domain,
                                     const char *prompt,
                                     float sigma_product);

/* JSON emitters. */
size_t cos_v167_to_json(const cos_v167_state_t *s, char *buf, size_t cap);
size_t cos_v167_audit_to_ndjson(const cos_v167_state_t *s,
                                char *buf, size_t cap);

const char *cos_v167_role_name(cos_v167_role_t r);
const char *cos_v167_verdict_name(cos_v167_verdict_t v);
const char *cos_v167_health_name(cos_v167_health_t h);

int cos_v167_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V167_GOVERNANCE_H */
