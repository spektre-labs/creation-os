/*
 * σ-mission — checkpointed multi-step execution with σ drift detection.
 */

#include "mission.h"

#include "coherence_watchdog.h"
#include "cos_think.h"
#include "engram_episodic.h"
#include "error_attribution.h"
#include "introspection.h"
#include "multi_sigma.h"
#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "engram_persist.h"
#include "stub_gen.h"
#include "state_ledger.h"
#include "reinforce.h"
#include "escalation.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

extern volatile int g_cos_cw_pause_request;

static int mission_offline(void);

#define COS_MISSION_CK_MAGIC 0x4350434du /* 'CMCP' BE-as-u32 read/write LE machine */
#define COS_MISSION_ST_MAGIC 0x3154534du /* 'MST1' */

#ifndef COS_THINK_MULTI_TEXT_CAP
#define COS_THINK_MULTI_TEXT_CAP 2048
#endif
#ifndef COS_THINK_MULTI_K_REGEN
#define COS_THINK_MULTI_K_REGEN 2
#endif

static int g_retries[COS_MISSION_MAX_STEPS];

typedef struct {
    cos_pipeline_config_t    cfg;
    cos_sigma_engram_entry_t slots[64];
    cos_sigma_engram_t       engram;
    cos_sigma_sovereign_t    sovereign;
    cos_sigma_agent_t        agent;
    cos_sigma_codex_t        codex;
    cos_cli_generate_ctx_t     genctx;
    cos_engram_persist_t      *persist;
    int                        have_codex;
} mission_sess_t;

typedef struct {
    cos_multi_sigma_t ens;
    int               have;
    int               k_used;
} mission_multi_t;

static int mission_genctx_minimal(cos_cli_generate_ctx_t *g,
                                  cos_sigma_codex_t *codex,
                                  cos_engram_persist_t *persist,
                                  int have_codex)
{
    memset(g, 0, sizeof(*g));
    g->magic                  = COS_CLI_GENERATE_CTX_MAGIC;
    g->codex                  = have_codex ? codex : NULL;
    g->persist                = persist;
    g->icl_exemplar_max_sigma = 0.35f;
    g->icl_k                  = 0;
    g->icl_rethink_only       = 0;
    g->icl_compose            = NULL;
    g->icl_ctx                = NULL;
    return 0;
}

static void mission_metacog_levels(const char *prompt,
                                   const cos_pipeline_result_t *r,
                                   int                          max_rethink,
                                   cos_ultra_metacog_levels_t *lv)
{
    memset(lv, 0, sizeof(*lv));
    int words = 0, in = 0;
    if (prompt != NULL) {
        for (const char *s = prompt; *s; ++s) {
            int w = (*s > ' ');
            if (w && !in) {
                words++;
                in = 1;
            } else if (!w)
                in = 0;
        }
    }
    lv->sigma_perception = (words <= 3) ? 0.35f : 0.05f;
    lv->sigma_self       = (r != NULL) ? r->sigma : 1.0f;
    lv->sigma_social     = 0.0f;
    if (prompt != NULL) {
        size_t L = strlen(prompt);
        if (L > 0 && prompt[L - 1] == '?' && words <= 4)
            lv->sigma_social = 0.45f;
    }
    int budget = max_rethink > 0 ? max_rethink : 1;
    lv->sigma_situational =
        (r != NULL) ? ((float)r->rethink_count / (float)budget) : 0.0f;
    if (lv->sigma_situational > 1.0f)
        lv->sigma_situational = 1.0f;
}

static int mission_multi_shadow(cos_pipeline_config_t *cfg, const char *input,
                                const char *primary_text, float primary_sigma,
                                mission_multi_t *out)
{
    if (out == NULL || cfg == NULL)
        return -1;
    memset(out, 0, sizeof(*out));
    const char *texts[1 + COS_THINK_MULTI_K_REGEN];
    char        copies[1 + COS_THINK_MULTI_K_REGEN][COS_THINK_MULTI_TEXT_CAP];
    int n = 0;
    if (primary_text != NULL) {
        snprintf(copies[n], sizeof(copies[n]), "%s", primary_text);
        texts[n] = copies[n];
        n++;
    }
    for (int k = 0; k < COS_THINK_MULTI_K_REGEN; ++k) {
        if (cfg->generate == NULL)
            break;
        const char *t = NULL;
        float       s = 1.0f;
        double      c = 0.0;
        if (cfg->generate(input, k + 1, cfg->generate_ctx, &t, &s, &c) != 0
            || t == NULL)
            break;
        snprintf(copies[n], sizeof(copies[n]), "%s", t);
        texts[n] = copies[n];
        n++;
    }
    float sigma_consistency =
        (n >= 2) ? cos_multi_sigma_consistency(texts, n) : 0.0f;
    if (sigma_consistency < 0.0f)
        sigma_consistency = 0.0f;
    float s_lp = primary_sigma;
    if (s_lp < 0.0f)
        s_lp = 0.0f;
    if (s_lp > 1.0f)
        s_lp = 1.0f;
    if (cos_multi_sigma_combine(s_lp, s_lp, s_lp, sigma_consistency, NULL,
                                  &out->ens) == 0) {
        out->have   = 1;
        out->k_used = n;
        return 0;
    }
    return -1;
}

static void mission_store_episode(const char *prompt, float sigma_combined,
                                  enum cos_error_source src,
                                  cos_sigma_action_t act, int ttt_applied)
{
    struct cos_engram_episode ep;
    ep.timestamp_ms   = (int64_t)time(NULL) * 1000LL;
    ep.prompt_hash    = cos_engram_prompt_hash(prompt);
    ep.sigma_combined = sigma_combined;
    ep.action         = (int)act;
    ep.was_correct    = -1;
    ep.attribution    = src;
    ep.ttt_applied    = ttt_applied != 0 ? 1 : 0;
    (void)cos_engram_episode_store(&ep);
}

static int mission_sess_init(mission_sess_t *S, int allow_cloud,
                             int max_rethink, float tau_a, float tau_r)
{
    memset(S, 0, sizeof(*S));
    if (cos_sigma_engram_init(&S->engram, S->slots, 64, 0.25f, 200, 20) != 0)
        return -1;
    cos_sigma_sovereign_init(&S->sovereign, 0.85f);
    cos_sigma_agent_init(&S->agent, 0.80f, 0.10f);

    const char *no_persist = getenv("COS_ENGRAM_DISABLE");
    if (no_persist == NULL || no_persist[0] != '1') {
        if (cos_engram_persist_open(NULL, &S->persist) != 0)
            S->persist = NULL;
        if (S->persist != NULL)
            (void)cos_engram_persist_load(S->persist, &S->engram, 64);
    }

    if (cos_sigma_codex_load_seed(&S->codex) == 0)
        S->have_codex = 1;

    mission_genctx_minimal(&S->genctx, &S->codex, S->persist, S->have_codex);

    cos_sigma_pipeline_config_defaults(&S->cfg);
    S->cfg.tau_accept  = tau_a;
    S->cfg.tau_rethink = tau_r;
    S->cfg.max_rethink = max_rethink;
    S->cfg.mode        = allow_cloud ? COS_PIPELINE_MODE_HYBRID
                                     : COS_PIPELINE_MODE_LOCAL_ONLY;
    S->cfg.codex       = S->have_codex ? &S->codex : NULL;
    S->cfg.engram      = &S->engram;
    S->cfg.sovereign   = &S->sovereign;
    S->cfg.agent       = &S->agent;
    S->cfg.generate    = cos_cli_chat_generate;
    S->cfg.generate_ctx = &S->genctx;
    S->cfg.escalate_ctx   = &S->genctx;
    S->cfg.escalate    = cos_cli_escalate_api;
    if (S->persist != NULL) {
        S->cfg.on_engram_store     = cos_engram_persist_store;
        S->cfg.on_engram_store_ctx = S->persist;
    }
    return 0;
}

static void mission_sess_shutdown(mission_sess_t *S)
{
    cos_sigma_pipeline_free_engram_values(&S->engram);
    if (S->have_codex)
        cos_sigma_codex_free(&S->codex);
    cos_engram_persist_close(S->persist);
}

static int64_t mission_now_ms(void)
{
    return (int64_t)time(NULL) * 1000LL;
}

/* ---------- SHA-256 (compact, public-domain style) ---------- */

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buf[64];
    size_t   buflen;
} mission_sha256_ctx;

static uint32_t rotr(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32 - n));
}

static void mission_sha256_transform(mission_sha256_ctx *ctx,
                                     const uint8_t block[64])
{
    static const uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)block[i * 4] << 24)
             | ((uint32_t)block[i * 4 + 1] << 16)
             | ((uint32_t)block[i * 4 + 2] << 8)
             | ((uint32_t)block[i * 4 + 3]);
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = ctx->state[0], b = ctx->state[1];
    uint32_t c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5];
    uint32_t g = ctx->state[6], h = ctx->state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + S0 + maj;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void mission_sha256_init(mission_sha256_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
}

static void mission_sha256_update(mission_sha256_ctx *ctx,
                                  const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        ctx->buf[ctx->buflen++] = data[i];
        ctx->bitlen += 8;
        if (ctx->buflen == 64) {
            mission_sha256_transform(ctx, ctx->buf);
            ctx->buflen = 0;
        }
    }
}

static void mission_sha256_final(mission_sha256_ctx *ctx, uint8_t out[32])
{
    uint64_t orig_bits = ctx->bitlen;
    uint8_t  b0        = 0x80;
    mission_sha256_update(ctx, &b0, 1);
    while ((ctx->buflen % 64) != 56) {
        uint8_t z = 0;
        mission_sha256_update(ctx, &z, 1);
    }
    uint8_t lenbe[8];
    uint64_t v = orig_bits;
    for (int i = 0; i < 8; ++i) {
        lenbe[7 - i] = (uint8_t)(v & 0xffu);
        v >>= 8;
    }
    memcpy(ctx->buf + 56, lenbe, 8);
    mission_sha256_transform(ctx, ctx->buf);
    for (int i = 0; i < 8; ++i) {
        out[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

static void mission_sha256_hex(const uint8_t *data, size_t len,
                               char hex[65])
{
    mission_sha256_ctx cx;
    mission_sha256_init(&cx);
    mission_sha256_update(&cx, data, len);
    uint8_t raw[32];
    mission_sha256_final(&cx, raw);
    static const char xd[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        hex[i * 2]     = xd[(raw[i] >> 4) & 15];
        hex[i * 2 + 1] = xd[raw[i] & 15];
    }
    hex[64] = '\0';
}

static int ensure_dir(const char *path)
{
#if defined(__unix__) || defined(__APPLE__)
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return 0;
        return -1;
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        return -1;
#endif
    return 0;
}

static int missions_home_dir(char *out, size_t cap)
{
    const char *home = getenv("HOME");
    if (!home || !home[0] || cap < 16)
        return -1;
    snprintf(out, cap, "%s/.cos/missions", home);
    return ensure_dir(out);
}

static int mission_dir_for(const struct cos_mission *m, char *out, size_t cap)
{
    if (!m || missions_home_dir(out, cap) != 0)
        return -1;
    size_t L = strlen(out);
    if (L + strlen(m->mission_id) + 8 >= cap)
        return -1;
    snprintf(out + L, cap - L, "/%s", m->mission_id);
    return ensure_dir(out);
}

static void mission_uuid(char *dst)
{
    unsigned char b[16];
#ifdef __APPLE__
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp) {
        (void)fread(b, 1, 16, fp);
        fclose(fp);
    } else {
        for (int i = 0; i < 16; ++i)
            b[i] = (unsigned char)(rand() ^ (rand() >> 8));
    }
#else
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp) {
        (void)fread(b, 1, 16, fp);
        fclose(fp);
    } else {
        for (int i = 0; i < 16; ++i)
            b[i] = (unsigned char)(rand() ^ (rand() >> 8));
    }
#endif
    b[6] = (unsigned char)((b[6] & 0x0fu) | 0x40u);
    b[8] = (unsigned char)((b[8] & 0x3fu) | 0x80u);
    static const char xd[] = "0123456789abcdef";
    int p = 0;
    for (int i = 0; i < 16; ++i) {
        dst[p++] = xd[(b[i] >> 4) & 15];
        dst[p++] = xd[b[i] & 15];
        if (i == 3 || i == 5 || i == 7 || i == 9)
            dst[p++] = '-';
    }
    dst[p] = '\0';
}

static int engram_episodes_path(char *buf, size_t cap)
{
    const char *env = getenv("COS_ENGRAM_EPISODES_DB");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    const char *home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    snprintf(buf, cap, "%s/.cos/engram_episodes.db", home);
    return 0;
}

static int engram_semantic_path(char *buf, size_t cap)
{
    const char *env = getenv("COS_ENGRAM_SEMANTIC_DB");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    const char *home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    snprintf(buf, cap, "%s/.cos/engram_semantic.db", home);
    return 0;
}

static int copy_if_exists(const char *src, const char *dst)
{
    FILE *fi = fopen(src, "rb");
    if (!fi)
        return 0;
    FILE *fo = fopen(dst, "wb");
    if (!fo) {
        fclose(fi);
        return -1;
    }
    char   buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) {
        if (fwrite(buf, 1, n, fo) != n) {
            fclose(fi);
            fclose(fo);
            return -1;
        }
    }
    fclose(fi);
    fclose(fo);
    return 0;
}

static void mission_sigma_mean_update(struct cos_mission *m)
{
    double s = 0.0;
    int    c = 0;
    for (int i = 0; i < m->n_steps; ++i) {
        if (m->steps[i].status == COS_MISSION_STEP_DONE) {
            s += (double)m->steps[i].sigma;
            c++;
        }
    }
    m->sigma_mean = (c > 0) ? (float)(s / (double)c) : 0.f;
}

float cos_mission_sigma_trend(const struct cos_mission *m)
{
    if (!m || m->n_steps < 2)
        return 0.f;
    float y[64];
    int   n = 0;
    for (int i = 0; i < m->n_steps && n < 64; ++i) {
        if (m->steps[i].status == COS_MISSION_STEP_DONE)
            y[n++] = m->steps[i].sigma;
    }
    if (n < 3)
        return 0.f;
    int take = n > 8 ? 8 : n;
    /* last `take` points */
    int off = n - take;
    double sumx = 0.0, sumy = 0.0, sumxx = 0.0, sumxy = 0.0;
    for (int i = 0; i < take; ++i) {
        double xi = (double)i;
        double yi = (double)y[off + i];
        sumx += xi;
        sumy += yi;
        sumxx += xi * xi;
        sumxy += xi * yi;
    }
    double den = (double)take * sumxx - sumx * sumx;
    if (fabs(den) < 1e-12)
        return 0.f;
    double slope =
        ((double)take * sumxy - sumx * sumy) / den;
    return (float)slope;
}

static float mission_env_float(const char *name, float def)
{
    const char *e = getenv(name);
    if (!e || !e[0])
        return def;
    return (float)atof(e);
}

int cos_mission_checkpoint_save(struct cos_mission *m, int step_idx)
{
    if (!m || step_idx < 0 || step_idx >= COS_MISSION_MAX_STEPS)
        return -1;
    char dir[768];
    if (mission_dir_for(m, dir, sizeof(dir)) != 0)
        return -2;

    struct cos_state_ledger L;
    cos_state_ledger_init(&L);
    char lp[768];
    char *ledger_js = NULL;
    if (cos_state_ledger_default_path(lp, sizeof(lp)) == 0
        && cos_state_ledger_load(&L, lp) == 0)
        ledger_js = cos_state_ledger_to_json(&L);
    else
        ledger_js = strdup("{}");
    if (!ledger_js)
        return -3;
    size_t ljlen = strlen(ledger_js);

    char path[900];
    snprintf(path, sizeof(path), "%s/checkpoint_%d.bin", dir, step_idx);
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(ledger_js);
        return -4;
    }

    uint32_t magic = COS_MISSION_CK_MAGIC;
    uint32_t ver   = 1;
    uint32_t stp   = (uint32_t)step_idx;
    uint32_t mlen  = (uint32_t)sizeof(struct cos_mission);
    uint32_t jlen  = (uint32_t)ljlen;

    if (fwrite(&magic, 4, 1, fp) != 1 || fwrite(&ver, 4, 1, fp) != 1
        || fwrite(&stp, 4, 1, fp) != 1 || fwrite(&mlen, 4, 1, fp) != 1
        || fwrite(&jlen, 4, 1, fp) != 1) {
        fclose(fp);
        free(ledger_js);
        return -5;
    }
    if (fwrite(m, sizeof(*m), 1, fp) != 1
        || fwrite(ledger_js, 1, ljlen, fp) != ljlen) {
        fclose(fp);
        free(ledger_js);
        return -6;
    }
    fclose(fp);
    free(ledger_js);

    /* Sidecar engram DB snapshots */
    char ep_src[768], sm_src[768];
    if (engram_episodes_path(ep_src, sizeof(ep_src)) == 0) {
        char ep_dst[900];
        snprintf(ep_dst, sizeof(ep_dst), "%s/checkpoint_%d_episodes.db",
                 dir, step_idx);
        (void)copy_if_exists(ep_src, ep_dst);
    }
    if (engram_semantic_path(sm_src, sizeof(sm_src)) == 0) {
        char sm_dst[900];
        snprintf(sm_dst, sizeof(sm_dst), "%s/checkpoint_%d_semantic.db",
                 dir, step_idx);
        (void)copy_if_exists(sm_src, sm_dst);
    }

    /* Hash file for step record */
    FILE *fr = fopen(path, "rb");
    if (fr) {
        fseek(fr, 0, SEEK_END);
        long sz = ftell(fr);
        fseek(fr, 0, SEEK_SET);
        if (sz > 0 && sz < 16777216) {
            uint8_t *blob = (uint8_t *)malloc((size_t)sz);
            if (blob && fread(blob, 1, (size_t)sz, fr) == (size_t)sz)
                mission_sha256_hex(blob, (size_t)sz,
                                   m->steps[step_idx].checkpoint_hash);
            free(blob);
        }
        fclose(fr);
    }
    return 0;
}

int cos_mission_checkpoint_restore(struct cos_mission *m, int step_idx)
{
    if (!m || step_idx < 0 || step_idx >= COS_MISSION_MAX_STEPS)
        return -1;
    char dir[768];
    if (mission_dir_for(m, dir, sizeof(dir)) != 0)
        return -2;

    char path[900];
    snprintf(path, sizeof(path), "%s/checkpoint_%d.bin", dir, step_idx);
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -3;

    uint32_t magic, ver, stp, mlen, jlen;
    if (fread(&magic, 4, 1, fp) != 1 || fread(&ver, 4, 1, fp) != 1
        || fread(&stp, 4, 1, fp) != 1 || fread(&mlen, 4, 1, fp) != 1
        || fread(&jlen, 4, 1, fp) != 1 || magic != COS_MISSION_CK_MAGIC
        || ver != 1u || mlen != (uint32_t)sizeof(struct cos_mission)) {
        fclose(fp);
        return -4;
    }
    (void)stp;

    struct cos_mission tmp;
    if (fread(&tmp, sizeof(tmp), 1, fp) != 1) {
        fclose(fp);
        return -5;
    }
    char *js = (char *)malloc((size_t)jlen + 1);
    if (!js) {
        fclose(fp);
        return -6;
    }
    if (fread(js, 1, (size_t)jlen, fp) != (size_t)jlen) {
        free(js);
        fclose(fp);
        return -7;
    }
    js[jlen] = '\0';
    fclose(fp);

    char tmpl[] = "/tmp/cos_ml_XXXXXX";
    int  fd     = mkstemp(tmpl);
    if (fd < 0) {
        free(js);
        return -8;
    }
    FILE *tw = fdopen(fd, "wb");
    if (!tw) {
        close(fd);
        free(js);
        return -9;
    }
    fputs(js, tw);
    fputc('\n', tw);
    fclose(tw);
    free(js);

    struct cos_state_ledger Ld;
    cos_state_ledger_init(&Ld);
    if (cos_state_ledger_load(&Ld, tmpl) == 0) {
        char defp[768];
        if (cos_state_ledger_default_path(defp, sizeof defp) == 0)
            (void)cos_state_ledger_persist(&Ld, defp);
    }
    unlink(tmpl);

    memcpy(m, &tmp, sizeof(*m));

    /* Restore engram sidecars */
    char ep_src[900], sm_src[900];
    snprintf(ep_src, sizeof(ep_src), "%s/checkpoint_%d_episodes.db", dir,
             step_idx);
    snprintf(sm_src, sizeof(sm_src), "%s/checkpoint_%d_semantic.db", dir,
             step_idx);
    char ep_dst[768], sm_dst[768];
    if (engram_episodes_path(ep_dst, sizeof(ep_dst)) == 0)
        (void)copy_if_exists(ep_src, ep_dst);
    if (engram_semantic_path(sm_dst, sizeof(sm_dst)) == 0)
        (void)copy_if_exists(sm_src, sm_dst);

    mission_sigma_mean_update(m);
    m->sigma_trend = cos_mission_sigma_trend(m);
    return 0;
}

int cos_mission_rollback(struct cos_mission *m)
{
    if (!m || m->current_step < 0 || m->current_step >= m->n_steps)
        return -1;
    int si = m->current_step;
    if (cos_mission_checkpoint_restore(m, si) != 0)
        return -2;
    m->steps[si].status = COS_MISSION_STEP_ROLLED_BACK;
    snprintf(m->steps[si].result, sizeof m->steps[si].result,
             "[rolled back at step %d]", si);
    return 0;
}

int cos_mission_pause(struct cos_mission *m, const char *reason)
{
    if (!m)
        return -1;
    m->paused = 1;
    if (reason)
        snprintf(m->pause_reason, sizeof m->pause_reason, "%s", reason);
    else
        m->pause_reason[0] = '\0';
    (void)cos_mission_persist(m, NULL);
    return 0;
}

int cos_mission_resume(struct cos_mission *m)
{
    if (!m)
        return -1;
    m->paused = 0;
    m->pause_reason[0] = '\0';
    return 0;
}

int cos_mission_persist(const struct cos_mission *m, const char *path)
{
    if (!m)
        return -1;
    char buf[900];
    const char *use = path;
    if (!use) {
        char dir[768];
        if (mission_dir_for(m, dir, sizeof(dir)) != 0)
            return -2;
        snprintf(buf, sizeof(buf), "%s/mission.state", dir);
        use = buf;
    }
    FILE *fp = fopen(use, "wb");
    if (!fp)
        return -3;
    uint32_t magic = COS_MISSION_ST_MAGIC;
    uint32_t ver   = 1;
    uint32_t mlen  = (uint32_t)sizeof(struct cos_mission);
    if (fwrite(&magic, 4, 1, fp) != 1 || fwrite(&ver, 4, 1, fp) != 1
        || fwrite(&mlen, 4, 1, fp) != 1
        || fwrite(m, sizeof(*m), 1, fp) != 1) {
        fclose(fp);
        return -4;
    }
    fclose(fp);

    /* current pointer */
    char curf[768];
    if (missions_home_dir(curf, sizeof(curf)) == 0) {
        size_t L = strlen(curf);
        snprintf(curf + L, sizeof(curf) - L, "/current");
        FILE *fc = fopen(curf, "w");
        if (fc) {
            fprintf(fc, "%s\n", m->mission_id);
            fclose(fc);
        }
    }
    return 0;
}

int cos_mission_load(struct cos_mission *m, const char *path)
{
    if (!m || !path)
        return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -2;
    uint32_t magic, ver, mlen;
    if (fread(&magic, 4, 1, fp) != 1 || fread(&ver, 4, 1, fp) != 1
        || fread(&mlen, 4, 1, fp) != 1 || magic != COS_MISSION_ST_MAGIC
        || ver != 1u || mlen != (uint32_t)sizeof(struct cos_mission)) {
        fclose(fp);
        return -3;
    }
    if (fread(m, sizeof(*m), 1, fp) != 1) {
        fclose(fp);
        return -4;
    }
    fclose(fp);
    return 0;
}

static void watchdog_cfg_from_env(void)
{
    struct cos_cw_config wc;
    cos_cw_default_config(&wc);
    wc.k_eff_min        = mission_env_float("COS_WATCHDOG_K_EFF_MIN",
                                            wc.k_eff_min);
    wc.sigma_max        = mission_env_float("COS_WATCHDOG_SIGMA_MAX",
                                              wc.sigma_max);
    const char *ci = getenv("COS_WATCHDOG_INTERVAL_MS");
    if (ci && ci[0])
        wc.check_interval_ms = atoi(ci);
    const char *ac = getenv("COS_WATCHDOG_ALARM_COUNT");
    if (ac && ac[0])
        wc.alarm_count = atoi(ac);
    cos_cw_install_config(&wc);
}

int cos_mission_create(const char *goal, struct cos_mission *m)
{
    if (!goal || !goal[0] || !m)
        return -1;
    memset(m, 0, sizeof(*m));
    memset(g_retries, 0, sizeof(g_retries));
    snprintf(m->goal, sizeof m->goal, "%s", goal);
    mission_uuid(m->mission_id);
    m->started_ms = mission_now_ms();

    if (mission_offline()) {
        m->n_steps              = 1;
        m->steps[0].step_id     = 0;
        snprintf(m->steps[0].description, sizeof m->steps[0].description, "%s",
                 goal);
        m->steps[0].status = COS_MISSION_STEP_PENDING;
        (void)cos_mission_persist(m, NULL);
        return 0;
    }

    char sub[COS_THINK_MAX_SUBTASKS][1024];
    int  n = 0;
    cos_think_decompose(goal, sub, &n);
    if (n <= 0)
        return -2;
    if (n > COS_MISSION_MAX_STEPS)
        n = COS_MISSION_MAX_STEPS;
    m->n_steps = n;
    for (int i = 0; i < n; ++i) {
        m->steps[i].step_id = i;
        snprintf(m->steps[i].description, sizeof m->steps[i].description, "%s",
                 sub[i]);
        m->steps[i].status = COS_MISSION_STEP_PENDING;
    }
    (void)cos_mission_persist(m, NULL);
    return 0;
}

static int mission_offline(void)
{
    const char *e = getenv("COS_MISSION_OFFLINE");
    return (e && e[0] == '1') ? 1 : 0;
}

int cos_mission_step_execute(struct cos_mission *m, int step_idx)
{
    if (!m || step_idx < 0 || step_idx >= m->n_steps)
        return -1;

    if (cos_mission_checkpoint_save(m, step_idx) != 0)
        return -2;

    m->steps[step_idx].status = COS_MISSION_STEP_RUNNING;
    m->steps[step_idx].started_ms = mission_now_ms();

    float tau_m = mission_env_float("COS_MISSION_SIGMA_TAU", 0.65f);

    const char *prefix[3] = { "", "Use a different approach than usual:\n",
                              "Third attempt — minimal assumptions:\n" };

    for (int attempt = 0; attempt <= 2; ++attempt) {
        char prompt[1200];
        snprintf(prompt, sizeof(prompt), "%s%s", prefix[attempt],
                 m->steps[step_idx].description);

        struct cos_state_ledger ledger;
        cos_state_ledger_init(&ledger);

        float sigma_out = 1.0f;

        if (mission_offline()) {
            snprintf(m->steps[step_idx].result,
                     sizeof m->steps[step_idx].result,
                     "[offline mission step %d attempt %d]", step_idx, attempt);
            sigma_out       = 0.25f;
            m->steps[step_idx].sigma = sigma_out;
            m->steps[step_idx].attribution = COS_ERR_NONE;
        } else {
            float tau_a = mission_env_float("COS_MISSION_TAU_ACCEPT", 0.35f);
            float tau_r = mission_env_float("COS_MISSION_TAU_RETHINK", 0.70f);
            int   mr    = (int)cos_sigma_reinforce_max_rounds();
            if (mr < 1)
                mr = 3;
            int allow_cloud = 1;
            const char *bl = getenv("COS_THINK_BACKENDS");
            if (bl != NULL && strstr(bl, "local") != NULL
                && strstr(bl, "cloud") == NULL)
                allow_cloud = 0;

            mission_sess_t S;
            if (mission_sess_init(&S, allow_cloud, mr, tau_a, tau_r) != 0) {
                mission_sess_shutdown(&S);
                m->steps[step_idx].status = COS_MISSION_STEP_FAILED;
                return -3;
            }
            cos_pipeline_result_t pr;
            memset(&pr, 0, sizeof(pr));
            clock_t c0 = clock();
            int prc = cos_sigma_pipeline_run(&S.cfg, prompt, &pr);
            clock_t c1 = clock();
            int64_t lat =
                (int64_t)((double)(c1 - c0) * 1000.0 / (double)CLOCKS_PER_SEC);
            if (lat < 0)
                lat = 0;

            if (prc != 0) {
                mission_sess_shutdown(&S);
                g_retries[step_idx]++;
                if (g_retries[step_idx] >= 3) {
                    m->steps[step_idx].status = COS_MISSION_STEP_FAILED;
                    snprintf(m->steps[step_idx].result,
                             sizeof m->steps[step_idx].result,
                             "[pipeline failure]");
                    cos_mission_pause(m, "step pipeline failure after retries");
                    return -4;
                }
                (void)cos_mission_checkpoint_restore(m, step_idx);
                continue;
            }

            mission_multi_t ms;
            float sigma_use = pr.sigma;
            if (mission_multi_shadow(&S.cfg, prompt,
                                     pr.response ? pr.response : "", pr.sigma,
                                     &ms)
                    == 0
                && ms.have)
                sigma_use = ms.ens.sigma_combined;

            cos_ultra_metacog_levels_t lv;
            mission_metacog_levels(prompt, &pr, S.cfg.max_rethink, &lv);
            struct cos_error_attribution attr =
                cos_error_attribute(ms.have ? ms.ens.sigma_logprob : sigma_use,
                                   ms.have ? ms.ens.sigma_entropy : sigma_use,
                                   ms.have ? ms.ens.sigma_consistency : 0.0f,
                                   lv.sigma_perception);

            mission_store_episode(prompt, sigma_use, attr.source, pr.final_action,
                                  pr.ttt_applied);

            cos_multi_sigma_t mfill;
            if (ms.have)
                cos_state_ledger_fill_multi(&ledger, &ms.ens);
            else {
                mfill.sigma_logprob     = sigma_use;
                mfill.sigma_entropy     = sigma_use;
                mfill.sigma_perplexity    = sigma_use;
                mfill.sigma_consistency = 0.0f;
                mfill.sigma_combined    = sigma_use;
                cos_state_ledger_fill_multi(&ledger, &mfill);
            }
            float meta[4] = { lv.sigma_perception, lv.sigma_self,
                              lv.sigma_social, lv.sigma_situational };
            cos_state_ledger_update(&ledger, sigma_use, meta,
                                     (int)pr.final_action);
            cos_state_ledger_add_cost(&ledger, (float)pr.cost_eur);
            if (pr.engram_hit)
                cos_state_ledger_note_cache_hit(&ledger);

            char lp[768];
            if (cos_state_ledger_default_path(lp, sizeof(lp)) == 0)
                (void)cos_state_ledger_persist(&ledger, lp);

            snprintf(m->steps[step_idx].result,
                     sizeof m->steps[step_idx].result, "%s",
                     pr.response ? pr.response : "");
            sigma_out                  = sigma_use;
            m->steps[step_idx].sigma   = sigma_out;
            m->steps[step_idx].attribution = attr.source;

            mission_sess_shutdown(&S);
            (void)lat;
        }

        if (sigma_out <= tau_m) {
            m->steps[step_idx].status      = COS_MISSION_STEP_DONE;
            m->steps[step_idx].finished_ms = mission_now_ms();
            g_retries[step_idx]            = 0;
            mission_sigma_mean_update(m);
            m->sigma_trend = cos_mission_sigma_trend(m);
            (void)cos_mission_checkpoint_save(m, step_idx + 1);
            (void)cos_mission_persist(m, NULL);
            return 0;
        }

        /* high σ — rollback checkpoint for this step and retry */
        g_retries[step_idx]++;
        if (cos_mission_checkpoint_restore(m, step_idx) != 0)
            break;
        if (g_retries[step_idx] >= 3) {
            snprintf(m->steps[step_idx].result,
                     sizeof m->steps[step_idx].result,
                     "[σ above τ after retries]");
            m->steps[step_idx].status = COS_MISSION_STEP_FAILED;
            cos_mission_pause(m, "σ gate / retries exhausted");
            return -5;
        }
    }

    m->steps[step_idx].status = COS_MISSION_STEP_FAILED;
    cos_mission_pause(m, "step failed");
    return -6;
}

int cos_mission_run(struct cos_mission *m)
{
    if (!m || m->n_steps <= 0)
        return -1;

    watchdog_cfg_from_env();

    int64_t t0 = mission_now_ms();

    for (int i = 0; i < m->n_steps; ++i) {
        if (m->paused)
            break;
        if (g_cos_cw_pause_request) {
            cos_mission_pause(m, "watchdog coherence / σ alarm");
            g_cos_cw_pause_request = 0;
            break;
        }

        m->current_step = i;

        struct cos_state_ledger Lchk;
        cos_state_ledger_init(&Lchk);
        char lpath[768];
        if (cos_state_ledger_default_path(lpath, sizeof(lpath)) == 0
            && cos_state_ledger_load(&Lchk, lpath) == 0) {
            m->coherence_status = Lchk.coherence_status;
            if (cos_cw_check(&Lchk))
                cos_mission_pause(m, "watchdog threshold (sync check)");
        }

        if (cos_mission_step_execute(m, i) != 0)
            break;

        m->elapsed_ms = mission_now_ms() - t0;
        m->sigma_trend = cos_mission_sigma_trend(m);
        if (m->sigma_trend > 0.05f && m->n_steps >= 3) {
            cos_mission_pause(m,
                              "Mission coherence declining (σ slope rising). "
                              "Review needed.");
            break;
        }

        if (getenv("COS_MISSION_ONE_STEP")) {
            const char *os = getenv("COS_MISSION_ONE_STEP");
            if (os && os[0] == '1')
                break;
        }
    }

    m->elapsed_ms = mission_now_ms() - t0;
    (void)cos_mission_persist(m, NULL);
    return 0;
}

char *cos_mission_report(const struct cos_mission *m)
{
    if (!m)
        return NULL;
    size_t cap = 65536;
    char * buf = (char *)malloc(cap);
    if (!buf)
        return NULL;
    int n = snprintf(buf, cap,
                     "mission_id=%s\n"
                     "goal=%s\n"
                     "n_steps=%d current_step=%d paused=%d\n"
                     "sigma_mean=%.4f sigma_trend=%.4f coherence_status=%d\n"
                     "elapsed_ms=%lld pause_reason=%s\n",
                     m->mission_id, m->goal, m->n_steps, m->current_step,
                     m->paused, (double)m->sigma_mean, (double)m->sigma_trend,
                     m->coherence_status, (long long)m->elapsed_ms,
                     m->pause_reason);
    if (n < 0 || (size_t)n >= cap) {
        free(buf);
        return NULL;
    }
    size_t w = (size_t)n;
    for (int i = 0; i < m->n_steps && w + 400 < cap; ++i) {
        int k =
            snprintf(buf + w, cap - w,
                     "step %d status=%d sigma=%.4f hash=%s desc=%.80s...\n", i,
                     m->steps[i].status, (double)m->steps[i].sigma,
                     m->steps[i].checkpoint_hash, m->steps[i].description);
        if (k > 0)
            w += (size_t)k;
    }
    return buf;
}

void cos_mission_print_status(const struct cos_mission *m)
{
    char *r = cos_mission_report(m);
    if (r) {
        fputs(r, stdout);
        free(r);
    }
}

#if defined(CREATION_OS_ENABLE_SELF_TESTS)

int cos_mission_self_test(void)
{
    const char *prev = getenv("COS_MISSION_OFFLINE");
    char        prevbuf[32];
    prevbuf[0] = '\0';
    if (prev)
        snprintf(prevbuf, sizeof(prevbuf), "%s", prev);
    setenv("COS_MISSION_OFFLINE", "1", 1);

    struct cos_mission m;
    if (cos_mission_create("test mission goal alpha", &m) != 0) {
        if (prev)
            setenv("COS_MISSION_OFFLINE", prevbuf, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return -1;
    }

    if (cos_mission_checkpoint_save(&m, 0) != 0) {
        if (prev)
            setenv("COS_MISSION_OFFLINE", prevbuf, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return -2;
    }

    if (cos_mission_checkpoint_restore(&m, 0) != 0) {
        if (prev)
            setenv("COS_MISSION_OFFLINE", prevbuf, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return -3;
    }

    if (fabsf(cos_mission_sigma_trend(&m)) > 1e-3f)
        ; /* ok */

    if (cos_mission_step_execute(&m, 0) != 0) {
        if (prev)
            setenv("COS_MISSION_OFFLINE", prevbuf, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return -4;
    }

    char *rep = cos_mission_report(&m);
    if (!rep || !strstr(rep, m.mission_id)) {
        free(rep);
        if (prev)
            setenv("COS_MISSION_OFFLINE", prevbuf, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return -5;
    }
    free(rep);

    char sp[900];
    if (mission_dir_for(&m, sp, sizeof(sp)) != 0) {
        if (prev)
            setenv("COS_MISSION_OFFLINE", prevbuf, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return -6;
    }
    snprintf(sp + strlen(sp), sizeof(sp) - strlen(sp), "/mission.state");
    if (cos_mission_persist(&m, sp) != 0 || cos_mission_load(&m, sp) != 0) {
        if (prev)
            setenv("COS_MISSION_OFFLINE", prevbuf, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return -7;
    }

    if (prev)
        setenv("COS_MISSION_OFFLINE", prevbuf, 1);
    else
        unsetenv("COS_MISSION_OFFLINE");
    return 0;
}

#else

int cos_mission_self_test(void)
{
    return 0;
}

#endif
