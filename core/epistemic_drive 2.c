#include "epistemic_drive.h"

#include "alm.h"
#include "facts_store.h"
#include "hdc.h"
#include "metacognition.h"
#include "verifier.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef SUCCESS_ATTESTED_STRICT
#define SUCCESS_ATTESTED_STRICT 1
#endif
#ifndef MISSION_FAILED
#define MISSION_FAILED 0
#endif

static uint64_t g_epi_rng;
static uint32_t epi_rand_u32(void) {
    if (g_epi_rng == 0)
        g_epi_rng = (uint64_t)time(NULL) ^ ((uint64_t)getpid() << 17) ^ 0x9E3779B97F4A7C15ull;
    g_epi_rng ^= g_epi_rng << 13;
    g_epi_rng ^= g_epi_rng >> 7;
    g_epi_rng ^= g_epi_rng << 17;
    return (uint32_t)(g_epi_rng >> 32);
}

#define EPI_SHADOW_CAP 512
/*
 * Goldilocks (σ = normalisoitu Hamming hv_encode-kysymyksille):
 * Alkuperäinen 0.4–0.7 pätee laajalle, melko satunnaiselle HV-joukolle. Nykyinen
 * hv_encode + lyhyet faktakysymykset tuottavat usein σ≈1e-4…1e-2; silloin kapea
 * absoluuttinen väli estää nollan hypoteesivirtaa. Säätö: hylkää vain lähes
 * identtiset ja selvästi kauimmat parit tällä asteikolla.
 */
#define EPI_GOLDILOW 0.00005f
#define EPI_GOLDIHIGH 0.05f
#define EPI_STRICT_SIGMA 0.10f
#define EPI_PAIR_ATTEMPTS 128

static time_t     g_last_user_wall = 0;
static uint64_t   g_shadow_fp[EPI_SHADOW_CAP];
static int        g_shadow_n;

static uint64_t hv_fingerprint(const HV *h) {
    const uint64_t FNV = 14695981039346656037ull;
    const uint64_t PRIME = 1099511628211ull;
    uint64_t       x = FNV;
    for (int i = 0; i < HDC_W; i++) {
        x ^= h->w[i];
        x *= PRIME;
    }
    return x ? x : 1ull;
}

static int shadow_blocked_fp(uint64_t fp) {
    for (int i = 0; i < g_shadow_n; i++) {
        if (g_shadow_fp[i] == fp)
            return 1;
    }
    return 0;
}

static void shadow_add_fp(uint64_t fp) {
    if (shadow_blocked_fp(fp))
        return;
    if (g_shadow_n < EPI_SHADOW_CAP)
        g_shadow_fp[g_shadow_n++] = fp;
    else {
        memmove(g_shadow_fp, g_shadow_fp + 1, (size_t)(EPI_SHADOW_CAP - 1) * sizeof(uint64_t));
        g_shadow_fp[EPI_SHADOW_CAP - 1] = fp;
    }
}

void epistemic_mark_user_activity(void) { g_last_user_wall = time(NULL); }

int epistemic_system_idle(int min_idle_sec) {
    if (min_idle_sec <= 0)
        return 1;
    time_t now = time(NULL);
    if (g_last_user_wall == 0)
        return 1;
    return (now - g_last_user_wall) >= (time_t)min_idle_sec;
}

/*
 * Tyhjyys (holo_recall-korvike): hv_bind(a,b) on usein lähellä jotakin g_hvec[j]:tä
 * (σ≈1e-3), jolloin pelkkä HDC-minimi ei erota "aukkoa". Tarkistetaan ettei pankissa ole
 * jo integroitua epi-paria (normaalin BBHash-haun kautta).
 */
static int epi_pair_not_yet_integrated(const char *k1, const char *k2) {
    char raw[2][MAXQ];
    snprintf(raw[0], sizeof raw[0], "epi|%s|%s", k1, k2);
    snprintf(raw[1], sizeof raw[1], "epi|%s|%s", k2, k1);
    for (int t = 0; t < 2; t++) {
        char qn[MAXQ];
        facts_norm_line(qn, raw[t], sizeof qn);
        uint64_t h = facts_gda_hash(qn);
        int      ix = -1;
        if (facts_bb_lookup(qn, h, &ix))
            return 0;
    }
    return 1;
}

static int causal_llm_disabled(void) {
    const char *e = getenv("ALM_DISABLE_LLM");
    return e && e[0] && strcmp(e, "0") != 0;
}

static int sk9_execute_mission(const char *mission, const char *key_a, const char *key_b) {
    if (causal_llm_disabled())
        return MISSION_FAILED;
    char *raw = alm_llm_generate_filebased(mission);
    if (!raw || !raw[0]) {
        free(raw);
        return MISSION_FAILED;
    }
    char qnorm[MAXQ];
    facts_norm_line(qnorm, mission, sizeof qnorm);
    verify_report_t vr = verify_answer(qnorm, raw);
    int ok = (vr.fact_sigma < EPI_STRICT_SIGMA && vr.fact_check == V_PASS);
    if (ok) {
        char qnew[MAXQ];
        snprintf(qnew, sizeof qnew, "epi|%s|%s", key_a, key_b);
        if (facts_append_pair(qnew, raw) != 0)
            ok = 0;
    }
    free(raw);
    return ok ? SUCCESS_ATTESTED_STRICT : MISSION_FAILED;
}

void epistemic_print_log_json(void) {
    int h, ok, sh, fl;
    float sr;
    meta_epistemic_get_stats(&h, &ok, &sh, &fl, &sr);
    printf("{\"epistemic_log\":true,\"hypotheses\":%d,\"attested_strict\":%d,\"shadowed\":%d,"
           "\"failures\":%d,\"success_rate\":%.4f}\n",
           h, ok, sh, fl, (double)sr);
    fflush(stdout);
}

int epistemic_drive_step_once(void) {
    if (g_nfacts < 2)
        return 0;

    for (int attempt = 0; attempt < EPI_PAIR_ATTEMPTS; attempt++) {
        int idx1 = (int)(epi_rand_u32() % (uint32_t)g_nfacts);
        int idx2 = (int)(epi_rand_u32() % (uint32_t)g_nfacts);
        if (idx2 == idx1)
            idx2 = (idx2 + 1) % g_nfacts;

        float dist = hv_sigma(g_hvec[idx1], g_hvec[idx2]);
        if (dist <= EPI_GOLDILOW || dist >= EPI_GOLDIHIGH)
            continue;

        const char *k1 = g_facts[idx1].q;
        const char *k2 = g_facts[idx2].q;

        HV hypothesis = hv_bind(g_hvec[idx1], g_hvec[idx2]);
        uint64_t fp = hv_fingerprint(&hypothesis);
        if (shadow_blocked_fp(fp))
            continue;

        if (!epi_pair_not_yet_integrated(k1, k2))
            continue;
        meta_epistemic_hypothesis_logged(k1, k2, dist);

        char mission[1024];
        snprintf(mission, sizeof mission,
                 "One factual sentence only: state a verified causal or definitional link between "
                 "concepts A and B. A=\"%s\" B=\"%s\". If uncertain, reply exactly: UNKNOWN.",
                 k1, k2);

        int st = sk9_execute_mission(mission, k1, k2);
        if (st == SUCCESS_ATTESTED_STRICT) {
            meta_epistemic_success_logged();
            return 1;
        }
        meta_epistemic_failure_logged();
        shadow_add_fp(fp);
        meta_epistemic_shadow_logged();
        return 1;
    }
    return 0;
}

int epistemic_drive_run_batch(int steps, int realtime_throttle) {
    if (steps < 1)
        steps = 1;
    for (int s = 0; s < steps; s++) {
        if (!epistemic_system_idle(2)) {
            if (realtime_throttle)
                sleep(1);
            continue;
        }
        (void)epistemic_drive_step_once();
        if (realtime_throttle)
            sleep(10);
    }
    return 0;
}
