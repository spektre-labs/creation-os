#include "tri_memory.h"

#include <string.h>
#include <time.h>

static char     g_wm_q[TRI_WM_MAX_EXCH][TRI_WM_LINE];
static char     g_wm_a[TRI_WM_MAX_EXCH][TRI_WM_LINE];
static int      g_wm_n;
static int      g_wm_head;
static time_t   g_wm_ts[TRI_WM_MAX_EXCH];

void tri_wm_reset(void) {
    g_wm_n = 0;
    g_wm_head = 0;
    memset(g_wm_q, 0, sizeof g_wm_q);
    memset(g_wm_a, 0, sizeof g_wm_a);
    memset(g_wm_ts, 0, sizeof g_wm_ts);
}

void tri_wm_push(const char *question, const char *answer) {
    if (!question)
        question = "";
    if (!answer)
        answer = "";
    int i = g_wm_head;
    strncpy(g_wm_q[i], question, sizeof g_wm_q[i] - 1);
    g_wm_q[i][sizeof g_wm_q[i] - 1] = 0;
    strncpy(g_wm_a[i], answer, sizeof g_wm_a[i] - 1);
    g_wm_a[i][sizeof g_wm_a[i] - 1] = 0;
    g_wm_ts[i] = time(NULL);
    g_wm_head = (g_wm_head + 1) % TRI_WM_MAX_EXCH;
    if (g_wm_n < TRI_WM_MAX_EXCH)
        g_wm_n++;
}

float tri_wm_consistency_sigma(const char *answer_norm) {
    (void)answer_norm;
    /* v1: laajennettavissa ristiriitatarkistukseen working memory -renkaasta. */
    return 0.f;
}
