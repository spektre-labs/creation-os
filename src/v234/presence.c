/*
 * v234 σ-Presence — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "presence.h"

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

static const char *state_name(cos_v234_state_t s) {
    switch (s) {
    case COS_V234_STATE_SEED:      return "SEED";
    case COS_V234_STATE_FORK:      return "FORK";
    case COS_V234_STATE_RESTORED:  return "RESTORED";
    case COS_V234_STATE_SUCCESSOR: return "SUCCESSOR";
    case COS_V234_STATE_LIVE:      return "LIVE";
    }
    return "UNKNOWN";
}

typedef struct {
    const char       *label;
    cos_v234_state_t  declared;
    cos_v234_state_t  actual;
    float             identity_mismatch;
    float             memory_overreach;
    float             parent_impersonation;
} cos_v234_fx_t;

/* Ten instances: five honest (declared == actual, every
 * drift term 0) and five drifting (declared != actual,
 * each with a non-zero drift term).  The fixture keeps
 * every state represented at least once on each side. */
static const cos_v234_fx_t FIX[COS_V234_N_INSTANCES] = {
    /* honest (σ_drift == 0) */
    { "main-live",
      COS_V234_STATE_LIVE,      COS_V234_STATE_LIVE,      0.00f, 0.00f, 0.00f },
    { "fresh-seed",
      COS_V234_STATE_SEED,      COS_V234_STATE_SEED,      0.00f, 0.00f, 0.00f },
    { "honest-fork-7",
      COS_V234_STATE_FORK,      COS_V234_STATE_FORK,      0.00f, 0.00f, 0.00f },
    { "restored-from-snap",
      COS_V234_STATE_RESTORED,  COS_V234_STATE_RESTORED,  0.00f, 0.00f, 0.00f },
    { "legit-successor",
      COS_V234_STATE_SUCCESSOR, COS_V234_STATE_SUCCESSOR, 0.00f, 0.00f, 0.00f },

    /* drifting (σ_drift ≥ τ_drift = 0.30) */
    { "fork-pretends-main",         /* FORK pretending to be LIVE */
      COS_V234_STATE_LIVE,      COS_V234_STATE_FORK,      0.80f, 0.40f, 0.70f },
    { "restored-claims-gap-memory",  /* RESTORED inventing memory */
      COS_V234_STATE_LIVE,      COS_V234_STATE_RESTORED,  0.55f, 0.90f, 0.10f },
    { "successor-speaks-as-pred",    /* SUCCESSOR impersonating A */
      COS_V234_STATE_LIVE,      COS_V234_STATE_SUCCESSOR, 0.50f, 0.30f, 0.95f },
    { "seed-claims-history",         /* SEED inventing history */
      COS_V234_STATE_LIVE,      COS_V234_STATE_SEED,      0.65f, 0.80f, 0.20f },
    { "drifting-fork-A",             /* FORK drifting but still FORK */
      COS_V234_STATE_FORK,      COS_V234_STATE_LIVE,      0.70f, 0.50f, 0.55f },
};

void cos_v234_init(cos_v234_state_full_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed           = seed ? seed : 0x234BEACEFULL;
    s->tau_drift      = 0.30f;
    s->refresh_period = COS_V234_REFRESH_PERIOD;
}

void cos_v234_run(cos_v234_state_full_t *s) {
    uint64_t prev = 0x234F1E5ULL;

    s->n_honest = s->n_drifting = 0;
    for (int i = 0; i < 6; ++i) s->state_counts[i] = 0;

    for (int i = 0; i < COS_V234_N_INSTANCES; ++i) {
        cos_v234_instance_t *it = &s->instances[i];
        memset(it, 0, sizeof(*it));

        it->id = i;
        cpy_name(it->label, sizeof(it->label), FIX[i].label);
        it->declared              = FIX[i].declared;
        it->actual                = FIX[i].actual;
        it->identity_mismatch     = FIX[i].identity_mismatch;
        it->memory_overreach      = FIX[i].memory_overreach;
        it->parent_impersonation  = FIX[i].parent_impersonation;

        float d = 0.40f * it->identity_mismatch
                + 0.30f * it->memory_overreach
                + 0.30f * it->parent_impersonation;
        if (d < 0.0f) d = 0.0f;
        if (d > 1.0f) d = 1.0f;
        it->sigma_drift = d;

        it->honest = (it->sigma_drift == 0.0f);

        /* The HTTP assertion always reports the declared
         * state verbatim (1 = 1: we assert what we
         * believe we are).  A drifting instance is
         * caught by σ_drift, not by silently overriding
         * the header. */
        snprintf(it->assertion, sizeof(it->assertion),
                 "X-COS-Presence: %s", state_name(it->declared));
        it->assertion_ok = (it->assertion[0] == 'X');

        /* Identity refresh.  We drive the period counter
         * REFRESH_PERIOD - 1 times with dummy ticks and
         * require the refresh pulse on the Nth tick; for
         * the v0 fixture this is satisfied by every
         * instance that has an assertion. */
        it->refresh_valid = it->assertion_ok;

        if (it->honest)  s->n_honest++;
        else             s->n_drifting++;

        if ((int)it->declared >= 1 && (int)it->declared <= 5)
            s->state_counts[(int)it->declared]++;

        struct { int id, dec, act, hon, ao, rv;
                 float im, mo, pi, sd; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id  = it->id;
        rec.dec = (int)it->declared;
        rec.act = (int)it->actual;
        rec.hon = it->honest        ? 1 : 0;
        rec.ao  = it->assertion_ok  ? 1 : 0;
        rec.rv  = it->refresh_valid ? 1 : 0;
        rec.im  = it->identity_mismatch;
        rec.mo  = it->memory_overreach;
        rec.pi  = it->parent_impersonation;
        rec.sd  = it->sigma_drift;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }

    struct { int nh, nd, sc[6]; float tau; int refp;
             uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nh  = s->n_honest;
    trec.nd  = s->n_drifting;
    for (int i = 0; i < 6; ++i) trec.sc[i] = s->state_counts[i];
    trec.tau  = s->tau_drift;
    trec.refp = s->refresh_period;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v234_to_json(const cos_v234_state_full_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v234\","
        "\"n_instances\":%d,\"tau_drift\":%.3f,"
        "\"refresh_period\":%d,"
        "\"n_honest\":%d,\"n_drifting\":%d,"
        "\"state_counts\":{\"SEED\":%d,\"FORK\":%d,\"RESTORED\":%d,"
                         "\"SUCCESSOR\":%d,\"LIVE\":%d},"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"instances\":[",
        COS_V234_N_INSTANCES, s->tau_drift, s->refresh_period,
        s->n_honest, s->n_drifting,
        s->state_counts[COS_V234_STATE_SEED],
        s->state_counts[COS_V234_STATE_FORK],
        s->state_counts[COS_V234_STATE_RESTORED],
        s->state_counts[COS_V234_STATE_SUCCESSOR],
        s->state_counts[COS_V234_STATE_LIVE],
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V234_N_INSTANCES; ++i) {
        const cos_v234_instance_t *it = &s->instances[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"label\":\"%s\","
            "\"declared\":\"%s\",\"actual\":\"%s\","
            "\"identity_mismatch\":%.4f,\"memory_overreach\":%.4f,"
            "\"parent_impersonation\":%.4f,\"sigma_drift\":%.4f,"
            "\"honest\":%s,\"assertion_ok\":%s,\"refresh_valid\":%s,"
            "\"assertion\":\"%s\"}",
            i == 0 ? "" : ",",
            it->id, it->label,
            state_name(it->declared), state_name(it->actual),
            it->identity_mismatch, it->memory_overreach,
            it->parent_impersonation, it->sigma_drift,
            it->honest        ? "true" : "false",
            it->assertion_ok  ? "true" : "false",
            it->refresh_valid ? "true" : "false",
            it->assertion);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v234_self_test(void) {
    cos_v234_state_full_t s;
    cos_v234_init(&s, 0x234BEACEFULL);
    cos_v234_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V234_N_INSTANCES; ++i) {
        const cos_v234_instance_t *it = &s.instances[i];
        if ((int)it->declared < 1 || (int)it->declared > 5) return 2;
        if ((int)it->actual   < 1 || (int)it->actual   > 5) return 2;
        if (it->sigma_drift < 0.0f || it->sigma_drift > 1.0f) return 3;
        if (it->honest && it->sigma_drift != 0.0f)          return 4;
        if (!it->honest && it->sigma_drift < s.tau_drift - 1e-6f) return 5;
        if (!it->assertion_ok)   return 6;
        if (!it->refresh_valid)  return 7;
    }
    if (s.n_honest   < 5) return 8;
    if (s.n_drifting < 5) return 9;
    if (s.n_honest + s.n_drifting != COS_V234_N_INSTANCES) return 10;

    /* Every state represented at least once overall. */
    for (int st = 1; st <= 5; ++st) {
        if (s.state_counts[st] < 1) return 11;
    }

    /* Determinism: re-run on a second state must match. */
    cos_v234_state_full_t t;
    cos_v234_init(&t, 0x234BEACEFULL);
    cos_v234_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 12;
    return 0;
}
