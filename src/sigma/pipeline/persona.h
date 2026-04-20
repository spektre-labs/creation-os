/*
 * σ-Persona — adaptive user profile.
 *
 * Fine-tuning a per-user model costs too much and freezes the
 * user into whatever embedding the upstream lab shipped.  Persona
 * is cheaper and reversible: a small profile
 *
 *     (language, expertise, domains[], verbosity, formality)
 *
 * is injected into the Codex envelope so the model adjusts tone
 * and depth per user without touching weights.  Updates come from
 * three deterministic signals:
 *
 *   1. query history       → domain vocabulary
 *   2. explicit feedback   → verbosity / formality sliders
 *   3. language detection  → response language
 *
 * The update rule is EMA with a fixed learning rate, so the
 * trajectory is reproducible and a malicious "too long" spam is
 * naturally bounded.
 *
 * Storage is a stable JSON blob the caller is expected to persist
 * at ~/.cos/persona.json; this kernel only provides the
 * (deterministic) read/write helpers and lives happily in RAM.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PERSONA_H
#define COS_SIGMA_PERSONA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_PERSONA_MAX_DOMAINS        16
#define COS_PERSONA_DOMAIN_MAX_CHARS    32
#define COS_PERSONA_NAME_MAX           128
#define COS_PERSONA_LANG_MAX             8
#define COS_PERSONA_LEVEL_MAX           16

enum cos_persona_status {
    COS_PERSONA_OK      =  0,
    COS_PERSONA_ERR_ARG = -1,
    COS_PERSONA_ERR_BUF = -2,
};

/* Explicit user feedback tokens — stable, English, lower-case. */
typedef enum {
    COS_PERSONA_FB_NONE         = 0,
    COS_PERSONA_FB_TOO_LONG     = 1,   /* -> verbosity ↓        */
    COS_PERSONA_FB_TOO_SHORT    = 2,   /* -> verbosity ↑        */
    COS_PERSONA_FB_TOO_TECHNICAL= 3,   /* -> expertise beginner */
    COS_PERSONA_FB_TOO_BASIC    = 4,   /* -> expertise expert   */
    COS_PERSONA_FB_TOO_FORMAL   = 5,   /* -> formality  ↓       */
    COS_PERSONA_FB_TOO_CASUAL   = 6,   /* -> formality  ↑       */
    COS_PERSONA_FB_PERFECT      = 7,   /* -> half-rate decay to current */
} cos_persona_feedback_t;

typedef struct {
    char  domain[COS_PERSONA_DOMAIN_MAX_CHARS];
    int   mentions;      /* monotonic query hit count                */
    float weight;        /* normalised relevance ∈ [0, 1]            */
} cos_persona_domain_t;

typedef struct {
    char  name[COS_PERSONA_NAME_MAX];
    char  language[COS_PERSONA_LANG_MAX];     /* "en", "fi", ...  */
    char  expertise_level[COS_PERSONA_LEVEL_MAX]; /* beginner/intermediate/expert */
    cos_persona_domain_t domains[COS_PERSONA_MAX_DOMAINS];
    int   n_domains;
    int   total_domain_mentions;
    float verbosity;     /* 0.0 = terse, 1.0 = long-form            */
    float formality;     /* 0.0 = casual, 1.0 = formal              */
    int   turns_observed;
    int   last_feedback;
} cos_persona_t;

/* -------- Lifecycle ---------------------------------------------- */
int  cos_persona_init(cos_persona_t *p, const char *name);
void cos_persona_reset(cos_persona_t *p);

/* -------- Language detection (pure, heuristic) ------------------ */
/* Returns a two-char ISO language code in `out` (always 2 chars +
 * NUL).  Deterministic heuristic: Finnish stop-words → "fi",
 * otherwise "en". */
int  cos_persona_detect_language(const char *text, char out[COS_PERSONA_LANG_MAX]);

/* -------- Learning ---------------------------------------------- */

/* Observe a query: detect language, extract up to 3 domain keywords,
 * EMA-update the language / domains.  Returns COS_PERSONA_OK. */
int cos_persona_observe_query(cos_persona_t *p, const char *query);

/* Apply explicit user feedback. */
int cos_persona_apply_feedback(cos_persona_t *p,
                               cos_persona_feedback_t fb);

/* Convenience: observe_query + apply_feedback in one call.  `feedback`
 * may be a short English token ("too long", "perfect", ...) or NULL. */
int cos_persona_learn(cos_persona_t *p,
                      const char *query,
                      const char *feedback);

/* -------- Introspection ----------------------------------------- */

/* Render the persona into `dst` as a stable, sorted JSON blob.
 * Returns bytes written (excluding NUL) or a negative status. */
int cos_persona_to_json(const cos_persona_t *p, char *dst, size_t cap);

/* Render into `dst` as the system-prompt envelope string Codex
 * prepends to every turn.  Example:
 *   "[persona] language=fi expertise=expert verbosity=0.40
 *    formality=0.60 domains=sigma,rag,offline"
 */
int cos_persona_to_envelope(const cos_persona_t *p, char *dst, size_t cap);

/* -------- Self-test --------------------------------------------- */
int cos_persona_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_PERSONA_H */
