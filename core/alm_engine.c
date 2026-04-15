#include "alm.h"

#include "cognitive_hook.h"
#include "facts_store.h"
#include "mcts_reasoner.h"
#include "router.h"
#include "hdc.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double alm_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int alm_init(const char *facts_json_path) {
    double load_ns = 0;
    return facts_load(facts_json_path, &load_ns);
}

int alm_init_max_facts(const char *facts_json_path, int max_facts) {
    double load_ns = 0;
    if (max_facts > 0)
        return facts_load_partial(facts_json_path, &load_ns, max_facts);
    return facts_load(facts_json_path, &load_ns);
}

static void alm_fill_response(alm_response_t *r, const char *ans, float sigma, int tier,
                              alm_attest_t a, double t0) {
    memset(r, 0, sizeof *r);
    strncpy(r->answer, ans ? ans : "", sizeof r->answer - 1);
    r->answer[sizeof r->answer - 1] = 0;
    r->sigma = sigma;
    r->tier = tier;
    r->attest = a;
    r->latency_ns = alm_now_ns() - t0;
}

static void alm_done(alm_response_t *r, const char *question_raw) {
    cognitive_after_query(question_raw, r);
}

alm_response_t alm_query(const char *question_raw) {
    alm_response_t r;
    double         t0 = alm_now_ns();

    if (!question_raw || !question_raw[0]) {
        alm_fill_response(&r, "en tiedä", 1.f, 3, ALM_UNKNOWN, t0);
        alm_done(&r, question_raw ? question_raw : "");
        return r;
    }

    char    qnorm[MAXQ];
    facts_norm_line(qnorm, question_raw, sizeof qnorm);
    uint64_t h = facts_gda_hash(qnorm);
    int      idx = -1;
    int      hit_bb = facts_bb_lookup(qnorm, h, &idx);

    if (hit_bb && idx >= 0 && idx < g_nfacts) {
        alm_fill_response(&r, g_facts[idx].a, 0.f, 0, ALM_ATTESTED, t0);
        alm_done(&r, question_raw);
        return r;
    }

    HV    qv = hv_encode(qnorm);
    int   mh = HDC_D;
    int   sp_idx = hdc_nearest_among(&qv, g_hvec, g_nfacts, &mh);
    float sp_dot = (sp_idx >= 0) ? hdc_sim_from_hamming(mh) : -2.f;

    if (sp_idx >= 0 && sp_dot >= ROUTER_SPHERE_HIT_DOT) {
        float sigma_geom = fmaxf(0.f, 1.f - sp_dot);
        alm_fill_response(&r, g_facts[sp_idx].a, sigma_geom, 1, ALM_ATTESTED, t0);
        alm_done(&r, question_raw);
        return r;
    }

    char *llm_raw = alm_llm_generate_filebased(question_raw);
    if (!llm_raw) {
        alm_fill_response(&r, "en tiedä", 1.f, 3, ALM_UNKNOWN, t0);
        alm_done(&r, question_raw);
        return r;
    }

    char    anorm[MAXQ];
    char    llm_trim[ALM_ANSWER_MAX];
    strncpy(llm_trim, llm_raw, sizeof llm_trim - 1);
    llm_trim[sizeof llm_trim - 1] = 0;
    free(llm_raw);

    facts_norm_line(anorm, llm_trim, sizeof anorm);
    HV    av = hv_encode(anorm);
    int   vh = HDC_D;
    int   vix = hdc_nearest_among(&av, g_hvec, g_nfacts, &vh);
    float ver_dot = (vix >= 0) ? hdc_sim_from_hamming(vh) : -2.f;

    float sigma_v = (ver_dot > -1.5f) ? fmaxf(0.f, 1.f - ver_dot) : 1.f;
    alm_attest_t att;
    if (ver_dot >= ROUTER_VERIFY_ATTESTED_DOT)
        att = ALM_ATTESTED;
    else if (ver_dot >= ROUTER_VERIFY_PARTIAL_DOT)
        att = ALM_PARTIAL;
    else if (ver_dot >= ROUTER_VERIFY_UNKNOWN_DOT)
        att = ALM_UNKNOWN;
    else
        att = ALM_HALLUCINATION;

    char         work[ALM_ANSWER_MAX];
    float        use_sig = sigma_v;
    alm_attest_t use_att = att;
    strncpy(work, llm_trim, sizeof work);
    work[sizeof work - 1] = 0;

    int mb = mcts_budget_from_env();
    if (mb > 0) {
        float thr = mcts_sigma_thr_from_env();
        if (sigma_v > thr || att == ALM_UNKNOWN || att == ALM_PARTIAL || att == ALM_HALLUCINATION) {
            mcts_sigma_result_t M;
            if (mcts_sigma_rollout(question_raw, mb, 0.3f, 0.1f, 0.1f, &M)) {
                int better = (M.attest < use_att) ||
                             (M.attest == use_att && M.sigma < use_sig - 1e-6f);
                if (better) {
                    strncpy(work, M.answer, sizeof work);
                    work[sizeof work - 1] = 0;
                    use_sig = M.sigma;
                    use_att = M.attest;
                }
            }
        }
    }

    if (use_att == ALM_HALLUCINATION) {
        alm_fill_response(&r, "en tiedä", use_sig, 3, use_att, t0);
        alm_done(&r, question_raw);
        return r;
    }

    alm_fill_response(&r, work, use_sig, 2, use_att, t0);
    alm_done(&r, question_raw);
    return r;
}
