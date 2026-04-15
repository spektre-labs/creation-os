#include "alm.h"

#include "epistemic_drive.h"
#include "learner.h"
#include "metacognition.h"
#include "tri_memory.h"

#include <string.h>

#define COG_LAST_Q 512

static char           g_last_q[COG_LAST_Q];
static alm_response_t g_last_r;
static int            g_last_valid;

void alm_cognitive_reset_session(void) {
    tri_wm_reset();
    meta_reset();
    g_last_valid = 0;
}

void cognitive_after_query(const char *question_raw, alm_response_t *r) {
    if (!r)
        return;
    epistemic_mark_user_activity();
    const char *q = question_raw ? question_raw : "";
    strncpy(g_last_q, q, sizeof g_last_q - 1);
    g_last_q[sizeof g_last_q - 1] = 0;
    g_last_r = *r;
    g_last_valid = 1;
    tri_wm_push(q, r->answer);
    meta_update_from_response(r);
}

void alm_learn_feedback(int accepted) {
    if (!g_last_valid)
        return;
    learner_process_feedback(g_last_q, &g_last_r, accepted);
    g_last_valid = 0;
}

void alm_meta_print_report(void) { meta_print_report(); }
