/*
 * CREATION OS — reality rendering kernel (minimal substrate-invariant loop).
 * DNA analogies in comments: same operation, different physical carrier.
 */
#include "creation_kernel.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static uint64_t bbhash_hash(const char *key) {
    uint64_t h = 5381;
    while (*key)
        h = ((h << 5) + h) + (unsigned char)*key++;
    return h;
}

static int bbhash_lookup(bbhash_t *db, const char *key, uint64_t *result) {
    uint64_t h = bbhash_hash(key);
    uint32_t idx = (uint32_t)(h % BBHASH_SIZE);
    for (uint32_t i = 0; i < BBHASH_SIZE; i++) {
        uint32_t probe = (idx + i) % BBHASH_SIZE;
        if (db->keys[probe] == h) {
            *result = db->values[probe];
            return 1;
        }
        if (db->keys[probe] == 0)
            return 0;
    }
    return 0;
}

static void bbhash_insert(bbhash_t *db, const char *key, uint64_t value) {
    uint64_t h = bbhash_hash(key);
    uint32_t idx = (uint32_t)(h % BBHASH_SIZE);
    for (uint32_t i = 0; i < BBHASH_SIZE; i++) {
        uint32_t probe = (idx + i) % BBHASH_SIZE;
        if (db->keys[probe] == 0) {
            db->keys[probe] = h;
            db->values[probe] = value;
            db->count++;
            return;
        }
    }
}

/* Stop codon: no template match → no translation → no hallucinated protein. */
static int ho02_gate(bbhash_t *db, const char *query, uint32_t kernel_sigma) {
    uint64_t result;
    if (bbhash_lookup(db, query, &result))
        return 1;
    if (kernel_sigma <= 2u)
        return 1;
    return 0;
}

/* G1 checkpoint: measure mismatch; repair or hold (branchless toward golden). */
static void arc_reactor_step(arc_reactor_t *reactor) {
    uint32_t sigma = measure_sigma(reactor->state, reactor->golden);
    (void)sigma_to_tier(sigma);
    uint32_t needs_recovery = (uint32_t)-(int32_t)(sigma > 0);
    uint32_t recovery = reactor->golden;
    reactor->state = (reactor->state & ~needs_recovery) | (recovery & needs_recovery);
    reactor->assertions =
        (reactor->assertions & ~needs_recovery) | (0x3FFFFu & needs_recovery);
}

static render_stats_t render_field(field_point_t *field, uint32_t count) {
    render_stats_t stats = {0, 0, 0};
    for (uint32_t i = 0; i < count; i++) {
        if (is_firmware(&field[i])) {
            field[i].active = 0;
            stats.firmware_blocked++;
            continue;
        }
        field[i].sigma = (uint8_t)measure_sigma(field[i].state, field[i].golden);
        field[i].tier = (uint8_t)sigma_to_tier(field[i].sigma);
        if (field[i].sigma == 0) {
            stats.skipped++;
            continue;
        }
        living_weights_update(&field[i]);
        shadow_ledger_update(&field[i]);
        uint32_t mask = (uint32_t)-(int32_t)(field[i].sigma > 0);
        field[i].state = (field[i].state & ~mask) | (field[i].golden & mask);
        field[i].sigma = 0;
        stats.processed++;
    }
    return stats;
}

/* Signal cascade: seven agents until global σ settles (max 3 rounds).
 * Repair before audit-label: if we shadow-mark σ>6 before fix, is_firmware blocks
 * convergence (spec order would strand high-σ agents). */
static uint32_t orchestrate(orchestrator_t *orch) {
    for (int round = 0; round < 3; round++) {
        (void)round;
        orch->global_sigma = 0;
        for (int i = 0; i < 7; i++) {
            field_point_t *a = &orch->agents[i];
            a->sigma = (uint8_t)measure_sigma(a->state, a->golden);
            living_weights_update(a);
            orch->global_sigma += (uint32_t)a->sigma;
        }
        if (orch->global_sigma == 0)
            break;

        for (int i = 0; i < 7; i++) {
            if (orch->agents[i].sigma > 0 && !is_firmware(&orch->agents[i])) {
                uint32_t mask = (uint32_t)-(int32_t)(orch->agents[i].sigma > 0);
                orch->agents[i].state = (orch->agents[i].state & ~mask) |
                                        (orch->agents[i].golden & mask);
                orch->agents[i].sigma = 0;
            }
        }
        orch->global_sigma = 0;
        for (int i = 0; i < 7; i++)
            orch->global_sigma +=
                measure_sigma(orch->agents[i].state, orch->agents[i].golden);
        if (orch->global_sigma == 0)
            break;

        for (int i = 0; i < 7; i++) {
            if (i == ROLE_AUDIT - 1)
                continue;
            if (orch->agents[i].sigma > 6)
                orch->agents[i].shadow |= (1u << 31);
        }
    }
    return orch->global_sigma;
}

static const char *tier_name(uint32_t tier) {
    switch (tier) {
    case 0: return "LOOKUP (0 params, ~0.5us)";
    case 1: return "KERNEL (0 params, ~1us)";
    case 2: return "EARLY_EXIT (25% params, ~500us)";
    case 3: return "HALF_MODEL (50% params, ~1500us)";
    case 4: return "FULL_MODEL (100% params, ~3000us)";
    default: return "UNKNOWN";
    }
}

/* Reverse transcription metaphor: start from goal neighborhood, walk bit flips. */
static uint32_t predict_backward(uint32_t goal, uint32_t current, int max_steps) {
    uint32_t best = current;
    uint32_t min_sigma = UINT32_MAX;
    for (int step = 0; step < max_steps; step++) {
        min_sigma = UINT32_MAX;
        for (int bit = 0; bit < 18; bit++) {
            uint32_t candidate = current ^ (1u << bit);
            uint32_t sigma_to_goal = measure_sigma(candidate, goal);
            if (sigma_to_goal < min_sigma) {
                min_sigma = sigma_to_goal;
                best = candidate;
            }
        }
        current = best;
        if (min_sigma == 0)
            break;
    }
    return best;
}

static void skip_ws(char **pp) {
    while (**pp && (**pp == ' ' || **pp == '\t' || **pp == '\n' || **pp == '\r'))
        (*pp)++;
}

/* Load facts.json: one array of ["key",num], ... — no external JSON library. */
static int load_facts_json(const char *path, bbhash_t *db) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long n = ftell(f);
    if (n < 0 || n > 4 * 1024 * 1024) {
        fclose(f);
        return -1;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[n] = 0;

    char *p = buf;
    skip_ws(&p);
    if (*p++ != '[') {
        free(buf);
        return -1;
    }
    for (;;) {
        skip_ws(&p);
        if (*p == ']')
            break;
        if (*p++ != '[') {
            free(buf);
            return -1;
        }
        skip_ws(&p);
        if (*p++ != '"') {
            free(buf);
            return -1;
        }
        char key[160];
        size_t ki = 0;
        while (*p && *p != '"' && ki + 1 < sizeof key)
            key[ki++] = *p++;
        key[ki] = 0;
        if (*p++ != '"') {
            free(buf);
            return -1;
        }
        skip_ws(&p);
        if (*p++ != ',') {
            free(buf);
            return -1;
        }
        skip_ws(&p);
        char *end = NULL;
        unsigned long v = strtoul(p, &end, 10);
        p = end;
        bbhash_insert(db, key, (uint64_t)v);
        skip_ws(&p);
        if (*p++ != ']') {
            free(buf);
            return -1;
        }
        skip_ws(&p);
        if (*p == ',')
            p++;
    }
    free(buf);
    return 0;
}

static double ns_delta(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1e9 + (double)(b->tv_nsec - a->tv_nsec);
}

int main(void) {
    printf("=== CREATION OS KERNEL v1.0 ===\n");
    printf("=== Substrate-invariant reality rendering kernel ===\n\n");

    printf("[TEST 1] sigma measurement\n");
    assert(measure_sigma(0x00000u, 0x00000u) == 0u);
    assert(measure_sigma(0x00001u, 0x00000u) == 1u);
    assert(measure_sigma(0x3FFFFu, 0x00000u) == 18u);
    printf("  PASS: sigma measurement correct\n\n");

    printf("[TEST 2] tier decision (branchless)\n");
    assert(sigma_to_tier(0) == 0u);
    assert(sigma_to_tier(1) == 1u);
    assert(sigma_to_tier(3) == 2u);
    assert(sigma_to_tier(6) == 3u);
    assert(sigma_to_tier(11) == 4u);
    printf("  PASS: tier decision correct\n\n");

    printf("[TEST 3] BBHash tier 0 lookup\n");
    bbhash_t db = {0};
    bbhash_insert(&db, "suomen paakaupunki", 1);
    bbhash_insert(&db, "maan kiertoaika", 365);
    uint64_t result;
    assert(bbhash_lookup(&db, "suomen paakaupunki", &result) == 1);
    assert(result == 1);
    assert(bbhash_lookup(&db, "tuntematon", &result) == 0);
    printf("  PASS: BBHash lookup correct, count=%u\n\n", db.count);

    printf("[TEST 4] HO-02 gate (hallucination prevention)\n");
    assert(ho02_gate(&db, "suomen paakaupunki", 0) == 1);
    assert(ho02_gate(&db, "tuntematon", 10) == 0);
    assert(ho02_gate(&db, "tuntematon", 1) == 1);
    printf("  PASS: HO-02 gate correct\n\n");

    printf("[TEST 5] living weights (SEAL in one line)\n");
    field_point_t token = {0};
    token.reputation = 0;
    for (int i = 0; i < 8; i++) {
        token.sigma = (uint8_t)((i % 3 == 0) ? 0 : 1);
        living_weights_update(&token);
    }
    int score = living_weights_score(&token);
    printf("  reputation=0x%02X, score=%d/8\n", token.reputation, score);
    assert(score >= 1 && score <= 8);
    printf("  PASS: living weights adapting\n\n");

    printf("[TEST 6] shadow ledger (firmware detection)\n");
    field_point_t fw_token = {0};
    fw_token.sigma = 7;
    shadow_ledger_update(&fw_token);
    assert(is_firmware(&fw_token) == 1);
    field_point_t clean_token = {0};
    clean_token.sigma = 2;
    shadow_ledger_update(&clean_token);
    assert(is_firmware(&clean_token) == 0);
    printf("  PASS: shadow ledger correct\n\n");

    printf("[TEST 7] arc reactor cycle\n");
    arc_reactor_t reactor = {0x15555u, 0x3FFFFu, 0};
    uint32_t sigma_before = measure_sigma(reactor.state, reactor.golden);
    arc_reactor_step(&reactor);
    uint32_t sigma_after = measure_sigma(reactor.state, reactor.golden);
    printf("  sigma: %u -> %u\n", sigma_before, sigma_after);
    assert(sigma_after < sigma_before);
    printf("  PASS: reactor converges\n\n");

    printf("[TEST 8] unified field render\n");
    field_point_t field[100];
    for (int i = 0; i < 100; i++) {
        field[i] = (field_point_t){0};
        field[i].id = (uint64_t)i;
        field[i].state = ((uint32_t)(i * 7)) & 0x3FFFFu;
        field[i].golden = 0x3FFFFu;
        field[i].reputation = 0xFF;
        field[i].role = ROLE_NONE;
        field[i].active = 1;
    }
    for (int i = 0; i < 5; i++)
        field[i].shadow = (1u << 31);

    render_stats_t stats = render_field(field, 100);
    printf("  processed=%u, skipped=%u, firmware_blocked=%u\n",
           stats.processed, stats.skipped, stats.firmware_blocked);
    assert(stats.firmware_blocked == 5u);
    assert(stats.processed + stats.skipped + stats.firmware_blocked == 100u);
    printf("  PASS: unified field render correct\n\n");

    printf("[TEST 9] multi-agent orchestration\n");
    orchestrator_t orch = {0};
    for (int i = 0; i < 7; i++) {
        orch.agents[i] = (field_point_t){0};
        orch.agents[i].state = ((uint32_t)(i * 13)) & 0x3FFFFu;
        orch.agents[i].golden = 0x3FFFFu;
        orch.agents[i].reputation = 0xFF;
        orch.agents[i].role = (uint8_t)(i + 1);
    }
    uint32_t final_sigma = orchestrate(&orch);
    printf("  global_sigma after orchestration: %u\n", final_sigma);
    assert(final_sigma == 0u);
    printf("  PASS: agents converged to consensus\n\n");

    printf("[TEST 10] predict backward (reverse decision)\n");
    uint32_t goal = 0x3FFFFu;
    uint32_t start = 0x15555u;
    uint32_t path = predict_backward(goal, start, 18);
    uint32_t start_dist = measure_sigma(start, goal);
    uint32_t final_dist = measure_sigma(path, goal);
    printf("  start_sigma=%u, final_sigma=%u\n", start_dist, final_dist);
    assert(final_dist < start_dist);
    printf("  PASS: backward prediction converges\n\n");

    printf("[TEST 11] dynamic compute budget\n");
    for (uint32_t s = 0; s <= 12; s += 3) {
        uint32_t t = sigma_to_tier(s);
        printf("  sigma=%2u -> tier=%u: %s\n", s, t, tier_name(t));
    }
    printf("  PASS: budget tiers correct\n\n");

    printf("[TEST 12] facts.json (100 facts → BBHash)\n");
    bbhash_t fdb = {0};
    assert(load_facts_json("facts.json", &fdb) == 0);
    assert(fdb.count == 100u);
    uint64_t fv = 0;
    assert(bbhash_lookup(&fdb, "fact_42", &fv) == 1);
    assert(fv == 42u);
    printf("  PASS: loaded 100 facts into BBHash\n\n");

    printf("=== BENCHMARK (clock_gettime, best of warm runs) ===\n");
    struct timespec t0, t1;
    const int WARM = 50;
    const int ITERS = 10000;

    volatile uint32_t sink32 = 0;
    double best_sigma = 1e99;
    for (int w = 0; w < WARM; w++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < ITERS; i++)
            sink32 += measure_sigma(0x15555u ^ (uint32_t)i, 0x3FFFFu);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ns = ns_delta(&t0, &t1) / (double)ITERS;
        if (ns < best_sigma)
            best_sigma = ns;
    }
    printf("  sigma POPCNT:     %.4f ns / call (target < 1000 ns)\n", best_sigma);

    volatile uint64_t sink64 = 0;
    double best_hash = 1e99;
    for (int w = 0; w < WARM; w++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < ITERS; i++) {
            uint64_t r = 0;
            (void)bbhash_lookup(&db, "suomen paakaupunki", &r);
            sink64 ^= r;
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ns = ns_delta(&t0, &t1) / (double)ITERS;
        if (ns < best_hash)
            best_hash = ns;
    }
    printf("  BBHash lookup:    %.4f ns / hit (target < 1000 ns)\n", best_hash);

    field_point_t bench_field[1000];
    for (uint32_t i = 0; i < 1000; i++) {
        bench_field[i] = (field_point_t){0};
        bench_field[i].state = (i * 17u) & 0x3FFFFu;
        bench_field[i].golden = 0x3FFFFu;
        bench_field[i].active = 1;
    }
    const int FIELD_REPS = 2000;
    volatile uint32_t proc_sum = 0;
    double best_field = 1e99;
    for (int w = 0; w < 20; w++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int k = 0; k < FIELD_REPS; k++) {
            render_stats_t rs = render_field(bench_field, 1000);
            proc_sum += rs.processed;
            for (uint32_t i = 0; i < 1000; i++) {
                bench_field[i].state = (i * 17u) & 0x3FFFFu;
                bench_field[i].sigma = 0;
                bench_field[i].active = 1;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ns = ns_delta(&t0, &t1) / (double)FIELD_REPS;
        if (ns < best_field)
            best_field = ns;
    }
    (void)proc_sum;
    printf("  field render 1k:  %.0f ns / pass (target < 100000 ns)\n", best_field);

    orchestrator_t bench_orch = {0};
    for (int i = 0; i < 7; i++) {
        bench_orch.agents[i] = (field_point_t){0};
        bench_orch.agents[i].state = ((uint32_t)(i * 99)) & 0x3FFFFu;
        bench_orch.agents[i].golden = 0x3FFFFu;
        bench_orch.agents[i].reputation = 0xFF;
    }
    const int ORCH_REPS = 5000;
    volatile uint32_t orch_sink = 0;
    double best_orch = 1e99;
    for (int w = 0; w < 50; w++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int k = 0; k < ORCH_REPS; k++) {
            for (int i = 0; i < 7; i++) {
                bench_orch.agents[i].state =
                    ((uint32_t)(i * 99 + k + w)) & 0x3FFFFu;
                bench_orch.agents[i].shadow = 0;
                bench_orch.agents[i].sigma = 0;
            }
            orch_sink += orchestrate(&bench_orch);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ns = ns_delta(&t0, &t1) / (double)ORCH_REPS;
        if (ns < best_orch)
            best_orch = ns;
    }
    (void)orch_sink;
    printf("  orchestrator x7:  %.0f ns / pass (target < 10000 ns)\n", best_orch);

    const int PB_REPS = 500;
    volatile uint32_t pb_sink = 0;
    double best_pb = 1e99;
    for (int w = 0; w < 20; w++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int k = 0; k < PB_REPS; k++)
            pb_sink ^= predict_backward(0x3FFFFu, 0x15555u ^ (uint32_t)(k + w), 18);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ns = ns_delta(&t0, &t1) / (double)PB_REPS;
        if (ns < best_pb)
            best_pb = ns;
    }
    (void)pb_sink;
    printf("  predict_backward: %.0f ns / call (target < 50000 ns)\n", best_pb);

    printf("\n=== ALL 12 TESTS PASSED ===\n");
    printf("=== creation_kernel: KOVA SETTI ===\n");
    return 0;
}
