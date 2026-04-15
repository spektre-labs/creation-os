#include "verifier.h"

#include "facts_store.h"
#include "router.h"
#include "hdc.h"
#include "tri_memory.h"

#include <math.h>
#include <string.h>

verify_report_t verify_answer(const char *question_norm, const char *answer_raw) {
    verify_report_t r;
    memset(&r, 0, sizeof r);
    r.fact_check = V_PASS;
    r.consistency = V_PASS;
    r.confidence = V_UNKNOWN;

    char anorm[MAXQ];
    facts_norm_line(anorm, answer_raw ? answer_raw : "", sizeof anorm);

    float ver_dot = -2.f;
    if (g_nfacts > 0 && anorm[0]) {
        HV av = hv_encode(anorm);
        int vh = HDC_D;
        (void)hdc_nearest_among(&av, g_hvec, g_nfacts, &vh);
        ver_dot = hdc_sim_from_hamming(vh);
    }
    r.fact_sigma = (ver_dot > -1.5f) ? fmaxf(0.f, 1.f - ver_dot) : 1.f;
    if (r.fact_sigma < 0.1f)
        r.fact_check = V_PASS;
    else if (r.fact_sigma < 0.5f)
        r.fact_check = V_WEAK;
    else
        r.fact_check = V_FAIL;

    r.cons_sigma = tri_wm_consistency_sigma(anorm);
    if (r.cons_sigma < 0.1f)
        r.consistency = V_PASS;
    else
        r.consistency = V_WEAK;

    float qdot = router_question_nearest_dot(question_norm ? question_norm : "");
    float conf = (qdot < -1.f) ? 1.f : fmaxf(0.f, 1.f - qdot);
    r.conf_sigma = conf;
    if (conf < 0.2f)
        r.confidence = V_PASS;
    else if (conf < 0.8f)
        r.confidence = V_WEAK;
    else
        r.confidence = V_UNKNOWN;

    r.sigma_total = r.fact_sigma + r.cons_sigma + r.conf_sigma;
    return r;
}
