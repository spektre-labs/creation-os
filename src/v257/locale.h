/*
 * v257 σ-Locale — localisation & cultural awareness.
 *
 *   Creation OS runs worldwide.  v257 types the locale
 *   surface: language support, cultural profile, legal
 *   compliance, and time / locale formatting — all with
 *   a σ per dimension.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Languages (exactly 10, canonical order):
 *     en · fi · zh · ja · de · fr · es · ar · hi · pt
 *   Each row carries:
 *     locale             non-empty
 *     timezone           non-empty (e.g. "Europe/Helsinki")
 *     date_format        non-empty (e.g. "DD.MM.YYYY")
 *     currency           non-empty ISO-4217 3-letter code
 *     sigma_language     ∈ [0, 1]
 *     locale_ok          = all fields typed + σ ∈ [0,1]
 *
 *   Cultural profile (exactly 3 styles — v202 culture):
 *     direct · indirect · high_context
 *   Each carries a representative example locale from
 *   the 10 above and `style_ok == true`.
 *
 *   Legal compliance (exactly 3 regimes, canonical order):
 *     EU_AI_ACT · GDPR · COLORADO_AI_ACT (June 2026)
 *   Each row: `regime`, `jurisdiction` (non-empty),
 *   `compliant == true`, `last_reviewed` tick > 0.
 *
 *   Time / locale sanity (exactly 2 checks):
 *     current_time_example  non-empty ("4:58 Helsinki")
 *     locale_lookup_ok      true (auto-detect path)
 *
 *   σ_locale (surface hygiene):
 *       σ_locale = 1 −
 *         (languages_ok + cultural_ok + legal_ok +
 *          time_ok) /
 *         (10 + 3 + 3 + 2)
 *   v0 requires `σ_locale == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 10 locale rows in canonical order, every
 *        `locale_ok == true`, every `σ_language ∈ [0, 1]`.
 *     2. Exactly 3 cultural styles in canonical order,
 *        every `style_ok == true`, example locale is one
 *        of the 10 declared.
 *     3. Exactly 3 legal regimes in canonical order,
 *        every `compliant == true`, `last_reviewed > 0`.
 *     4. Both time checks pass.
 *     5. σ_locale ∈ [0, 1] AND σ_locale == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v257.1 (named, not implemented): live locale auto-
 *     detection via OS + IANA tz db, on-disk MO/PO files
 *     for every listed locale, v202 culture-driven reply
 *     style, v199 live law-module lookup per jurisdiction,
 *     σ_compliance updated from a legal-source audit.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V257_LOCALE_H
#define COS_V257_LOCALE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V257_N_LOCALES   10
#define COS_V257_N_STYLES     3
#define COS_V257_N_REGIMES    3
#define COS_V257_N_TIME       2

typedef struct {
    char  locale[8];
    char  timezone[28];
    char  date_format[16];
    char  currency[4];
    float sigma_language;
    bool  locale_ok;
} cos_v257_locale_t;

typedef struct {
    char  style[16];
    char  example_locale[8];
    bool  style_ok;
} cos_v257_style_t;

typedef struct {
    char  regime[20];
    char  jurisdiction[20];
    bool  compliant;
    int   last_reviewed;
} cos_v257_regime_t;

typedef struct {
    char  name[28];
    bool  pass;
    char  evidence[32];
} cos_v257_time_t;

typedef struct {
    cos_v257_locale_t locales[COS_V257_N_LOCALES];
    cos_v257_style_t  styles [COS_V257_N_STYLES];
    cos_v257_regime_t regimes[COS_V257_N_REGIMES];
    cos_v257_time_t   time   [COS_V257_N_TIME];

    int   n_locales_ok;
    int   n_styles_ok;
    int   n_regimes_ok;
    int   n_time_ok;

    float sigma_locale;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v257_state_t;

void   cos_v257_init(cos_v257_state_t *s, uint64_t seed);
void   cos_v257_run (cos_v257_state_t *s);

size_t cos_v257_to_json(const cos_v257_state_t *s,
                         char *buf, size_t cap);

int    cos_v257_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V257_LOCALE_H */
