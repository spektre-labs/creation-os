/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v53 σ-triggered sub-agent dispatch — scaffold.
 *
 * Design: Claude Code dispatches sub-agents when the user / main agent
 * asks for one. v53 dispatches ONLY when main-agent σ in a specific
 * domain exceeds a threshold. The specialist runs in fresh context
 * (Mama Claude insight: uncorrelated context = test-time compute) and
 * returns a SUMMARY with its own σ — never its full trace.
 */
#ifndef CREATION_OS_V53_HARNESS_DISPATCH_H
#define CREATION_OS_V53_HARNESS_DISPATCH_H

#include "../../sigma/decompose.h"

#ifdef __cplusplus
extern "C" {
#endif

#define V53_DOMAIN_NAME_MAX 32

typedef struct {
    char  name[V53_DOMAIN_NAME_MAX]; /* "security", "performance", … */
    float spawn_trigger;              /* main-agent σ in domain ≥ this → spawn */
    float specialist_sigma_cap;       /* specialist abstains above this */
} v53_specialist_cfg_t;

typedef struct {
    int   spawned;
    char  domain[V53_DOMAIN_NAME_MAX];
    float specialist_sigma_observed;  /* σ reported by specialist */
    int   specialist_abstained;       /* 1 if specialist itself bailed out */
    const char *summary;              /* short pointer; not owned */
} v53_dispatch_result_t;

/**
 * Decide whether to spawn a specialist for a given domain.
 * No actual sub-process is started; this is the policy surface.
 *
 * Rules:
 *   - if no specialist for `domain` → spawned = 0
 *   - if main σ in domain < cfg.spawn_trigger → spawned = 0
 *   - else spawned = 1; specialist_sigma_observed is produced via the
 *     caller-supplied `specialist_sigma_callback` (or defaults to
 *     `main_domain_sigma * 0.5f` as a "fresh context lowers σ" proxy).
 *   - specialist_abstained = 1 iff specialist_sigma_observed > cfg.specialist_sigma_cap
 */
v53_dispatch_result_t v53_dispatch_if_needed(
    const v53_specialist_cfg_t *cfgs, int n_cfgs,
    const char *domain,
    const sigma_decomposed_t *main_domain_sigma);

/* Helper for tests: build a default specialist table covering
 * {security, performance, correctness}. Caller owns the storage. */
int v53_default_specialists(v53_specialist_cfg_t *out, int cap);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V53_HARNESS_DISPATCH_H */
