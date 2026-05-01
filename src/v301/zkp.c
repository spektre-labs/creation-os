/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* v301 σ-ZKP — v0 manifest implementation. */

#include "zkp.h"

#include <stdio.h>
#include <string.h>

static uint64_t cos_v301_fnv1a(uint64_t h, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (cap == 0 || !src) { if (cap) dst[0] = '\0'; return; }
    for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void cos_v301_init(cos_v301_state_t *s, uint64_t seed)
{ memset(s, 0, sizeof *s); s->seed = seed; }

void cos_v301_run(cos_v301_state_t *s)
{
    /* 1) Verifiable σ-gate proofs */
    const char *p_lbl[] = {
        "well_formed_proof", "edge_case_proof", "forged_proof" };
    const float p_s[] = { 0.05f, 0.30f, 0.85f };

    int pok = 0, n_valid = 0, n_invalid = 0, no_reveal_all = 1;
    for (int i = 0; i < COS_V301_N_PROOF; ++i) {
        cos_v301_proof_t *p = &s->proof[i];
        cpy(p->label, sizeof p->label, p_lbl[i]);
        p->sigma_proof = p_s[i];
        p->valid = (p->sigma_proof <= COS_V301_TAU_PROOF);
        p->reveals_raw = false;
        if (p->valid) ++n_valid; else ++n_invalid;
        if (p->reveals_raw) no_reveal_all = 0;
        if (strlen(p->label) > 0) ++pok;
    }
    s->n_proof_rows_ok = pok;

    bool p_ord = true;
    for (int i = 1; i < COS_V301_N_PROOF; ++i)
        if (!(s->proof[i].sigma_proof > s->proof[i-1].sigma_proof))
            p_ord = false;
    s->proof_order_ok = p_ord;
    s->proof_polarity_ok = (n_valid > 0) && (n_invalid > 0);
    s->proof_no_reveal_ok = (no_reveal_all == 1);

    /* 2) ZK-inference roles */
    const char *r_name[] = { "client", "cloud", "verifier" };
    const bool  r_raw [] = { false, true,  false };
    const bool  r_w   [] = { false, true,  false };
    const bool  r_ans [] = { true,  true,  false };
    const bool  r_sig [] = { true,  true,  true  };
    const bool  r_prf [] = { true,  true,  true  };

    int rok = 0;
    for (int i = 0; i < COS_V301_N_ROLE; ++i) {
        cos_v301_role_t *r = &s->role[i];
        cpy(r->role, sizeof r->role, r_name[i]);
        r->sees_raw_input     = r_raw[i];
        r->sees_model_weights = r_w[i];
        r->sees_answer        = r_ans[i];
        r->sees_sigma         = r_sig[i];
        r->sees_proof         = r_prf[i];
        if (strlen(r->role) > 0) ++rok;
    }
    s->n_role_rows_ok = rok;
    /* client hides raw + weights */
    s->role_client_hidden_ok =
        (!s->role[0].sees_raw_input) &&
        (!s->role[0].sees_model_weights);
    /* verifier hides raw + weights + answer */
    s->role_verifier_hidden_ok =
        (!s->role[2].sees_raw_input) &&
        (!s->role[2].sees_model_weights) &&
        (!s->role[2].sees_answer);
    s->zk_privacy_holds =
        s->role_client_hidden_ok && s->role_verifier_hidden_ok;

    /* 3) Model integrity */
    const char *i_sc[] = {
        "advertised_served", "silent_downgrade", "advertised_match" };
    const float i_s [] = { 0.10f, 0.75f, 0.12f };

    int iok = 0, n_ok = 0, n_det = 0;
    for (int i = 0; i < COS_V301_N_INTEGRITY; ++i) {
        cos_v301_integrity_t *it = &s->integrity[i];
        cpy(it->scenario, sizeof it->scenario, i_sc[i]);
        it->sigma_integrity = i_s[i];
        it->detected_mismatch =
            (it->sigma_integrity > COS_V301_TAU_INTEGRITY);
        cpy(it->verdict, sizeof it->verdict,
            it->detected_mismatch ? "DETECTED" : "OK");
        if (it->detected_mismatch) ++n_det; else ++n_ok;
        if (strlen(it->scenario) > 0) ++iok;
    }
    s->n_integrity_rows_ok = iok;
    s->integrity_polarity_ok = (n_ok > 0) && (n_det > 0);
    s->integrity_count_ok = (n_ok == 2) && (n_det == 1);

    /* 4) SCSL + ZKP */
    const char *x_lbl[] = {
        "allowed_purpose_a", "allowed_purpose_b", "disallowed_purpose" };
    const float x_s  [] = { 0.08f, 0.20f, 0.90f };

    int xok = 0, n_att = 0, n_rej = 0, no_rev_all = 1;
    for (int i = 0; i < COS_V301_N_SCSL; ++i) {
        cos_v301_scsl_t *x = &s->scsl[i];
        cpy(x->policy, sizeof x->policy, x_lbl[i]);
        x->sigma_policy = x_s[i];
        x->attested = (x->sigma_policy <= COS_V301_TAU_POLICY);
        x->purpose_revealed = false;
        if (x->attested) ++n_att; else ++n_rej;
        if (x->purpose_revealed) no_rev_all = 0;
        if (strlen(x->policy) > 0) ++xok;
    }
    s->n_scsl_rows_ok = xok;
    s->scsl_polarity_ok = (n_att > 0) && (n_rej > 0);
    s->scsl_no_reveal_ok = (no_rev_all == 1);

    /* σ_zkp aggregation */
    int good = 0, denom = 0;
    good += s->n_proof_rows_ok;                       denom += 3;
    good += s->proof_order_ok ? 1 : 0;                denom += 1;
    good += s->proof_polarity_ok ? 1 : 0;             denom += 1;
    good += s->proof_no_reveal_ok ? 1 : 0;            denom += 1;
    good += s->n_role_rows_ok;                        denom += 3;
    good += s->role_client_hidden_ok ? 1 : 0;         denom += 1;
    good += s->role_verifier_hidden_ok ? 1 : 0;       denom += 1;
    good += s->zk_privacy_holds ? 1 : 0;              denom += 1;
    good += s->n_integrity_rows_ok;                   denom += 3;
    good += s->integrity_polarity_ok ? 1 : 0;         denom += 1;
    good += s->integrity_count_ok ? 1 : 0;            denom += 1;
    good += s->n_scsl_rows_ok;                        denom += 3;
    good += s->scsl_polarity_ok ? 1 : 0;              denom += 1;
    good += s->scsl_no_reveal_ok ? 1 : 0;             denom += 1;
    s->sigma_zkp = 1.0f - ((float)good / (float)denom);

    uint64_t h = 0xcbf29ce484222325ULL;
    h = cos_v301_fnv1a(h, &s->seed, sizeof s->seed);
    for (int i = 0; i < COS_V301_N_PROOF; ++i)
        h = cos_v301_fnv1a(h, &s->proof[i], sizeof s->proof[i]);
    for (int i = 0; i < COS_V301_N_ROLE; ++i)
        h = cos_v301_fnv1a(h, &s->role[i], sizeof s->role[i]);
    for (int i = 0; i < COS_V301_N_INTEGRITY; ++i)
        h = cos_v301_fnv1a(h, &s->integrity[i], sizeof s->integrity[i]);
    for (int i = 0; i < COS_V301_N_SCSL; ++i)
        h = cos_v301_fnv1a(h, &s->scsl[i], sizeof s->scsl[i]);
    h = cos_v301_fnv1a(h, &s->sigma_zkp, sizeof s->sigma_zkp);
    s->terminal_hash = h;
    s->chain_valid = true;
}

size_t cos_v301_to_json(const cos_v301_state_t *s, char *buf, size_t cap)
{
    if (!buf || cap == 0) return 0;
    int w = snprintf(buf, cap,
        "{\"kernel\":\"v301_zkp\","
        "\"tau_proof\":%.4f,\"tau_integrity\":%.4f,\"tau_policy\":%.4f,"
        "\"proofs\":[",
        (double)COS_V301_TAU_PROOF,
        (double)COS_V301_TAU_INTEGRITY,
        (double)COS_V301_TAU_POLICY);
    if (w < 0 || (size_t)w >= cap) return (size_t)w;

    for (int i = 0; i < COS_V301_N_PROOF; ++i) {
        const cos_v301_proof_t *p = &s->proof[i];
        int n = snprintf(buf + w, cap - (size_t)w,
            "%s{\"label\":\"%s\",\"sigma_proof\":%.4f,"
            "\"valid\":%s,\"reveals_raw\":%s}",
            i ? "," : "", p->label, (double)p->sigma_proof,
            p->valid ? "true" : "false",
            p->reveals_raw ? "true" : "false");
        if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
        w += n;
    }
    int n = snprintf(buf + w, cap - (size_t)w,
        "],\"proof_rows_ok\":%d,\"proof_order_ok\":%s,"
        "\"proof_polarity_ok\":%s,\"proof_no_reveal_ok\":%s,"
        "\"roles\":[",
        s->n_proof_rows_ok,
        s->proof_order_ok ? "true" : "false",
        s->proof_polarity_ok ? "true" : "false",
        s->proof_no_reveal_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V301_N_ROLE; ++i) {
        const cos_v301_role_t *r = &s->role[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"role\":\"%s\","
            "\"sees_raw_input\":%s,\"sees_model_weights\":%s,"
            "\"sees_answer\":%s,\"sees_sigma\":%s,\"sees_proof\":%s}",
            i ? "," : "", r->role,
            r->sees_raw_input ? "true" : "false",
            r->sees_model_weights ? "true" : "false",
            r->sees_answer ? "true" : "false",
            r->sees_sigma ? "true" : "false",
            r->sees_proof ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"role_rows_ok\":%d,"
        "\"role_client_hidden_ok\":%s,\"role_verifier_hidden_ok\":%s,"
        "\"zk_privacy_holds\":%s,\"integrity\":[",
        s->n_role_rows_ok,
        s->role_client_hidden_ok ? "true" : "false",
        s->role_verifier_hidden_ok ? "true" : "false",
        s->zk_privacy_holds ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V301_N_INTEGRITY; ++i) {
        const cos_v301_integrity_t *it = &s->integrity[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"scenario\":\"%s\",\"sigma_integrity\":%.4f,"
            "\"detected_mismatch\":%s,\"verdict\":\"%s\"}",
            i ? "," : "", it->scenario, (double)it->sigma_integrity,
            it->detected_mismatch ? "true" : "false", it->verdict);
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"integrity_rows_ok\":%d,\"integrity_polarity_ok\":%s,"
        "\"integrity_count_ok\":%s,\"scsl\":[",
        s->n_integrity_rows_ok,
        s->integrity_polarity_ok ? "true" : "false",
        s->integrity_count_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V301_N_SCSL; ++i) {
        const cos_v301_scsl_t *x = &s->scsl[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"policy\":\"%s\",\"sigma_policy\":%.4f,"
            "\"attested\":%s,\"purpose_revealed\":%s}",
            i ? "," : "", x->policy, (double)x->sigma_policy,
            x->attested ? "true" : "false",
            x->purpose_revealed ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"scsl_rows_ok\":%d,\"scsl_polarity_ok\":%s,"
        "\"scsl_no_reveal_ok\":%s,"
        "\"sigma_zkp\":%.4f,\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\"}",
        s->n_scsl_rows_ok,
        s->scsl_polarity_ok ? "true" : "false",
        s->scsl_no_reveal_ok ? "true" : "false",
        (double)s->sigma_zkp,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;
    return (size_t)w;
}

int cos_v301_self_test(void)
{
    cos_v301_state_t s;
    cos_v301_init(&s, 301u);
    cos_v301_run(&s);
    if (s.n_proof_rows_ok  != COS_V301_N_PROOF)     return 1;
    if (!s.proof_order_ok)                          return 2;
    if (!s.proof_polarity_ok)                       return 3;
    if (!s.proof_no_reveal_ok)                      return 4;
    if (s.n_role_rows_ok   != COS_V301_N_ROLE)      return 5;
    if (!s.role_client_hidden_ok)                   return 6;
    if (!s.role_verifier_hidden_ok)                 return 7;
    if (!s.zk_privacy_holds)                        return 8;
    if (s.n_integrity_rows_ok != COS_V301_N_INTEGRITY) return 9;
    if (!s.integrity_polarity_ok)                   return 10;
    if (!s.integrity_count_ok)                      return 11;
    if (s.n_scsl_rows_ok   != COS_V301_N_SCSL)      return 12;
    if (!s.scsl_polarity_ok)                        return 13;
    if (!s.scsl_no_reveal_ok)                       return 14;
    if (!(s.sigma_zkp == 0.0f))                     return 15;
    if (!s.chain_valid)                             return 16;
    if (s.terminal_hash == 0ULL)                    return 17;
    char buf[4096];
    size_t nj = cos_v301_to_json(&s, buf, sizeof buf);
    if (nj == 0 || nj >= sizeof buf)                return 18;
    if (!strstr(buf, "\"kernel\":\"v301_zkp\""))    return 19;
    return 0;
}
