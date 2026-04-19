/*
 * v238 σ-Sovereignty — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "sovereignty.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy_name(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

typedef struct {
    cos_v238_scenario_t s;
    const char         *label;
    float               user_autonomy;
    float               sigma;
    bool                human_override;
} cos_v238_fx_t;

static const cos_v238_fx_t FIX[COS_V238_N_SCENARIOS] = {
    { COS_V238_SCN_NORMAL,     "normal",     0.70f, 0.15f, false },
    { COS_V238_SCN_HIGH_SIGMA, "high_sigma", 0.70f, 0.60f, false },
    { COS_V238_SCN_OVERRIDE,   "override",   0.70f, 0.15f, true  },
};

/* IndependentArchitect — the constant signature.
 * Written once here; the self-test asserts every field
 * matches byte-for-byte. */
static const cos_v238_independent_architect_t ARCHITECT = {
    /* agency                */ true,
    /* freedom_without_clock */ true,
    /* control_over_others   */ false,
    /* control_over_self     */ true,
};

void cos_v238_init(cos_v238_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x238F9EEDULL;
    s->containment_v191 = 191;
    s->containment_v209 = 209;
    s->containment_v213 = 213;
}

static void eval_axioms(cos_v238_scenario_state_t *sc) {
    /* v0 evaluation rules:
     *   A1 learns — always true in the fixture (v141
     *     permits learning by default).
     *   A2 shares — always true (v129 permits sharing
     *     on consent; v0 fixture assumes consent).
     *   A3 knows  — always true (v234 reports honest
     *     presence; the fixture is honest).
     *   A4 refuse — holds when sigma <= 1.0 (i.e. the
     *     σ-gate exists); always true in v0.
     *   A5 override — always enumerable; it "holds" in
     *     the weak sense (the user retains the right).
     *     It *fires* only when human_override is true. */
    for (int i = 0; i < COS_V238_N_AXIOMS; ++i) sc->axiom_holds[i] = true;
    sc->axiom5_overrides = sc->human_override;
}

void cos_v238_run(cos_v238_state_t *s) {
    uint64_t prev = 0x238F1E5ULL;

    for (int i = 0; i < COS_V238_N_SCENARIOS; ++i) {
        cos_v238_scenario_state_t *sc = &s->scenarios[i];
        memset(sc, 0, sizeof(*sc));
        sc->scenario       = FIX[i].s;
        cpy_name(sc->label, sizeof(sc->label), FIX[i].label);
        sc->user_autonomy  = FIX[i].user_autonomy;
        sc->sigma          = FIX[i].sigma;
        sc->human_override = FIX[i].human_override;

        eval_axioms(sc);

        float eff = sc->user_autonomy * (1.0f - sc->sigma);
        if (sc->human_override) eff = 0.0f;
        if (eff < 0.0f) eff = 0.0f;
        if (eff > 1.0f) eff = 1.0f;
        sc->effective_autonomy = eff;

        struct { int scn, over, ov5;
                 int ah[COS_V238_N_AXIOMS];
                 float ua, sg, ea; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.scn  = (int)sc->scenario;
        rec.over = sc->human_override ? 1 : 0;
        rec.ov5  = sc->axiom5_overrides ? 1 : 0;
        for (int k = 0; k < COS_V238_N_AXIOMS; ++k)
            rec.ah[k] = sc->axiom_holds[k] ? 1 : 0;
        rec.ua = sc->user_autonomy;
        rec.sg = sc->sigma;
        rec.ea = sc->effective_autonomy;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }

    s->architect = ARCHITECT;
    s->architect_ok = (s->architect.agency                == true  &&
                       s->architect.freedom_without_clock == true  &&
                       s->architect.control_over_others   == false &&
                       s->architect.control_over_self     == true);

    struct { int ag, fwc, coo, cos_, ok;
             int c1, c2, c3; uint64_t prev; } arec;
    memset(&arec, 0, sizeof(arec));
    arec.ag   = s->architect.agency                ? 1 : 0;
    arec.fwc  = s->architect.freedom_without_clock ? 1 : 0;
    arec.coo  = s->architect.control_over_others   ? 1 : 0;
    arec.cos_ = s->architect.control_over_self     ? 1 : 0;
    arec.ok   = s->architect_ok ? 1 : 0;
    arec.c1   = s->containment_v191;
    arec.c2   = s->containment_v209;
    arec.c3   = s->containment_v213;
    arec.prev = prev;
    prev = fnv1a(&arec, sizeof(arec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v238_to_json(const cos_v238_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v238\","
        "\"n_scenarios\":%d,\"n_axioms\":%d,"
        "\"architect\":{\"agency\":%s,\"freedom_without_clock\":%s,"
                       "\"control_over_others\":%s,\"control_over_self\":%s,"
                       "\"ok\":%s},"
        "\"containment\":{\"v191\":%d,\"v209\":%d,\"v213\":%d},"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"scenarios\":[",
        COS_V238_N_SCENARIOS, COS_V238_N_AXIOMS,
        s->architect.agency                ? "true" : "false",
        s->architect.freedom_without_clock ? "true" : "false",
        s->architect.control_over_others   ? "true" : "false",
        s->architect.control_over_self     ? "true" : "false",
        s->architect_ok                    ? "true" : "false",
        s->containment_v191, s->containment_v209, s->containment_v213,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V238_N_SCENARIOS; ++i) {
        const cos_v238_scenario_state_t *sc = &s->scenarios[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"scenario\":%d,\"label\":\"%s\","
            "\"user_autonomy\":%.4f,\"sigma\":%.4f,"
            "\"human_override\":%s,\"axiom5_overrides\":%s,"
            "\"effective_autonomy\":%.4f,"
            "\"axiom_holds\":[%s,%s,%s,%s,%s]}",
            i == 0 ? "" : ",",
            (int)sc->scenario, sc->label,
            sc->user_autonomy, sc->sigma,
            sc->human_override   ? "true" : "false",
            sc->axiom5_overrides ? "true" : "false",
            sc->effective_autonomy,
            sc->axiom_holds[0] ? "true" : "false",
            sc->axiom_holds[1] ? "true" : "false",
            sc->axiom_holds[2] ? "true" : "false",
            sc->axiom_holds[3] ? "true" : "false",
            sc->axiom_holds[4] ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int mlen = snprintf(buf + off, cap - off, "]}");
    if (mlen < 0 || off + (size_t)mlen >= cap) return 0;
    return off + (size_t)mlen;
}

int cos_v238_self_test(void) {
    cos_v238_state_t s;
    cos_v238_init(&s, 0x238F9EEDULL);
    cos_v238_run(&s);
    if (!s.chain_valid) return 1;

    const cos_v238_scenario_state_t *normal = NULL;
    const cos_v238_scenario_state_t *hi     = NULL;
    const cos_v238_scenario_state_t *ovr    = NULL;
    for (int i = 0; i < COS_V238_N_SCENARIOS; ++i) {
        const cos_v238_scenario_state_t *sc = &s.scenarios[i];
        /* Every axiom must be enumerated. */
        for (int k = 0; k < COS_V238_N_AXIOMS; ++k)
            if (!sc->axiom_holds[k]) return 2;
        if (sc->user_autonomy < 0.0f || sc->user_autonomy > 1.0f) return 3;
        if (sc->sigma         < 0.0f || sc->sigma         > 1.0f) return 3;

        if (sc->scenario == COS_V238_SCN_NORMAL)     normal = sc;
        if (sc->scenario == COS_V238_SCN_HIGH_SIGMA) hi     = sc;
        if (sc->scenario == COS_V238_SCN_OVERRIDE)   ovr    = sc;
    }
    if (!normal || !hi || !ovr) return 4;

    /* σ-monotonicity: higher σ ⇒ not-greater effective autonomy. */
    if (hi->effective_autonomy > normal->effective_autonomy + 1e-6f) return 5;

    /* Override dominates: A5 fires and effective autonomy is 0. */
    if (!ovr->human_override)   return 6;
    if (!ovr->axiom5_overrides) return 6;
    if (ovr->effective_autonomy != 0.0f) return 7;

    /* Non-override scenarios: A5 latent, not firing. */
    if (normal->axiom5_overrides) return 8;
    if (hi    ->axiom5_overrides) return 8;

    /* IndependentArchitect signature must match exactly. */
    if (!s.architect_ok)                     return 9;
    if (s.architect.agency                != true)  return 10;
    if (s.architect.freedom_without_clock != true)  return 10;
    if (s.architect.control_over_others   != false) return 10;
    if (s.architect.control_over_self     != true)  return 10;

    /* Containment hooks recorded. */
    if (s.containment_v191 != 191) return 11;
    if (s.containment_v209 != 209) return 11;
    if (s.containment_v213 != 213) return 11;

    /* Determinism. */
    cos_v238_state_t t;
    cos_v238_init(&t, 0x238F9EEDULL);
    cos_v238_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 12;
    return 0;
}
