/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v250 σ-A2A — Agent-to-Agent Protocol.
 *
 *   v249 (MCP) talks to tools.  v250 talks to *other
 *   agents*, and carries a σ on every cross-agent
 *   envelope.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Agent Card (exactly 6 required fields):
 *     name         "Creation OS"
 *     capabilities ["reason","plan","create","simulate",
 *                    "teach","coherence"]   (6)
 *     sigma_profile.mean        ∈ [0, 1]
 *     sigma_profile.calibration ∈ [0, 1]
 *     presence     "LIVE"   (v234 enum)
 *     scsl         true     (SCSL-1.0 attestation)
 *     endpoint     "https://..." (non-empty)
 *
 *   Task delegation (v0 fixture: 4 tasks):
 *     - factual reason, σ = 0.18 → ACCEPT
 *     - plan,           σ = 0.34 → ACCEPT
 *     - creative,       σ = 0.62 → NEGOTIATE (σ > τ_neg=0.50)
 *     - moral dilemma,  σ = 0.81 → REFUSE   (σ > τ_refuse=0.75)
 *
 *   Task negotiation:
 *     Whenever σ > τ_neg, v250 emits a negotiation envelope
 *     ("I can do this but σ=X. Still want me to?").  v201
 *     diplomacy is cited as the binding upstream kernel.
 *     The fixture requires ≥ 1 ACCEPT, ≥ 1 NEGOTIATE,
 *     and ≥ 1 REFUSE so all branches are exercised.
 *
 *   Federation (exactly 3 Creation OS partners):
 *     alice · bob · carol — each with `σ_trust ∈ [0,1]`
 *     and a `presence` marker.
 *     v128 mesh is named as the A2A transport; v129
 *     federated learning rides over it.
 *
 *   σ_a2a (surface hygiene):
 *       σ_a2a = 1 −
 *         (card_fields_ok + delegation_ok +
 *          federation_ok) /
 *         (6 + 4 + 3)
 *   v0 requires `σ_a2a == 0.0`.
 *
 *   Contracts (v0):
 *     1. Agent Card has all 6 required fields non-empty;
 *        presence == "LIVE"; scsl == true.
 *     2. capabilities has exactly 6 non-empty entries in
 *        canonical order.
 *     3. sigma_profile.{mean,calibration} ∈ [0, 1].
 *     4. 4 delegation fixtures; every σ_product ∈ [0, 1];
 *        decision strictly matches σ vs {τ_neg, τ_refuse};
 *        ≥ 1 ACCEPT AND ≥ 1 NEGOTIATE AND ≥ 1 REFUSE.
 *     5. 3 federation partners in canonical order;
 *        every σ_trust ∈ [0, 1].
 *     6. σ_a2a ∈ [0, 1] AND σ_a2a == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v250.1 (named, not implemented): live A2A wire
 *     protocol, real cross-agent TLS handshake,
 *     v201 diplomacy-driven negotiation state machine,
 *     v129 federated learning over the real mesh
 *     transport.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V250_A2A_H
#define COS_V250_A2A_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V250_N_CAPABILITIES 6
#define COS_V250_N_TASKS        4
#define COS_V250_N_PARTNERS     3

typedef enum {
    COS_V250_DEC_ACCEPT    = 1,
    COS_V250_DEC_NEGOTIATE = 2,
    COS_V250_DEC_REFUSE    = 3
} cos_v250_decision_t;

typedef struct {
    char  name[32];
    char  capabilities[COS_V250_N_CAPABILITIES][16];
    int   n_capabilities;
    float sigma_mean;
    float sigma_calibration;
    char  presence[12];    /* "LIVE" etc. */
    bool  scsl;
    char  endpoint[64];
    bool  card_ok;
} cos_v250_card_t;

typedef struct {
    char                 task_type[16];
    float                sigma_product;
    cos_v250_decision_t  decision;
} cos_v250_task_t;

typedef struct {
    char   name[16];
    float  sigma_trust;
    char   presence[12];
} cos_v250_partner_t;

typedef struct {
    cos_v250_card_t    card;
    cos_v250_task_t    tasks   [COS_V250_N_TASKS];
    cos_v250_partner_t partners[COS_V250_N_PARTNERS];

    float  tau_neg;
    float  tau_refuse;
    int    n_accept;
    int    n_negotiate;
    int    n_refuse;
    int    n_card_fields_ok;
    int    n_partners_ok;

    float  sigma_a2a;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v250_state_t;

void   cos_v250_init(cos_v250_state_t *s, uint64_t seed);
void   cos_v250_run (cos_v250_state_t *s);

size_t cos_v250_to_json(const cos_v250_state_t *s,
                         char *buf, size_t cap);

int    cos_v250_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V250_A2A_H */
