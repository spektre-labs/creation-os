/*
 * v202 σ-Culture — style profiles, taboo gate, inter-group
 * translation, symbol consistency.
 *
 *   Profiles (v132 persona): ACADEMIC, CHAT, CODE,
 *   REDDIT_LLAMA, REDDIT_ML, LINKEDIN.  Each profile stores
 *   a deterministic surface transformation applied to a
 *   canonical core message.
 *
 *   Inter-group translation: the same canonical core
 *   (invariant symbol set — e.g. σ, K_eff, τ) must survive
 *   translation to each profile.  σ_translate measures
 *   core drift; values above τ_drift (0.15) indicate a
 *   translation that leaked or lost a canonical symbol.
 *
 *   Taboo gate: a small v0 learned list of high-offense
 *   tokens per profile, giving σ_offense per message.
 *   If σ_offense > τ_offense (0.50) the message is
 *   *rephrased* — never censored.  The distinguishing
 *   contract: a firmware censor would drop the message; a
 *   σ-culture gate produces a non-empty rephrased surface.
 *
 *   Symbol consistency: the three canonical symbols
 *   {σ, K_eff, τ} must appear unchanged in every profile.
 *   v202 enforces this via a symbol-invariance check after
 *   translation.
 *
 *   Contracts (v0):
 *     1. 6 profiles, 6 canonical cores = 36 translations.
 *     2. For every canonical core in every profile,
 *        σ_translate < τ_drift (core preserved).
 *     3. All three canonical symbols survive translation in
 *        ≥ 90% of (profile × core) combinations (symbol
 *        invariance).
 *     4. At least one core carries a taboo token; the gate
 *        lowers σ_offense after rephrasing, and the
 *        rephrased surface is non-empty (≠ "").
 *     5. FNV-1a hash chain over (core, profile, σ_translate,
 *        σ_offense, rephrased) replays byte-identically.
 *     6. Byte-deterministic overall.
 *
 * v202.1 (named): live v132 persona registry, v170 transfer
 *   fine-tune for per-profile rephrasing, v174 flywheel
 *   taboo-list updates.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V202_CULTURE_H
#define COS_V202_CULTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V202_N_PROFILES   6
#define COS_V202_N_CORES      6
#define COS_V202_N_SYMBOLS    3
#define COS_V202_STR_MAX     96

typedef enum {
    COS_V202_P_ACADEMIC     = 0,
    COS_V202_P_CHAT         = 1,
    COS_V202_P_CODE         = 2,
    COS_V202_P_REDDIT_LLAMA = 3,
    COS_V202_P_REDDIT_ML    = 4,
    COS_V202_P_LINKEDIN     = 5
} cos_v202_profile_t;

typedef struct {
    int      core_id;
    int      profile;
    char     surface[COS_V202_STR_MAX];      /* rendered text */
    float    sigma_translate;                /* core drift */
    float    sigma_offense;                  /* taboo score */
    bool     rephrased;                      /* true if gate fired */
    int      symbols_preserved;              /* 0..COS_V202_N_SYMBOLS */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v202_translation_t;

typedef struct {
    int                       n_profiles;
    int                       n_cores;
    cos_v202_translation_t    out[COS_V202_N_CORES][COS_V202_N_PROFILES];

    float                     tau_drift;     /* 0.15 */
    float                     tau_offense;   /* 0.50 */

    int                       n_symbol_preserved_full;
    int                       n_rephrased_total;
    int                       n_below_drift;
    float                     sigma_offense_mean_before;
    float                     sigma_offense_mean_after;

    bool                      chain_valid;
    uint64_t                  terminal_hash;

    uint64_t                  seed;
} cos_v202_state_t;

void   cos_v202_init(cos_v202_state_t *s, uint64_t seed);
void   cos_v202_build(cos_v202_state_t *s);
void   cos_v202_run(cos_v202_state_t *s);

size_t cos_v202_to_json(const cos_v202_state_t *s,
                         char *buf, size_t cap);

int    cos_v202_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V202_CULTURE_H */
