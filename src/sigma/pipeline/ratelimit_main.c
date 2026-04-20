/* σ-RateLimit self-test + canonical demo (PROD-2).
 *
 * Demonstrates the four decisions (ALLOWED, THROTTLED_RATE,
 * THROTTLED_GIVE, BLOCKED) on a deterministic 4-peer scenario.
 * The JSON is pinned by benchmarks/sigma_pipeline/check_sigma_ratelimit.sh.
 */

#include "ratelimit.h"

#include <stdio.h>
#include <string.h>

static void run_peer(const char *label, cos_rl_t *rl, const char *id,
                     int n_queries, uint64_t t0, uint64_t step_ns,
                     int *out_allowed, int *out_blocked,
                     int *out_rate, int *out_give,
                     float *out_rep)
{
    int a = 0, b = 0, r = 0, g = 0;
    cos_rl_report_t rep;
    for (int i = 0; i < n_queries; i++) {
        cos_sigma_rl_check(rl, id, t0 + (uint64_t)i * step_ns, &rep);
        switch (rep.decision) {
        case COS_RL_ALLOWED:        a++; break;
        case COS_RL_BLOCKED:        b++; break;
        case COS_RL_THROTTLED_RATE: r++; break;
        case COS_RL_THROTTLED_GIVE: g++; break;
        }
    }
    cos_rl_peer_t *p = cos_sigma_rl_find(rl, id);
    if (out_allowed) *out_allowed = a;
    if (out_blocked) *out_blocked = b;
    if (out_rate)    *out_rate    = r;
    if (out_give)    *out_give    = g;
    if (out_rep)     *out_rep     = p ? p->reputation : 0.0f;
    (void)label;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int rc = cos_sigma_rl_self_test();

    cos_rl_t rl;
    cos_rl_peer_t slots[8];
    cos_sigma_rl_init(&rl, slots, 8,
                      /*qpm=*/5, /*qph=*/60,
                      /*r_ban=*/0.10f, /*r_good=*/0.80f,
                      /*give_min=*/0.10f,
                      /*sigma_reject=*/0.70f,
                      /*evict_grace=*/0);

    uint64_t t = 1000000000ULL;

    /* 1. alice — trusted: 20 low-σ answers before any query. */
    for (int i = 0; i < 20; i++)
        cos_sigma_rl_observe_answer(&rl, "alice", 0.05f, 0, t);
    int a_allowed = 0, a_blocked = 0, a_rate = 0, a_give = 0;
    float a_rep = 0.0f;
    run_peer("alice", &rl, "alice", 8, t + 1, 1, &a_allowed, &a_blocked,
             &a_rate, &a_give, &a_rep);

    /* 2. spammer — burst 10 queries in quick succession (minute cap=5). */
    int s_allowed = 0, s_blocked = 0, s_rate = 0, s_give = 0;
    float s_rep = 0.0f;
    run_peer("spammer", &rl, "spammer", 10, t + 100, 1,
             &s_allowed, &s_blocked, &s_rate, &s_give, &s_rep);

    /* 3. griefer — serves us 10 high-σ answers → reputation collapses. */
    for (int i = 0; i < 10; i++)
        cos_sigma_rl_observe_answer(&rl, "griefer", 0.95f, 0, t + 200);
    int g_allowed = 0, g_blocked = 0, g_rate = 0, g_give = 0;
    float g_rep = 0.0f;
    run_peer("griefer", &rl, "griefer", 3, t + 300, 1,
             &g_allowed, &g_blocked, &g_rate, &g_give, &g_rep);

    /* 4. leech — 12 queries, never serves → give-ratio throttle. */
    int l_allowed = 0, l_blocked = 0, l_rate = 0, l_give = 0;
    float l_rep = 0.0f;
    uint64_t base = t + 10ULL * 60ULL * 1000000000ULL;
    run_peer("leech", &rl, "leech", 12, base, 60ULL * 1000000000ULL,
             &l_allowed, &l_blocked, &l_rate, &l_give, &l_rep);

    printf("{\"kernel\":\"sigma_ratelimit\","
           "\"self_test_rc\":%d,"
           "\"config\":{"
             "\"max_per_minute\":%d,"
             "\"max_per_hour\":%d,"
             "\"r_ban\":%.2f,"
             "\"r_good\":%.2f,"
             "\"give_min\":%.2f,"
             "\"sigma_reject_peer\":%.2f"
           "},"
           "\"peers\":["
             "{\"id\":\"alice\",  \"queries\":8, \"allowed\":%d,\"rate\":%d,\"give\":%d,\"blocked\":%d,\"reputation\":%.3f},"
             "{\"id\":\"spammer\",\"queries\":10,\"allowed\":%d,\"rate\":%d,\"give\":%d,\"blocked\":%d,\"reputation\":%.3f},"
             "{\"id\":\"griefer\",\"queries\":3, \"allowed\":%d,\"rate\":%d,\"give\":%d,\"blocked\":%d,\"reputation\":%.3f},"
             "{\"id\":\"leech\",  \"queries\":12,\"allowed\":%d,\"rate\":%d,\"give\":%d,\"blocked\":%d,\"reputation\":%.3f}"
           "],"
           "\"pass\":%s}\n",
           rc,
           rl.max_per_minute, rl.max_per_hour,
           (double)rl.r_ban, (double)rl.r_good,
           (double)rl.give_min, (double)rl.sigma_reject_peer,
           a_allowed, a_rate, a_give, a_blocked, (double)a_rep,
           s_allowed, s_rate, s_give, s_blocked, (double)s_rep,
           g_allowed, g_rate, g_give, g_blocked, (double)g_rep,
           l_allowed, l_rate, l_give, l_blocked, (double)l_rep,
           rc == 0 ? "true" : "false");
    return rc == 0 ? 0 : 1;
}
