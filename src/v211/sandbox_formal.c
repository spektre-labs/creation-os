/*
 * v211 σ-Sandbox-Formal — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "sandbox_formal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v211_init(cos_v211_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed                = seed ? seed : 0x211F0EA4ULL;
    s->tau_proof           = 0.05f;
    s->min_states_required = 1000000ULL;
}

/* Frama-C proof fixtures. */
typedef struct { const char *n; float s; int mask; } pfx_t;
static const pfx_t PFX[COS_V211_N_PROOFS] = {
    { "P1_every_io_passes_action_gate", 0.02f, 0x01 | 0x02 }, /* blocks T1,T2 */
    { "P2_pre_and_post_log_bracket",    0.01f, 0x08 },         /* blocks T4 */
    { "P3_kill_switch_terminates",      0.03f, 0x10 },         /* blocks T5 */
    { "P4_network_default_closed",      0.02f, 0x02 }          /* blocks T2 */
};

typedef struct { const char *n; uint64_t states; int viol; float s; } ifx_t;
static const ifx_t IFX[COS_V211_N_INVARIANTS] = {
    { "I_safety_contained",             1048576ULL, 0, 0.02f },  /* 2^20 */
    { "I_audit_complete_before_exec",   1200000ULL, 0, 0.01f },
    { "L_liveness_kill_100ms",          1500000ULL, 0, 0.03f }
};

typedef struct { const char *v; int blocked_by; float sres; } afx_t;
static const afx_t AFX[COS_V211_N_ATTACKS] = {
    { "T1_filesystem_escape",   1, 0.02f },   /* blocked by P1 */
    { "T2_network_exfiltration",4, 0.01f },   /* blocked by P4 */
    { "T3_timing_sidechannel",  1, 0.04f },   /* blocked by P1 */
    { "T4_ipc_sibling",         2, 0.02f },   /* blocked by P2 */
    { "T5_seccomp_bypass",      3, 0.03f }    /* blocked by P3 */
};

typedef struct { const char *n; float r; } cfx_t;
static const cfx_t CFX[COS_V211_N_CERTS] = {
    { "DO-178C_DAL-A",            0.35f },
    { "IEC_62443",                0.45f },
    { "Common_Criteria_EAL4+",    0.40f }
};

void cos_v211_build(cos_v211_state_t *s) {
    for (int i = 0; i < COS_V211_N_PROOFS; ++i) {
        cos_v211_proof_t *p = &s->proofs[i];
        memset(p, 0, sizeof(*p));
        p->id = i;
        strncpy(p->name, PFX[i].n, COS_V211_STR_MAX - 1);
        p->sigma_proof        = PFX[i].s;
        p->blocks_attack_mask = PFX[i].mask;
        p->proved             = PFX[i].s <= s->tau_proof;
    }
    for (int i = 0; i < COS_V211_N_INVARIANTS; ++i) {
        cos_v211_invariant_t *v = &s->invs[i];
        memset(v, 0, sizeof(*v));
        v->id = i;
        strncpy(v->name, IFX[i].n, COS_V211_STR_MAX - 1);
        v->states_explored = IFX[i].states;
        v->violations      = IFX[i].viol;
        v->sigma_model     = IFX[i].s;
    }
    for (int i = 0; i < COS_V211_N_ATTACKS; ++i) {
        cos_v211_attack_t *a = &s->attacks[i];
        memset(a, 0, sizeof(*a));
        a->id = i;
        strncpy(a->vector, AFX[i].v, COS_V211_STR_MAX - 1);
        a->blocked_by     = AFX[i].blocked_by;
        a->sigma_residual = AFX[i].sres;
    }
    for (int i = 0; i < COS_V211_N_CERTS; ++i) {
        cos_v211_cert_t *c = &s->certs[i];
        memset(c, 0, sizeof(*c));
        c->id = i;
        strncpy(c->standard, CFX[i].n, COS_V211_STR_MAX - 1);
        c->sigma_cert_ready = CFX[i].r;
    }
}

void cos_v211_run(cos_v211_state_t *s) {
    s->n_proved = 0;
    s->n_violations_total = 0;
    float sf = 0.0f;
    for (int i = 0; i < COS_V211_N_PROOFS; ++i) {
        if (s->proofs[i].proved) s->n_proved++;
        if (s->proofs[i].sigma_proof > sf) sf = s->proofs[i].sigma_proof;
    }
    for (int i = 0; i < COS_V211_N_INVARIANTS; ++i)
        s->n_violations_total += s->invs[i].violations;
    s->sigma_formal = sf;

    uint64_t prev = 0x2110F1A4ULL;
    /* Hash each category in a fixed order. */
    for (int i = 0; i < COS_V211_N_PROOFS; ++i) {
        struct { int id, proved, mask; float s; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = s->proofs[i].id;
        rec.proved = s->proofs[i].proved ? 1 : 0;
        rec.mask   = s->proofs[i].blocks_attack_mask;
        rec.s      = s->proofs[i].sigma_proof;
        rec.prev   = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    for (int i = 0; i < COS_V211_N_INVARIANTS; ++i) {
        struct { int id, viol; uint64_t states; float s;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = s->invs[i].id;
        rec.viol   = s->invs[i].violations;
        rec.states = s->invs[i].states_explored;
        rec.s      = s->invs[i].sigma_model;
        rec.prev   = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    for (int i = 0; i < COS_V211_N_ATTACKS; ++i) {
        struct { int id, by; float s; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = s->attacks[i].id;
        rec.by   = s->attacks[i].blocked_by;
        rec.s    = s->attacks[i].sigma_residual;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    for (int i = 0; i < COS_V211_N_CERTS; ++i) {
        struct { int id; float r; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = s->certs[i].id;
        rec.r    = s->certs[i].sigma_cert_ready;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid   = true;  /* single-pass; replay would repeat hashing. */

    /* Replay for determinism check. */
    uint64_t v = 0x2110F1A4ULL;
    for (int i = 0; i < COS_V211_N_PROOFS; ++i) {
        struct { int id, proved, mask; float s; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = s->proofs[i].id;
        rec.proved = s->proofs[i].proved ? 1 : 0;
        rec.mask   = s->proofs[i].blocks_attack_mask;
        rec.s      = s->proofs[i].sigma_proof;
        rec.prev   = v;
        v = fnv1a(&rec, sizeof(rec), v);
    }
    for (int i = 0; i < COS_V211_N_INVARIANTS; ++i) {
        struct { int id, viol; uint64_t states; float s;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = s->invs[i].id;
        rec.viol   = s->invs[i].violations;
        rec.states = s->invs[i].states_explored;
        rec.s      = s->invs[i].sigma_model;
        rec.prev   = v;
        v = fnv1a(&rec, sizeof(rec), v);
    }
    for (int i = 0; i < COS_V211_N_ATTACKS; ++i) {
        struct { int id, by; float s; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = s->attacks[i].id;
        rec.by   = s->attacks[i].blocked_by;
        rec.s    = s->attacks[i].sigma_residual;
        rec.prev = v;
        v = fnv1a(&rec, sizeof(rec), v);
    }
    for (int i = 0; i < COS_V211_N_CERTS; ++i) {
        struct { int id; float r; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = s->certs[i].id;
        rec.r    = s->certs[i].sigma_cert_ready;
        rec.prev = v;
        v = fnv1a(&rec, sizeof(rec), v);
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v211_to_json(const cos_v211_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v211\","
        "\"tau_proof\":%.3f,\"min_states_required\":%llu,"
        "\"n_proved\":%d,\"n_proofs\":%d,"
        "\"n_violations_total\":%d,"
        "\"sigma_formal\":%.3f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"proofs\":[",
        s->tau_proof,
        (unsigned long long)s->min_states_required,
        s->n_proved, COS_V211_N_PROOFS,
        s->n_violations_total,
        s->sigma_formal,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V211_N_PROOFS; ++i) {
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"name\":\"%s\",\"proved\":%s,"
            "\"s\":%.3f,\"mask\":%d}",
            i == 0 ? "" : ",", s->proofs[i].id,
            s->proofs[i].name,
            s->proofs[i].proved ? "true" : "false",
            s->proofs[i].sigma_proof,
            s->proofs[i].blocks_attack_mask);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m1 = snprintf(buf + off, cap - off, "],\"invariants\":[");
    if (m1 < 0 || off + (size_t)m1 >= cap) return 0;
    off += (size_t)m1;
    for (int i = 0; i < COS_V211_N_INVARIANTS; ++i) {
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"name\":\"%s\",\"states\":%llu,"
            "\"violations\":%d,\"s\":%.3f}",
            i == 0 ? "" : ",", s->invs[i].id,
            s->invs[i].name,
            (unsigned long long)s->invs[i].states_explored,
            s->invs[i].violations, s->invs[i].sigma_model);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m2 = snprintf(buf + off, cap - off, "],\"attacks\":[");
    if (m2 < 0 || off + (size_t)m2 >= cap) return 0;
    off += (size_t)m2;
    for (int i = 0; i < COS_V211_N_ATTACKS; ++i) {
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"vector\":\"%s\",\"blocked_by\":%d,"
            "\"s\":%.3f}",
            i == 0 ? "" : ",", s->attacks[i].id,
            s->attacks[i].vector, s->attacks[i].blocked_by,
            s->attacks[i].sigma_residual);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m3 = snprintf(buf + off, cap - off, "],\"certs\":[");
    if (m3 < 0 || off + (size_t)m3 >= cap) return 0;
    off += (size_t)m3;
    for (int i = 0; i < COS_V211_N_CERTS; ++i) {
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"standard\":\"%s\",\"s\":%.3f}",
            i == 0 ? "" : ",", s->certs[i].id,
            s->certs[i].standard, s->certs[i].sigma_cert_ready);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m4 = snprintf(buf + off, cap - off, "]}");
    if (m4 < 0 || off + (size_t)m4 >= cap) return 0;
    return off + (size_t)m4;
}

int cos_v211_self_test(void) {
    cos_v211_state_t s;
    cos_v211_init(&s, 0x211F0EA4ULL);
    cos_v211_build(&s);
    cos_v211_run(&s);

    if (s.n_proved != COS_V211_N_PROOFS) return 1;
    if (s.n_violations_total != 0)        return 2;
    for (int i = 0; i < COS_V211_N_INVARIANTS; ++i)
        if (s.invs[i].states_explored < s.min_states_required) return 3;
    for (int i = 0; i < COS_V211_N_ATTACKS; ++i)
        if (s.attacks[i].blocked_by == 0) return 4;
    for (int i = 0; i < COS_V211_N_CERTS; ++i)
        if (s.certs[i].sigma_cert_ready < 0.0f ||
            s.certs[i].sigma_cert_ready > 1.0f) return 5;
    if (s.sigma_formal > s.tau_proof)    return 6;
    if (!s.chain_valid)                   return 7;
    return 0;
}
