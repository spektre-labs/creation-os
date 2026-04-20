/* σ-Marketplace — pay-per-query provider selection with SCSL guard. */

#include "marketplace.h"

#include <string.h>

static float clamp01f(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

int cos_sigma_mkt_init(cos_mkt_t *m,
                       cos_mkt_provider_t *storage, int cap,
                       float ewma_alpha, float sigma_ban_floor)
{
    if (!m || !storage || cap <= 0) return -1;
    if (!(ewma_alpha >= 0.0f && ewma_alpha <= 1.0f))             return -2;
    if (!(sigma_ban_floor > 0.0f && sigma_ban_floor <= 1.0f))    return -3;
    memset(m, 0, sizeof *m);
    m->slots            = storage;
    m->cap              = cap;
    m->ewma_alpha       = ewma_alpha;
    m->sigma_ban_floor  = sigma_ban_floor;
    memset(storage, 0, sizeof(cos_mkt_provider_t) * (size_t)cap);
    return 0;
}

int cos_sigma_mkt_find(const cos_mkt_t *m, const char *id, int *out_idx) {
    if (!m || !id) return -1;
    for (int i = 0; i < m->count; ++i) {
        if (strncmp(m->slots[i].provider_id, id, COS_MKT_ID_CAP - 1) == 0) {
            if (out_idx) *out_idx = i;
            return 0;
        }
    }
    return -2;
}

int cos_sigma_mkt_register(cos_mkt_t *m, const char *id, const char *model,
                           double price, float sig, float lat_ms,
                           int scsl_accepted)
{
    if (!m || !id || !model || id[0] == '\0' || model[0] == '\0') return -1;
    if (price < 0.0)                                                return -2;
    if (!(sig >= 0.0f && sig <= 1.0f))                              return -3;
    if (m->count >= m->cap)                                         return -4;
    if (cos_sigma_mkt_find(m, id, NULL) == 0)                       return -5;
    cos_mkt_provider_t *p = &m->slots[m->count++];
    memset(p, 0, sizeof *p);
    strncpy(p->provider_id, id,    COS_MKT_ID_CAP   - 1);
    strncpy(p->model_name,  model, COS_MKT_MODEL_CAP - 1);
    p->price_per_query = price;
    p->mean_sigma      = sig;
    p->mean_latency_ms = lat_ms < 0.0f ? 0.0f : lat_ms;
    p->scsl_accepted   = scsl_accepted ? 1 : 0;
    p->banned          = 0;
    return 0;
}

int cos_sigma_mkt_ban(cos_mkt_t *m, const char *id, const char *reason) {
    (void)reason;
    int idx = -1;
    if (cos_sigma_mkt_find(m, id, &idx) != 0) return -1;
    m->slots[idx].banned      = 1;
    m->slots[idx].mean_sigma  = 1.0f;
    return 0;
}

int cos_sigma_mkt_unban(cos_mkt_t *m, const char *id) {
    int idx = -1;
    if (cos_sigma_mkt_find(m, id, &idx) != 0) return -1;
    m->slots[idx].banned = 0;
    return 0;
}

const cos_mkt_provider_t *cos_sigma_mkt_select(const cos_mkt_t *m,
                                               double price_budget,
                                               float  sigma_budget,
                                               float  latency_budget_ms)
{
    if (!m || m->count == 0) return NULL;
    const cos_mkt_provider_t *best = NULL;
    for (int i = 0; i < m->count; ++i) {
        const cos_mkt_provider_t *p = &m->slots[i];
        if (!p->scsl_accepted) continue;
        if (p->banned)          continue;
        if (p->price_per_query  > price_budget)      continue;
        if (p->mean_sigma       > sigma_budget)      continue;
        if (p->mean_latency_ms  > latency_budget_ms) continue;
        if (!best) { best = p; continue; }
        /* Cheapest wins.  Ties broken by σ, then latency. */
        if (p->price_per_query  < best->price_per_query) {
            best = p;
        } else if (p->price_per_query == best->price_per_query) {
            if (p->mean_sigma < best->mean_sigma) best = p;
            else if (p->mean_sigma == best->mean_sigma
                     && p->mean_latency_ms < best->mean_latency_ms) {
                best = p;
            }
        }
    }
    return best;
}

int cos_sigma_mkt_report(cos_mkt_t *m, const char *id,
                         float sigma_result, float latency_observed_ms,
                         double cost_eur, int success)
{
    int idx = -1;
    if (cos_sigma_mkt_find(m, id, &idx) != 0) return -1;
    cos_mkt_provider_t *p = &m->slots[idx];
    float a = m->ewma_alpha;
    p->mean_sigma = clamp01f(a * p->mean_sigma
                             + (1.0f - a) * clamp01f(sigma_result));
    if (latency_observed_ms >= 0.0f) {
        p->mean_latency_ms = a * p->mean_latency_ms
                             + (1.0f - a) * latency_observed_ms;
    }
    if (cost_eur > 0.0) {
        p->total_spent_eur   += cost_eur;
        m->total_spend_eur   += cost_eur;
    }
    m->total_queries += 1.0;
    if (success) p->n_successes++; else p->n_failures++;

    /* Auto-ban: σ over the floor AND the provider has more
     * failures than successes on at least 4 attempts.  This lets
     * one-off blips slide but quarantines a systematically
     * unreliable provider. */
    int total = p->n_successes + p->n_failures;
    if (!p->banned && total >= 4 &&
        p->mean_sigma >= m->sigma_ban_floor &&
        p->n_failures > p->n_successes)
    {
        p->banned     = 1;
        p->mean_sigma = 1.0f;
    }
    return 0;
}

/* ---------- self-test ---------- */

static int check_guards(void) {
    cos_mkt_t m;
    cos_mkt_provider_t slots[2];
    if (cos_sigma_mkt_init(NULL, slots, 2, 0.8f, 0.8f) == 0)  return 10;
    if (cos_sigma_mkt_init(&m,   slots, 2, 1.5f, 0.8f) == 0)  return 11;
    if (cos_sigma_mkt_init(&m,   slots, 2, 0.8f, 0.0f) == 0)  return 12;
    return 0;
}

static int check_select_cheapest(void) {
    cos_mkt_t m;
    cos_mkt_provider_t slots[4];
    cos_sigma_mkt_init(&m, slots, 4, 0.80f, 0.70f);
    /* All accept SCSL. */
    cos_sigma_mkt_register(&m, "cheap-a", "m-small",  0.002,  0.25f, 120.0f, 1);
    cos_sigma_mkt_register(&m, "mid-b",   "m-medium", 0.008,  0.12f,  80.0f, 1);
    cos_sigma_mkt_register(&m, "pro-c",   "m-large",  0.020,  0.08f,  60.0f, 1);
    /* SCSL violator: should NEVER be picked. */
    cos_sigma_mkt_register(&m, "banned-d","m-large",  0.001,  0.05f,  40.0f, 0);

    /* Loose budgets: cheapest wins → cheap-a. */
    const cos_mkt_provider_t *p =
        cos_sigma_mkt_select(&m, 0.050, 0.30f, 200.0f);
    if (!p)                                       return 20;
    if (strcmp(p->provider_id, "cheap-a") != 0)   return 21;

    /* Tighten σ budget to 0.10 → only pro-c qualifies. */
    p = cos_sigma_mkt_select(&m, 0.050, 0.10f, 200.0f);
    if (!p)                                       return 22;
    if (strcmp(p->provider_id, "pro-c") != 0)     return 23;

    /* Tighten price budget to 0.005 → only cheap-a under price,
     * but σ budget already excludes it: result NULL. */
    p = cos_sigma_mkt_select(&m, 0.005, 0.10f, 200.0f);
    if (p != NULL)                                return 24;

    /* Even if a non-SCSL provider has the best numbers, we
     * refuse it. */
    p = cos_sigma_mkt_select(&m, 0.002, 0.10f, 200.0f);
    if (p != NULL)                                return 25;

    return 0;
}

static int check_reputation_autoban(void) {
    cos_mkt_t m;
    cos_mkt_provider_t slots[1];
    cos_sigma_mkt_init(&m, slots, 1, 0.50f, 0.80f);
    cos_sigma_mkt_register(&m, "flaky", "m",  0.01, 0.40f, 100.0f, 1);

    /* Five failures with σ_result=0.95 → mean_sigma rises above
     * floor and the provider is auto-banned. */
    for (int i = 0; i < 5; ++i)
        cos_sigma_mkt_report(&m, "flaky", 0.95f, 150.0f, 0.01, 0);

    int idx = -1; cos_sigma_mkt_find(&m, "flaky", &idx);
    if (!slots[idx].banned)                       return 30;
    if (slots[idx].mean_sigma < 0.80f)            return 31;
    if (slots[idx].n_failures != 5)               return 32;

    /* Selection now returns NULL even with generous budgets. */
    const cos_mkt_provider_t *p =
        cos_sigma_mkt_select(&m, 1.0, 1.0f, 5000.0f);
    if (p != NULL)                                return 33;

    /* Unban + fresh σ_report should allow selection again
     * (but σ_capability still high unless many good samples). */
    cos_sigma_mkt_unban(&m, "flaky");
    cos_sigma_mkt_report(&m, "flaky", 0.10f, 80.0f, 0.01, 1);
    /* After one good round mean_sigma is α·1.0 + (1−α)·0.10
     *                                    = 0.5·1.0 + 0.5·0.10 = 0.55. */
    if (!(slots[idx].mean_sigma > 0.50f && slots[idx].mean_sigma < 0.60f))
        return 34;
    return 0;
}

static int check_scsl_ban(void) {
    cos_mkt_t m;
    cos_mkt_provider_t slots[2];
    cos_sigma_mkt_init(&m, slots, 2, 0.80f, 0.80f);
    cos_sigma_mkt_register(&m, "good", "m", 0.005, 0.10f, 50.0f, 1);
    cos_sigma_mkt_register(&m, "ok",   "m", 0.010, 0.15f, 60.0f, 1);
    cos_sigma_mkt_ban(&m, "good", "unauthorised-weapons-use");
    const cos_mkt_provider_t *p =
        cos_sigma_mkt_select(&m, 1.0, 1.0f, 1000.0f);
    if (!p)                                      return 40;
    if (strcmp(p->provider_id, "ok") != 0)       return 41;
    return 0;
}

int cos_sigma_mkt_self_test(void) {
    int rc;
    if ((rc = check_guards()))            return rc;
    if ((rc = check_select_cheapest()))   return rc;
    if ((rc = check_reputation_autoban()))return rc;
    if ((rc = check_scsl_ban()))          return rc;
    return 0;
}
