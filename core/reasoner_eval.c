#include "reasoner.h"

#include <stdio.h>
#include <string.h>

static void load_hundred_triples(void) {
    for (int i = 0; i < 25; i++) {
        char c[48], r[48];
        snprintf(c, sizeof c, "city_%02d", i);
        snprintf(r, sizeof r, "region_%02d", i);
        reasoner_add_triple(c, "is_capital_of", r);
        reasoner_add_triple(r, "is_in", "europe");
    }
    for (int i = 0; i < 25; i++) {
        char s[48], z[48];
        snprintf(s, sizeof s, "spot_%02d", i);
        snprintf(z, sizeof z, "zone_%02d", i);
        reasoner_add_triple(s, "is_in", z);
        reasoner_add_triple(z, "is_in", "earth");
    }
}

typedef struct {
    const char *subj;
    const char *rel;
    const char *obj;
    int         expect_yes;
} rq_t;

int reasoner_run_eval_json(void) {
    reasoner_reset();
    load_hundred_triples();
    int base = reasoner_count();
    reasoner_closure(64);
    int total = reasoner_count();
    int derived = total - base;

    rq_t queries[] = {
        {"city_00", "is_in", "europe", 1},
        {"city_01", "is_in", "europe", 1},
        {"city_02", "is_in", "europe", 1},
        {"city_03", "is_in", "europe", 1},
        {"city_04", "is_in", "europe", 1},
        {"city_10", "is_in", "europe", 1},
        {"city_15", "is_in", "europe", 1},
        {"city_20", "is_in", "europe", 1},
        {"city_23", "is_in", "europe", 1},
        {"city_24", "is_in", "europe", 1},
        {"spot_00", "is_in", "earth", 1},
        {"spot_05", "is_in", "earth", 1},
        {"spot_10", "is_in", "earth", 1},
        {"spot_15", "is_in", "earth", 1},
        {"spot_24", "is_in", "earth", 1},
        {"city_00", "is_in", "mars", 0},
        {"city_00", "is_in", "pluto", 0},
        {"spot_00", "is_in", "mars", 0},
        {"city_00", "is_in", "zone_05", 0},
        {"spot_00", "is_capital_of", "zone_00", 0},
    };

    const int nq = (int)(sizeof queries / sizeof queries[0]);
    int       ok = 0;
    for (int q = 0; q < nq; q++) {
        reasoner_answer_t a = reasoner_query_yesno(queries[q].subj, queries[q].rel, queries[q].obj);
        int yes = (a == REASONER_ANSWER_YES);
        int pass = (yes == queries[q].expect_yes);
        if (pass)
            ok++;
        const char *ans = (a == REASONER_ANSWER_YES) ? "YES" : "UNKNOWN";
        printf(
            "{\"reasoner_q\":true,\"i\":%d,\"subject\":\"%s\",\"relation\":\"%s\",\"object\":\"%s\","
            "\"answer\":\"%s\",\"expect_yes\":%d,\"pass\":%s}\n",
            q + 1, queries[q].subj, queries[q].rel, queries[q].obj, ans, queries[q].expect_yes,
            pass ? "true" : "false");
    }

    printf(
        "{\"reasoner_eval\":true,\"base_triples\":%d,\"derived_triples\":%d,\"total_triples\":%d,"
        "\"queries\":%d,\"queries_pass\":%d}\n",
        base, derived, total, nq, ok);
    return (ok == nq) ? 0 : 1;
}
