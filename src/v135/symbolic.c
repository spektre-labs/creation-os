/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v135 σ-Symbolic — implementation.
 */
#include "symbolic.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =================================================================
 * Atoms / KB
 * ============================================================== */

void cos_v135_kb_init(cos_v135_kb_t *kb) {
    if (!kb) return;
    memset(kb, 0, sizeof *kb);
    /* Reserve atom id 0 = "" invalid. */
    kb->n_atoms = 1;
}

cos_v135_aid_t cos_v135_kb_intern(cos_v135_kb_t *kb, const char *name) {
    if (!kb || !name || !*name) return 0;
    /* atom names are lowercased on intern — Prolog convention   */
    char norm[COS_V135_ATOM_LEN];
    int n = 0;
    for (; name[n] && n < COS_V135_ATOM_LEN - 1; ++n) {
        char c = name[n];
        norm[n] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    norm[n] = 0;
    for (int i = 1; i < kb->n_atoms; ++i)
        if (!strcmp(kb->atoms[i], norm)) return (cos_v135_aid_t)i;
    if (kb->n_atoms >= COS_V135_MAX_ATOMS) return 0;
    strcpy(kb->atoms[kb->n_atoms], norm);
    return (cos_v135_aid_t)(kb->n_atoms++);
}

const char *cos_v135_kb_atom(const cos_v135_kb_t *kb, cos_v135_aid_t id) {
    if (!kb || id <= 0 || id >= kb->n_atoms) return "";
    return kb->atoms[id];
}

int cos_v135_kb_mark_functional(cos_v135_kb_t *kb, const char *pred) {
    if (!kb || !pred) return -1;
    cos_v135_aid_t a = cos_v135_kb_intern(kb, pred);
    if (!a) return -1;
    for (int i = 0; i < kb->n_functional; ++i)
        if (kb->functional[i] == a) return 0;
    if (kb->n_functional >= COS_V135_MAX_ATOMS) return -1;
    kb->functional[kb->n_functional++] = a;
    return 0;
}

static int kb_is_functional(const cos_v135_kb_t *kb, cos_v135_aid_t a) {
    for (int i = 0; i < kb->n_functional; ++i)
        if (kb->functional[i] == a) return 1;
    return 0;
}

/* --- tokenizer ------------------------------------------------- */
typedef struct {
    const char *s;
    size_t      i;
} v135_lex_t;

static void lex_skip(v135_lex_t *L) {
    while (L->s[L->i] && (isspace((unsigned char)L->s[L->i])
                          || L->s[L->i] == '\n'))
        L->i++;
    /* line comments starting with % */
    while (L->s[L->i] == '%') {
        while (L->s[L->i] && L->s[L->i] != '\n') L->i++;
        while (L->s[L->i] && isspace((unsigned char)L->s[L->i])) L->i++;
    }
}

/* Read a token: identifier or single punctuation. */
static int lex_ident(v135_lex_t *L, char *out, size_t cap) {
    lex_skip(L);
    size_t o = 0;
    while (L->s[L->i] && (isalnum((unsigned char)L->s[L->i])
                          || L->s[L->i] == '_')) {
        if (o + 1 >= cap) return -1;
        out[o++] = L->s[L->i++];
    }
    out[o] = 0;
    return (int)o;
}

static int lex_expect(v135_lex_t *L, char c) {
    lex_skip(L);
    if (L->s[L->i] != c) return -1;
    L->i++;
    return 0;
}

/* Parse a fact or query body "pred(arg1, arg2, ...)" starting at L.
 * Writes the predicate atom (interned into `kb` if not NULL) into
 * *out_pred, and the raw token strings into out_args.  Variables
 * keep their original capitalisation; atoms stay lowercase.
 * Returns the arity or -1 on error. */
static int parse_term_head(v135_lex_t *L, cos_v135_kb_t *kb,
                           cos_v135_aid_t *out_pred,
                           char args[][COS_V135_ATOM_LEN],
                           int *is_var /* nullable */) {
    char buf[COS_V135_ATOM_LEN];
    if (lex_ident(L, buf, sizeof buf) <= 0) return -1;
    if (kb) *out_pred = cos_v135_kb_intern(kb, buf);
    else    *out_pred = 0;
    if (lex_expect(L, '(') < 0) return -1;
    int n = 0;
    for (;;) {
        if (n >= COS_V135_MAX_ARITY) return -1;
        if (lex_ident(L, args[n], COS_V135_ATOM_LEN) <= 0) return -1;
        if (is_var)
            is_var[n] = (args[n][0] >= 'A' && args[n][0] <= 'Z') ? 1 : 0;
        n++;
        lex_skip(L);
        if (L->s[L->i] == ',') { L->i++; continue; }
        if (L->s[L->i] == ')') { L->i++; break; }
        return -1;
    }
    return n;
}

int cos_v135_kb_assert(cos_v135_kb_t *kb, const char *fact_text) {
    if (!kb || !fact_text) return -1;
    /* Rules not supported in v135.0. */
    if (strstr(fact_text, ":-")) {
        fprintf(stderr,
            "v135: rules (:-) are a v135.1 follow-up; "
            "only ground facts are accepted here\n");
        return -2;
    }
    v135_lex_t L = { fact_text, 0 };
    cos_v135_aid_t pred = 0;
    char raw[COS_V135_MAX_ARITY][COS_V135_ATOM_LEN];
    int  isv[COS_V135_MAX_ARITY];
    int  arity = parse_term_head(&L, kb, &pred, raw, isv);
    if (arity <= 0 || !pred) return -1;
    for (int i = 0; i < arity; ++i)
        if (isv[i]) {
            fprintf(stderr, "v135: fact must be ground (arg %d is '%s')\n",
                    i, raw[i]);
            return -1;
        }
    if (kb->n_facts >= COS_V135_MAX_FACTS) return -1;
    kb->facts[kb->n_facts][0] = pred;
    for (int i = 0; i < arity; ++i)
        kb->facts[kb->n_facts][1 + i] = cos_v135_kb_intern(kb, raw[i]);
    kb->arities[kb->n_facts] = arity;
    kb->n_facts++;
    return 0;
}

int cos_v135_kb_load_file(cos_v135_kb_t *kb, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char line[512];
    int n = 0;
    while (fgets(line, sizeof line, fp)) {
        /* strip trailing '.' and whitespace */
        char *end = line + strlen(line);
        while (end > line && (end[-1] == '\n' || end[-1] == '\r'
                              || isspace((unsigned char)end[-1])
                              || end[-1] == '.')) { end[-1] = 0; end--; }
        if (!line[0] || line[0] == '%') continue;
        if (cos_v135_kb_assert(kb, line) == 0) n++;
    }
    fclose(fp);
    return n;
}

/* =================================================================
 * Query / solve
 * ============================================================== */

int cos_v135_parse_query(const cos_v135_kb_t *kb, const char *text,
                         cos_v135_query_t *out) {
    if (!kb || !text || !out) return -1;
    memset(out, 0, sizeof *out);
    /* Accept optional leading "?- " */
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p[0] == '?' && p[1] == '-') { p += 2; while (*p == ' ') p++; }
    v135_lex_t L = { p, 0 };
    cos_v135_aid_t pred = 0;
    char raw[COS_V135_MAX_ARITY][COS_V135_ATOM_LEN];
    int  isv[COS_V135_MAX_ARITY];
    /* We must intern atoms in a non-const KB; do a const-stripped
     * lookup here to keep atoms consistent. */
    int arity = parse_term_head(&L, (cos_v135_kb_t *)kb, &pred, raw, isv);
    if (arity <= 0 || !pred) return -1;
    out->predicate = pred;
    out->arity = arity;
    for (int i = 0; i < arity; ++i) {
        out->args[i].is_var = isv[i];
        if (isv[i]) {
            strncpy(out->args[i].var_name, raw[i],
                    COS_V135_ATOM_LEN - 1);
        } else {
            out->args[i].atom =
                cos_v135_kb_intern((cos_v135_kb_t *)kb, raw[i]);
        }
    }
    return 0;
}

int cos_v135_solve(const cos_v135_kb_t *kb,
                   const cos_v135_query_t *q,
                   cos_v135_solution_t *out, int cap) {
    if (!kb || !q || !out || cap <= 0) return -1;
    int n = 0;
    for (int f = 0; f < kb->n_facts; ++f) {
        if (kb->facts[f][0] != q->predicate) continue;
        if (kb->arities[f] != q->arity)      continue;
        /* Attempt unification with repeated-variable support. */
        cos_v135_binding_t bind[COS_V135_MAX_VARS];
        int nb = 0;
        int ok = 1;
        for (int i = 0; i < q->arity; ++i) {
            cos_v135_aid_t fact_arg = kb->facts[f][1 + i];
            if (!q->args[i].is_var) {
                if (q->args[i].atom != fact_arg) { ok = 0; break; }
            } else {
                /* Check if this variable was bound earlier. */
                int found = -1;
                for (int k = 0; k < nb; ++k)
                    if (!strcmp(bind[k].var, q->args[i].var_name))
                        { found = k; break; }
                if (found >= 0) {
                    if (bind[found].atom != fact_arg) { ok = 0; break; }
                } else if (nb < COS_V135_MAX_VARS) {
                    strncpy(bind[nb].var, q->args[i].var_name,
                            COS_V135_ATOM_LEN - 1);
                    bind[nb].var[COS_V135_ATOM_LEN - 1] = 0;
                    bind[nb].atom = fact_arg;
                    nb++;
                }
            }
        }
        if (!ok) continue;
        if (n < cap) {
            out[n].n_bindings = nb;
            for (int k = 0; k < nb; ++k) out[n].bindings[k] = bind[k];
            n++;
        }
    }
    return n;
}

/* =================================================================
 * Triple consistency check
 * ============================================================== */

cos_v135_verdict_t
cos_v135_check_triple(const cos_v135_kb_t *kb,
                      const char *subject,
                      const char *predicate,
                      const char *object) {
    if (!kb || !subject || !predicate || !object)
        return COS_V135_UNKNOWN;
    cos_v135_aid_t sp = cos_v135_kb_intern((cos_v135_kb_t *)kb, subject);
    cos_v135_aid_t pp = cos_v135_kb_intern((cos_v135_kb_t *)kb, predicate);
    cos_v135_aid_t op = cos_v135_kb_intern((cos_v135_kb_t *)kb, object);
    int found_same = 0, found_different = 0;
    for (int f = 0; f < kb->n_facts; ++f) {
        if (kb->facts[f][0] != pp) continue;
        if (kb->arities[f] < 2)    continue;
        if (kb->facts[f][1] != sp) continue;
        if (kb->facts[f][2] == op) found_same = 1;
        else                       found_different = 1;
    }
    if (found_same) return COS_V135_CONFIRM;
    if (found_different && kb_is_functional(kb, pp))
        return COS_V135_CONTRADICT;
    return COS_V135_UNKNOWN;
}

/* =================================================================
 * σ-routed hybrid reasoner
 * ============================================================== */

cos_v135_route_t
cos_v135_route(float sigma, const cos_v135_route_cfg_t *cfg) {
    if (!cfg) return COS_V135_ROUTE_EMIT;
    float tau = (cfg->tau_symbolic <= 0.0f) ? 0.30f : cfg->tau_symbolic;
    if (sigma < tau)                  return COS_V135_ROUTE_EMIT;
    if (cfg->is_logical_question)     return COS_V135_ROUTE_VERIFY;
    return COS_V135_ROUTE_ABSTAIN;
}

/* =================================================================
 * KG extraction stub
 * Input: "subj1 pred1 obj1 ; subj2 pred2 obj2"
 * Accept only when sigma_product ≤ tau_accept (callers pick τ).
 * ============================================================== */

int cos_v135_extract_triples(cos_v135_kb_t *kb, const char *text,
                             float sigma_product, float tau_accept) {
    if (!kb || !text) return 0;
    if (sigma_product > tau_accept) return 0;
    int n = 0;
    const char *p = text;
    while (*p) {
        char subj[COS_V135_ATOM_LEN], pred[COS_V135_ATOM_LEN],
             obj [COS_V135_ATOM_LEN];
        int ns = 0, np = 0, no = 0;
        while (*p && isspace((unsigned char)*p)) p++;
        while (*p && !isspace((unsigned char)*p) && *p != ';' && ns < COS_V135_ATOM_LEN - 1) subj[ns++] = *p++;
        subj[ns] = 0;
        while (*p && isspace((unsigned char)*p)) p++;
        while (*p && !isspace((unsigned char)*p) && *p != ';' && np < COS_V135_ATOM_LEN - 1) pred[np++] = *p++;
        pred[np] = 0;
        while (*p && isspace((unsigned char)*p)) p++;
        while (*p && *p != ';' && no < COS_V135_ATOM_LEN - 1) {
            if (isspace((unsigned char)*p)) { p++; continue; }
            obj[no++] = *p++;
        }
        obj[no] = 0;
        if (ns && np && no) {
            char fact[4 * COS_V135_ATOM_LEN];
            snprintf(fact, sizeof fact, "%s(%s,%s)", pred, subj, obj);
            if (cos_v135_kb_assert(kb, fact) == 0) n++;
        }
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
    }
    return n;
}

/* =================================================================
 * Self-test
 * ============================================================== */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v135 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v135_self_test(void) {
    cos_v135_kb_t kb; cos_v135_kb_init(&kb);

    /* --- A. Assert ground facts -------------------------------- */
    fprintf(stderr, "check-v135: assert facts\n");
    _CHECK(cos_v135_kb_assert(&kb, "works_at(lauri, spektre_labs)")   == 0,
           "assert works_at");
    _CHECK(cos_v135_kb_assert(&kb, "works_at(alice, openai)")         == 0,
           "assert alice");
    _CHECK(cos_v135_kb_assert(&kb, "located_in(spektre_labs, helsinki)")  == 0,
           "assert located_in");
    _CHECK(cos_v135_kb_assert(&kb, "developed(lauri, creation_os)")   == 0,
           "assert developed");
    /* Rule input should be refused. */
    _CHECK(cos_v135_kb_assert(&kb, "ancestor(X,Y) :- parent(X,Y)")    == -2,
           "rules refused");

    /* --- B. Parse + solve query with variable ------------------ */
    fprintf(stderr, "check-v135: query ?- works_at(X, spektre_labs)\n");
    cos_v135_query_t q;
    _CHECK(cos_v135_parse_query(&kb,
            "?- works_at(X, spektre_labs).", &q) == 0, "parse query");
    cos_v135_solution_t sols[COS_V135_MAX_SOLUTIONS];
    int ns = cos_v135_solve(&kb, &q, sols, COS_V135_MAX_SOLUTIONS);
    _CHECK(ns == 1, "one solution");
    _CHECK(sols[0].n_bindings == 1, "one binding");
    _CHECK(!strcmp(sols[0].bindings[0].var, "X"), "var name = X");
    _CHECK(!strcmp(cos_v135_kb_atom(&kb, sols[0].bindings[0].atom),
                   "lauri"), "X bound to lauri");

    /* --- C. Consistency: CONFIRM / CONTRADICT / UNKNOWN -------- */
    fprintf(stderr, "check-v135: functional predicate consistency\n");
    cos_v135_kb_mark_functional(&kb, "works_at");
    /* CONFIRM: triple already present. */
    _CHECK(cos_v135_check_triple(&kb, "lauri", "works_at", "spektre_labs")
           == COS_V135_CONFIRM, "CONFIRM");
    /* CONTRADICT: BitNet claim "lauri works_at openai" conflicts
     * because works_at is functional and we have spektre_labs. */
    _CHECK(cos_v135_check_triple(&kb, "lauri", "works_at", "openai")
           == COS_V135_CONTRADICT, "CONTRADICT");
    /* UNKNOWN: subject we've never heard of. */
    _CHECK(cos_v135_check_triple(&kb, "zod", "works_at", "acme")
           == COS_V135_UNKNOWN, "UNKNOWN");

    /* --- D. σ-routed hybrid reasoner --------------------------- */
    fprintf(stderr, "check-v135: σ-router\n");
    cos_v135_route_cfg_t rc = { .tau_symbolic = 0.30f,
                                .is_logical_question = 1 };
    _CHECK(cos_v135_route(0.10f, &rc) == COS_V135_ROUTE_EMIT,
           "low σ → EMIT");
    _CHECK(cos_v135_route(0.50f, &rc) == COS_V135_ROUTE_VERIFY,
           "high σ + logical → VERIFY");
    rc.is_logical_question = 0;
    _CHECK(cos_v135_route(0.50f, &rc) == COS_V135_ROUTE_ABSTAIN,
           "high σ + non-logical → ABSTAIN");

    /* --- E. KG extraction stub, σ-gated ------------------------ */
    fprintf(stderr, "check-v135: KG extraction (σ=0.10, τ=0.20)\n");
    int before = kb.n_facts;
    int k = cos_v135_extract_triples(&kb,
        "bob works_at acme ; acme located_in nyc",
        0.10f, 0.20f);
    _CHECK(k == 2 && kb.n_facts == before + 2, "two facts asserted");
    /* High σ → nothing accepted. */
    before = kb.n_facts;
    k = cos_v135_extract_triples(&kb,
        "eve works_at mars", 0.90f, 0.20f);
    _CHECK(k == 0 && kb.n_facts == before, "high σ rejects extraction");

    fprintf(stderr, "check-v135: OK (KB + query + consistency + router)\n");
    return 0;
}
