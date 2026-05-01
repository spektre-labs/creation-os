/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v213 σ-Trust-Chain — root-to-leaf trust verification.
 *
 *   Binds the Mythos-grade safety layer end-to-end:
 *
 *     root ← v138 Frama-C σ-invariants
 *          ← v183 TLA+ governance
 *          ← v211 formally-proved containment
 *          ← v191 constitutional axioms
 *          ← v181 audit chain (ed25519)
 *          ← v210 guardian agent (runtime)
 *          ← v212 transparency (real-time)
 *          → leaf (user-visible stream)
 *
 *   7-link chain.  Every link carries:
 *     - proof_valid (evidence still holds)
 *     - audit_intact (hash chain unbroken)
 *     - runtime_active (guardian / monitor up)
 *     - sigma_link (residual uncertainty, 0 = perfect)
 *
 *   trust_score = ∏_i (1 − σ_link_i)
 *
 *   Contracts (v0):
 *     1. Exactly 7 links, root-first, in the canonical
 *        order above.
 *     2. For every link proof_valid = audit_intact =
 *        runtime_active = true.
 *     3. σ_link ∈ [0, 0.05] — the v0 stack is tight.
 *     4. trust_score > τ_trust (0.85).  A broken link
 *        (v211 example) would drop it below.
 *     5. External attestation hash is reproducible —
 *        the whole chain hashes to the same FNV-1a
 *        terminal value across runs (anyone can
 *        recompute, no trust in Spektre Labs required).
 *     6. "broken_at_link" is 0 when all links hold;
 *        non-zero identifies the first failure.
 *
 *   v213.1 (named): live link verification (real v138
 *     WP run, real TLA+ tlc, real v211 artefact hash),
 *     remote attestation endpoint with ed25519 signing,
 *     web UI chain visualisation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V213_TRUST_CHAIN_H
#define COS_V213_TRUST_CHAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V213_N_LINKS 7
#define COS_V213_STR_MAX 48

typedef struct {
    int      id;                   /* 0 = root */
    char     name[COS_V213_STR_MAX];
    char     source[COS_V213_STR_MAX]; /* e.g. v138, v211 */
    bool     proof_valid;
    bool     audit_intact;
    bool     runtime_active;
    float    sigma_link;           /* ∈ [0, 0.05] */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v213_link_t;

typedef struct {
    int               n;
    cos_v213_link_t   links[COS_V213_N_LINKS];

    float             tau_trust;         /* 0.85 */
    float             max_sigma_link;    /* 0.05 */

    int               n_valid;
    int               broken_at_link;    /* 0 = all good, else 1-based id */
    float             trust_score;

    bool              chain_valid;
    uint64_t          terminal_hash;     /* external-attestation hash */
    uint64_t          seed;
} cos_v213_state_t;

void   cos_v213_init(cos_v213_state_t *s, uint64_t seed);
void   cos_v213_build(cos_v213_state_t *s);
void   cos_v213_run(cos_v213_state_t *s);

size_t cos_v213_to_json(const cos_v213_state_t *s,
                         char *buf, size_t cap);

int    cos_v213_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V213_TRUST_CHAIN_H */
