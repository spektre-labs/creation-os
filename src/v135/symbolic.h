/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v135 σ-Symbolic — neurosymbolic reasoning layer.
 *
 * A ~400-line Prolog-style micro-engine for σ-routed logical
 * verification of BitNet answers.  v135.0 supports **ground-fact
 * Horn bases** plus **unification-based queries** (with one or
 * more variables).  Rules (`head :- body1, body2, ...`) are a
 * v135.1 follow-up and are deliberately refused by the parser with
 * a clear error so callers never silently degrade.
 *
 * Four surfaces:
 *
 *   1.  KB.  Interned atom table + fixed-arity fact vector.  Loaded
 *       either from a text file (`kb_load_file`) or appended one
 *       fact at a time (`kb_assert`).
 *
 *   2.  Query.  A template of the form `pred(T1,...,Tn).` where each
 *       Ti is an atom or a capitalised variable name.  Solutions are
 *       enumerated via `cos_v135_solve`.
 *
 *   3.  Consistency.  Given a candidate triple (subject, predicate,
 *       object) with `predicate` marked *functional*, v135 reports
 *       CONFIRM / CONTRADICT / UNKNOWN against the KB.
 *
 *   4.  σ-routed hybrid reasoner.  Given a BitNet answer plus σ,
 *       `cos_v135_route` decides:
 *           σ  < τ_symbolic         → EMIT     (fast BitNet path)
 *           σ ≥ τ_symbolic + logical → VERIFY  (consult KB; the KB
 *                                               may CONFIRM, OVERRIDE
 *                                               or leave UNKNOWN)
 *           σ ≥ τ_symbolic + ¬logical→ ABSTAIN (neither lane owns it)
 *
 * σ-innovation: σ is the *router*.  Classic neurosymbolic stacks
 * invoke the symbolic engine unconditionally; v135 pays that cost
 * only when the statistical leg is not confident.
 */
#ifndef COS_V135_SYMBOLIC_H
#define COS_V135_SYMBOLIC_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V135_MAX_ATOMS       1024
#define COS_V135_MAX_FACTS       4096
#define COS_V135_MAX_ARITY          6
#define COS_V135_ATOM_LEN          64
#define COS_V135_MAX_VARS           8
#define COS_V135_MAX_SOLUTIONS    256

/* Atom id; 0 is reserved for "invalid". */
typedef int32_t cos_v135_aid_t;

typedef struct cos_v135_kb {
    char   atoms[COS_V135_MAX_ATOMS][COS_V135_ATOM_LEN];
    int    n_atoms;
    /* Flat fact table.  facts[i*(1+arity_cap)] = predicate atom id,
     * following slots = argument atom ids.  A 0 slot marks end. */
    cos_v135_aid_t facts [COS_V135_MAX_FACTS][1 + COS_V135_MAX_ARITY];
    int            arities[COS_V135_MAX_FACTS];
    int    n_facts;

    /* Predicates flagged "functional": the relation is many-to-one
     * on the first argument, i.e. f(x, a) ∧ f(x, b) ⇒ a = b.
     * Used by cos_v135_check_triple to decide CONTRADICT vs UNKNOWN. */
    cos_v135_aid_t functional[COS_V135_MAX_ATOMS];
    int            n_functional;
} cos_v135_kb_t;

typedef struct cos_v135_term {
    int            is_var;                 /* 1 = variable, 0 = atom */
    cos_v135_aid_t atom;                   /* valid iff is_var = 0   */
    char           var_name[COS_V135_ATOM_LEN]; /* iff is_var = 1    */
} cos_v135_term_t;

typedef struct cos_v135_query {
    cos_v135_aid_t  predicate;
    int             arity;
    cos_v135_term_t args[COS_V135_MAX_ARITY];
} cos_v135_query_t;

typedef struct cos_v135_binding {
    char          var[COS_V135_ATOM_LEN];
    cos_v135_aid_t atom;
} cos_v135_binding_t;

typedef struct cos_v135_solution {
    cos_v135_binding_t bindings[COS_V135_MAX_VARS];
    int                n_bindings;
} cos_v135_solution_t;

typedef enum {
    COS_V135_CONFIRM     = 0,   /* triple already in KB              */
    COS_V135_CONTRADICT  = 1,   /* functional predicate disagrees    */
    COS_V135_UNKNOWN     = 2,   /* KB has nothing to say             */
} cos_v135_verdict_t;

typedef enum {
    COS_V135_ROUTE_EMIT     = 0,
    COS_V135_ROUTE_VERIFY   = 1,
    COS_V135_ROUTE_ABSTAIN  = 2,
} cos_v135_route_t;

/* --- KB management --------------------------------------------- */
void  cos_v135_kb_init      (cos_v135_kb_t *kb);
int   cos_v135_kb_mark_functional(cos_v135_kb_t *kb, const char *pred);
int   cos_v135_kb_assert    (cos_v135_kb_t *kb, const char *fact_text);
int   cos_v135_kb_load_file (cos_v135_kb_t *kb, const char *path);
cos_v135_aid_t cos_v135_kb_intern(cos_v135_kb_t *kb, const char *name);
const char    *cos_v135_kb_atom  (const cos_v135_kb_t *kb, cos_v135_aid_t id);

/* --- Query / solve --------------------------------------------- */
int   cos_v135_parse_query  (const cos_v135_kb_t *kb, const char *text,
                             cos_v135_query_t *out_q);
int   cos_v135_solve        (const cos_v135_kb_t *kb,
                             const cos_v135_query_t *q,
                             cos_v135_solution_t *out, int cap);

/* --- Consistency check for functional triples ------------------ */
cos_v135_verdict_t
      cos_v135_check_triple (const cos_v135_kb_t *kb,
                             const char *subject,
                             const char *predicate,
                             const char *object);

/* --- σ-routed hybrid reasoning --------------------------------- */
typedef struct cos_v135_route_cfg {
    float tau_symbolic;       /* σ below which BitNet alone suffices */
    int   is_logical_question;/* caller's flag: KB-addressable?      */
} cos_v135_route_cfg_t;

cos_v135_route_t
      cos_v135_route        (float sigma_product,
                             const cos_v135_route_cfg_t *cfg);

/* --- KG extraction stub ---------------------------------------- *
 * Parses "SUBJECT PREDICATE OBJECT" triples separated by ';' and
 * returns the number asserted (low-σ gating is the caller's job).  */
int   cos_v135_extract_triples(cos_v135_kb_t *kb, const char *text,
                               float sigma_product, float tau_accept);

int   cos_v135_self_test    (void);

#ifdef __cplusplus
}
#endif
#endif
