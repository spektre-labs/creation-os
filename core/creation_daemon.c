/*
 * creation_daemon — 15s heartbeat, Kairos, Reflex (3×σ), dream queue, BBHash sweep.
 * CREATION_OS_GCD=1 on Darwin: libdispatch timer; else sleep(15).
 */
#include "facts_store.h"
#include "kernel_shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif

typedef enum { KAIROS_SLEEP = 0, KAIROS_WAKE = 1, KAIROS_ALERT = 2 } kairos_t;

typedef struct {
    uint32_t state;
    uint32_t golden;
    uint32_t sig_ring[3];
    uint8_t  ring_pos;
    uint32_t dream_rejects;
    uint8_t  daemon_rep;
    int      big_qi;
    int      have_facts;
    unsigned tick_count;
} daemon_ctx_t;

static daemon_ctx_t g_ctx;

static const char *kairos_name(kairos_t k) {
    return k == KAIROS_SLEEP ? "sleep" : (k == KAIROS_ALERT ? "alert" : "wake");
}

static void daemon_tick(daemon_ctx_t *ctx) {
    ctx->tick_count++;
    uint32_t s = measure_sigma_u32(ctx->state & 0x3FFFFu, ctx->golden & 0x3FFFFu);
    ctx->sig_ring[ctx->ring_pos % 3u] = s;
    ctx->ring_pos++;

    uint32_t avg = (ctx->sig_ring[0] + ctx->sig_ring[1] + ctx->sig_ring[2]) / 3u;
    kairos_t k = KAIROS_WAKE;
    if (avg <= 2u)
        k = KAIROS_SLEEP;
    else if (avg >= 10u)
        k = KAIROS_ALERT;

    uint32_t s_perceive = measure_sigma_u32(ctx->state & 0x3FFFFu, ctx->golden & 0x3FFFFu);
    uint32_t s_reflect =
        measure_sigma_u32((ctx->state >> 1) & 0x3FFFFu, ctx->golden & 0x3FFFFu);
    uint32_t s_act =
        measure_sigma_u32((ctx->state * 7u) & 0x3FFFFu, ctx->golden & 0x3FFFFu);

    if (s > 6u)
        ctx->dream_rejects++;

    if (k == KAIROS_SLEEP && ctx->dream_rejects > 0) {
        ctx->dream_rejects--;
        ctx->daemon_rep = (uint8_t)((ctx->daemon_rep << 1) | 1u);
    }

    volatile int bb_hit = 0;
    if (ctx->have_facts && g_nfacts > 0) {
        ctx->big_qi = (ctx->big_qi + 1) % g_nfacts;
        int ix = -1;
        uint64_t hh = facts_gda_hash(g_facts[ctx->big_qi].q);
        bb_hit = facts_bb_lookup(g_facts[ctx->big_qi].q, hh, &ix);
    }
    (void)bb_hit;

    uint32_t needs = (uint32_t)-(int32_t)(s > 0);
    ctx->state = (ctx->state & ~needs) | (ctx->golden & needs);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    printf("[%lld.%03ld] tick=%u kairos=%s avgσ=%u reflex(σp,σr,σa)=(%u,%u,%u) "
           "dream_q=%u rep=0x%02x big_qi=%d\n",
           (long long)ts.tv_sec, ts.tv_nsec / 1000000L, ctx->tick_count, kairos_name(k),
           avg, s_perceive, s_reflect, s_act, (unsigned)ctx->dream_rejects,
           ctx->daemon_rep, ctx->big_qi);
    fflush(stdout);
}

#ifdef __APPLE__
static void run_gcd_timer(void) {
    dispatch_queue_t q = dispatch_queue_create("creation_os.daemon", DISPATCH_QUEUE_SERIAL);
    dispatch_source_t t =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, q);
    dispatch_source_set_timer(
        t, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(15 * NSEC_PER_SEC)),
        (uint64_t)(15 * NSEC_PER_SEC), (uint64_t)(1 * NSEC_PER_SEC));
    dispatch_source_set_event_handler(t, ^{ daemon_tick(&g_ctx); });
    dispatch_resume(t);
    dispatch_main();
}
#endif

int main(int argc, char **argv) {
    memset(&g_ctx, 0, sizeof g_ctx);
    g_ctx.state = 0x15555u;
    g_ctx.golden = 0x3FFFFu;

    double load_ns = 0;
    if (argc >= 2) {
        if (facts_load(argv[1], &load_ns) == 0) {
            g_ctx.have_facts = 1;
            fprintf(stderr, "daemon: loaded %d facts (%.3f ms)\n", g_nfacts,
                    load_ns / 1e6);
        } else
            fprintf(stderr, "daemon: facts_load failed, running without DB\n");
    } else
        fprintf(stderr, "daemon: no facts.json — run: %s facts.json\n", argv[0]);

#ifdef __APPLE__
    if (getenv("CREATION_OS_GCD") != NULL) {
        fprintf(stderr, "daemon: GCD 15s timer (CREATION_OS_GCD=1)\n");
        run_gcd_timer();
        return 0;
    }
#endif

    fprintf(stderr, "daemon: POSIX sleep(15) loop, Ctrl-C to stop\n");
    for (;;)
        daemon_tick(&g_ctx), sleep(15);
}
