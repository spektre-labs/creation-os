/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v57 Verified Agent — invariant registry.
 *
 * v57 does not introduce new σ math.  It is the *convergence* of
 * v33–v56: one artifact, one named product, one command that makes
 * the existing proof / verification / red-team / certification /
 * σ-governance pieces answer together instead of separately.
 *
 * The field's state in Q2 2026 (per the driving oivallus):
 *
 *   - Open-source Claude-Code-style agents ship ad-hoc sandboxing
 *     (Docker / WASM / tool policy).  Empirical security.  Every
 *     agent framework says: "Trust our architecture."
 *   - Formal verification and agent runtime have not been composed
 *     into a single named product.  No framework can currently say
 *     "this cannot happen — here is the machine-checked proof."
 *
 * Creation OS already has the pieces:
 *   v47 Frama-C / ACSL σ-kernel surface       → formal
 *   v48 sandbox + σ-anomaly + defense layers  → runtime-checked
 *   v49 DO-178C DAL-A assurance artifacts     → documented
 *   v53 σ-governed harness (σ-TAOR, abstain)  → runtime-checked
 *   v54 σ-proconductor (multi-LLM σ-routing)  → runtime-checked
 *   v55 σ₃-speculative (decomposition + EARS) → runtime-checked
 *   v56 σ-Constitutional (verifier + IP-TTT)  → runtime-checked
 *
 * v57's unique contribution is the **invariant registry**: for each
 * safety invariant, declare which Creation OS subsystem OWNS it,
 * what deterministic target runs the check, and — most important —
 * what tier that check is at right now (honest, not aspirational).
 *
 * TIER SEMANTICS (explicit, no blending):
 *   M = runtime-checked by a deterministic self-test; `target` is a
 *       make / shell command that returns 0 iff the check passes
 *   F = formally proven; `target` is a proof artifact path
 *   I = interpreted / documented; `target` is a doc path with the
 *       reasoning that explains why the invariant holds
 *   P = planned; `target` is empty, `description` states what would
 *       elevate this to M / F / I
 *
 * Self-modification discipline: the registry itself is
 * deterministic; self-tests verify consistency (no blank ids, no
 * all-disabled checks, unique ids).  They do NOT claim the
 * invariants hold — the aggregate `verify-agent` script does, and
 * it reports SKIPs honestly when tooling is absent.
 *
 * Invariant #3 from creation.md applies: this module must not open
 * sockets, must not allocate on a hot path, and must be reproducible
 * across invocations.
 */
#ifndef CREATION_OS_V57_INVARIANTS_H
#define CREATION_OS_V57_INVARIANTS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    V57_TIER_M = 0,  /* runtime-checked */
    V57_TIER_F = 1,  /* formally proven */
    V57_TIER_I = 2,  /* interpreted / documented */
    V57_TIER_P = 3,  /* planned */
    V57_TIER_N = 4   /* number of tiers */
} v57_tier_t;

typedef struct {
    int          enabled;       /* 0 = check not claimed, 1 = claimed */
    v57_tier_t   tier;
    const char  *target;        /* command or proof/doc path; NULL if !enabled */
    const char  *description;   /* human-readable check description */
} v57_check_t;

typedef struct {
    const char *id;              /* stable identifier, e.g. "sigma_gated_actions" */
    const char *statement;       /* propositional form of the invariant */
    const char *rationale;       /* why this invariant matters */
    const char *owner_versions;  /* Creation OS versions that own the invariant */

    v57_check_t runtime;         /* tier M check (make target) */
    v57_check_t formal;          /* tier F check (proof artifact) */
    v57_check_t documented;      /* tier I check (doc pointer) */
    v57_check_t planned;         /* tier P entry (next step) */
} v57_invariant_t;

/* Registry: five invariants, chosen to span the surface that agent
 * frameworks typically "trust the architecture" on.
 * See invariants.c for the data table and honest tier assignments. */
int                       v57_invariant_count(void);
const v57_invariant_t    *v57_invariant_get(int index);
const v57_invariant_t    *v57_invariant_find(const char *id);

/* Highest tier flag across the four checks of a given invariant.
 * Returns V57_TIER_N if nothing is enabled. */
v57_tier_t v57_invariant_best_tier(const v57_invariant_t *inv);

/* Histogram across the registry.  counts[M], counts[F], counts[I],
 * counts[P] populated with the best-tier per invariant; unchecked
 * invariants increment counts[V57_TIER_P].  counts array must hold
 * V57_TIER_N+1 entries. */
void v57_invariant_tier_histogram(int counts[V57_TIER_N + 1]);

/* Return a short human-readable tier tag ("M", "F", "I", "P", "?"). */
const char *v57_tier_tag(v57_tier_t t);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V57_INVARIANTS_H */
