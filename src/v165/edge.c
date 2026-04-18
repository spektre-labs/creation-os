/*
 * v165 σ-Edge — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "edge.h"

#include <stdio.h>
#include <string.h>

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

const char *cos_v165_target_name(cos_v165_target_t t) {
    switch (t) {
        case COS_V165_TARGET_MACBOOK_M3:  return "macbook_m3";
        case COS_V165_TARGET_RPI5:        return "rpi5";
        case COS_V165_TARGET_JETSON_ORIN: return "jetson_orin";
        case COS_V165_TARGET_ANDROID:     return "android";
        case COS_V165_TARGET_IOS:         return "ios";
        default:                          return "?";
    }
}

void cos_v165_profiles_init(cos_v165_profile_t out[COS_V165_N_PROFILES]) {
    memset(out, 0, sizeof(cos_v165_profile_t) * COS_V165_N_PROFILES);

    /* macbook_m3 — full reference host */
    out[0].target = COS_V165_TARGET_MACBOOK_M3;
    safe_copy(out[0].name,        sizeof(out[0].name),        "macbook_m3");
    safe_copy(out[0].arch,        sizeof(out[0].arch),        "arm64");
    safe_copy(out[0].triple,      sizeof(out[0].triple),      "arm64-apple-darwin");
    safe_copy(out[0].make_target, sizeof(out[0].make_target), "cos-host");
    out[0].total_ram_mb     = 16384;
    out[0].available_ram_mb =  8192;
    out[0].has_gpu          = true;   /* Metal */
    out[0].has_camera       = true;
    out[0].default_port     = 8080;
    out[0].tau_scale        = 1.0f;
    out[0].supported_in_v0  = true;

    /* rpi5 */
    out[1].target = COS_V165_TARGET_RPI5;
    safe_copy(out[1].name,        sizeof(out[1].name),        "rpi5");
    safe_copy(out[1].arch,        sizeof(out[1].arch),        "arm64");
    safe_copy(out[1].triple,      sizeof(out[1].triple),      "aarch64-linux-gnu");
    safe_copy(out[1].make_target, sizeof(out[1].make_target), "cos-lite-rpi5");
    out[1].total_ram_mb     = 8192;
    out[1].available_ram_mb = 6144;
    out[1].has_gpu          = false;
    out[1].has_camera       = false;
    out[1].default_port     = 8080;
    out[1].tau_scale        = 0.75f;
    out[1].supported_in_v0  = true;

    /* jetson_orin */
    out[2].target = COS_V165_TARGET_JETSON_ORIN;
    safe_copy(out[2].name,        sizeof(out[2].name),        "jetson_orin");
    safe_copy(out[2].arch,        sizeof(out[2].arch),        "arm64");
    safe_copy(out[2].triple,      sizeof(out[2].triple),      "aarch64-linux-gnu");
    safe_copy(out[2].make_target, sizeof(out[2].make_target), "cos-lite-jetson");
    out[2].total_ram_mb     = 8192;
    out[2].available_ram_mb = 6656;
    out[2].has_gpu          = true;   /* Orin GPU */
    out[2].has_camera       = true;   /* CSI camera */
    out[2].default_port     = 8080;
    out[2].tau_scale        = 0.85f;
    out[2].supported_in_v0  = true;

    /* android */
    out[3].target = COS_V165_TARGET_ANDROID;
    safe_copy(out[3].name,        sizeof(out[3].name),        "android");
    safe_copy(out[3].arch,        sizeof(out[3].arch),        "arm64");
    safe_copy(out[3].triple,      sizeof(out[3].triple),      "aarch64-linux-android");
    safe_copy(out[3].make_target, sizeof(out[3].make_target), "cos-lite-android");
    out[3].total_ram_mb     = 6144;
    out[3].available_ram_mb = 3072;
    out[3].has_gpu          = false;   /* Vulkan compute via v165.1 */
    out[3].has_camera       = false;
    out[3].default_port     = 8080;
    out[3].tau_scale        = 0.55f;
    out[3].supported_in_v0  = true;

    /* ios — declared but not enabled in v0 */
    out[4].target = COS_V165_TARGET_IOS;
    safe_copy(out[4].name,        sizeof(out[4].name),        "ios");
    safe_copy(out[4].arch,        sizeof(out[4].arch),        "arm64");
    safe_copy(out[4].triple,      sizeof(out[4].triple),      "arm64-apple-ios");
    safe_copy(out[4].make_target, sizeof(out[4].make_target), "cos-lite-ios");
    out[4].total_ram_mb     = 6144;
    out[4].available_ram_mb = 2560;
    out[4].has_gpu          = true;    /* Metal */
    out[4].has_camera       = true;
    out[4].default_port     = 8080;
    out[4].tau_scale        = 0.45f;
    out[4].supported_in_v0  = false;
}

const cos_v165_profile_t *cos_v165_profile_get(cos_v165_target_t t) {
    static cos_v165_profile_t profiles[COS_V165_N_PROFILES];
    static int initialized;
    if (!initialized) { cos_v165_profiles_init(profiles); initialized = 1; }
    for (int i = 0; i < COS_V165_N_PROFILES; ++i)
        if (profiles[i].target == t) return &profiles[i];
    return NULL;
}

float cos_v165_tau_edge(float tau_default, int available_ram_mb) {
    float ratio = (float)available_ram_mb / 8192.0f;
    /* small device → tau scales *up* to produce more abstains.
     * model: tau_edge = tau_default / max(ratio, 0.125) then
     * clamp so extremely tiny devices don't drive τ above 1.0. */
    float eff = ratio < 0.125f ? 0.125f : ratio;
    float tau = tau_default / eff;
    /* but then blend with a floor so τ never collapses to 0 on
     * a huge box */
    if (tau < 0.15f) tau = 0.15f;
    if (tau > 1.00f) tau = 1.00f;
    return tau;
}

cos_v165_fit_t cos_v165_fit(const cos_v165_profile_t *p,
                            const cos_v165_budget_t  *b,
                            float tau_default) {
    cos_v165_fit_t r;
    memset(&r, 0, sizeof(r));
    if (!p || !b) return r;
    r.profile = *p;
    r.tau_default = tau_default;
    r.tau_edge = cos_v165_tau_edge(tau_default, p->available_ram_mb);
    int used = b->binary_mb + b->weights_mb + b->kvcache_mb + b->sigma_overhead_mb;
    r.total_used_mb = used;
    r.headroom_mb   = p->available_ram_mb - used;
    r.fits          = r.headroom_mb >= 0;
    if (r.fits) {
        snprintf(r.note, COS_V165_MAX_MSG,
                 "fits: %d MB used / %d MB avail, headroom %d MB",
                 used, p->available_ram_mb, r.headroom_mb);
    } else {
        snprintf(r.note, COS_V165_MAX_MSG,
                 "OOM: %d MB used > %d MB avail (short %d MB)",
                 used, p->available_ram_mb, -r.headroom_mb);
    }
    (void)clampf;  /* reserved */
    return r;
}

cos_v165_crosscompile_t cos_v165_crosscompile(cos_v165_target_t t) {
    cos_v165_crosscompile_t cc;
    memset(&cc, 0, sizeof(cc));
    cc.target = t;
    const cos_v165_profile_t *p = cos_v165_profile_get(t);
    if (!p) return cc;
    safe_copy(cc.cos_lite_target, sizeof(cc.cos_lite_target), p->make_target);
    safe_copy(cc.triple,          sizeof(cc.triple),          p->triple);
    /* arm64 on an x86 host requires QEMU in CI.  macOS arm64
     * host + arm64 targets are native, so flag macbook+ios as
     * "no QEMU needed". */
    cc.needs_qemu = !(t == COS_V165_TARGET_MACBOOK_M3 || t == COS_V165_TARGET_IOS);
    return cc;
}

cos_v165_boot_receipt_t cos_v165_boot_cos_lite(cos_v165_target_t t,
                                               float tau_default) {
    cos_v165_boot_receipt_t r;
    memset(&r, 0, sizeof(r));
    r.target = t;
    const cos_v165_profile_t *p = cos_v165_profile_get(t);
    if (!p) {
        safe_copy(r.boot_msg, sizeof(r.boot_msg), "unknown target");
        return r;
    }
    /* default cos-lite budget: 5 MB binary, 1200 MB weights
     * (BitNet 2B GGUF), 256 MB KV-cache, 64 MB σ overhead */
    cos_v165_budget_t b = { .binary_mb = 5, .weights_mb = 1200,
                             .kvcache_mb = 256, .sigma_overhead_mb = 64 };
    r.fit = cos_v165_fit(p, &b, tau_default);
    r.server_port = p->default_port;
    if (!p->supported_in_v0) {
        r.booted = false;
        snprintf(r.boot_msg, sizeof(r.boot_msg),
                 "target %s reserved for v165.1", p->name);
    } else if (!r.fit.fits) {
        r.booted = false;
        safe_copy(r.boot_msg, sizeof(r.boot_msg),
                  "cos-lite refuses: OOM under default budget");
    } else {
        r.booted = true;
        snprintf(r.boot_msg, sizeof(r.boot_msg),
                 "cos-lite booted on %s, τ_edge=%.3f, port=%d",
                 p->name, (double)r.fit.tau_edge, r.server_port);
    }
    return r;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

static size_t emit_profile_json(char *buf, size_t cap,
                                const cos_v165_profile_t *p,
                                float tau_default,
                                const cos_v165_budget_t *budget) {
    cos_v165_fit_t f = cos_v165_fit(p, budget, tau_default);
    cos_v165_crosscompile_t cc = cos_v165_crosscompile(p->target);
    int n = snprintf(buf, cap,
        "{\"name\":\"%s\",\"target\":\"%s\",\"arch\":\"%s\","
        "\"triple\":\"%s\",\"make_target\":\"%s\","
        "\"total_ram_mb\":%d,\"available_ram_mb\":%d,"
        "\"has_gpu\":%s,\"has_camera\":%s,\"default_port\":%d,"
        "\"tau_scale\":%.4f,\"tau_edge\":%.4f,"
        "\"supported_in_v0\":%s,"
        "\"fit\":{\"total_used_mb\":%d,\"headroom_mb\":%d,\"fits\":%s},"
        "\"crosscompile\":{\"triple\":\"%s\",\"needs_qemu\":%s}}",
        p->name, cos_v165_target_name(p->target), p->arch,
        p->triple, p->make_target,
        p->total_ram_mb, p->available_ram_mb,
        p->has_gpu ? "true" : "false",
        p->has_camera ? "true" : "false",
        p->default_port,
        (double)p->tau_scale, (double)f.tau_edge,
        p->supported_in_v0 ? "true" : "false",
        f.total_used_mb, f.headroom_mb, f.fits ? "true" : "false",
        cc.triple, cc.needs_qemu ? "true" : "false");
    if (n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}

size_t cos_v165_profiles_to_json(char *buf, size_t cap,
                                 float tau_default,
                                 const cos_v165_budget_t *default_budget) {
    cos_v165_profile_t profiles[COS_V165_N_PROFILES];
    cos_v165_profiles_init(profiles);

    cos_v165_budget_t fallback = { 5, 1200, 256, 64 };
    if (!default_budget) default_budget = &fallback;

    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
                     "{\"kernel\":\"v165\",\"tau_default\":%.4f,"
                     "\"default_budget\":{\"binary_mb\":%d,"
                     "\"weights_mb\":%d,\"kvcache_mb\":%d,"
                     "\"sigma_overhead_mb\":%d},\"profiles\":[",
                     (double)tau_default,
                     default_budget->binary_mb,
                     default_budget->weights_mb,
                     default_budget->kvcache_mb,
                     default_budget->sigma_overhead_mb);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;

    for (int i = 0; i < COS_V165_N_PROFILES; ++i) {
        if (i > 0) {
            if (used + 1 >= cap) return 0;
            buf[used++] = ',';
        }
        size_t k = emit_profile_json(buf + used, cap - used,
                                     &profiles[i], tau_default,
                                     default_budget);
        if (k == 0) return 0;
        used += k;
    }
    if (used + 2 >= cap) return 0;
    buf[used++] = ']';
    buf[used++] = '}';
    buf[used] = '\0';
    return used;
}

size_t cos_v165_boot_to_json(const cos_v165_boot_receipt_t *r,
                             char *buf, size_t cap) {
    if (!r || !buf) return 0;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v165\",\"event\":\"boot\","
        "\"target\":\"%s\",\"booted\":%s,\"server_port\":%d,"
        "\"tau_edge\":%.4f,\"total_used_mb\":%d,\"headroom_mb\":%d,"
        "\"fits\":%s,\"note\":\"%s\"}",
        cos_v165_target_name(r->target),
        r->booted ? "true" : "false",
        r->server_port,
        (double)r->fit.tau_edge,
        r->fit.total_used_mb, r->fit.headroom_mb,
        r->fit.fits ? "true" : "false",
        r->boot_msg);
    if (n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v165_self_test(void) {
    cos_v165_profile_t profs[COS_V165_N_PROFILES];
    cos_v165_profiles_init(profs);
    if (profs[1].target != COS_V165_TARGET_RPI5) return 1;

    /* τ_edge: a tiny 2 GB device must raise τ above 0.5 when
     * τ_default is 0.5 */
    float tau = cos_v165_tau_edge(0.5f, 2048);
    if (!(tau > 0.5f)) return 2;

    /* 16 GB device keeps τ roughly at τ_default */
    float tau_big = cos_v165_tau_edge(0.5f, 16384);
    if (!(tau_big <= 0.51f)) return 3;

    /* boot rpi5 under default budget: must fit (avail 6144 >
     * 5+1200+256+64 = 1525). */
    cos_v165_boot_receipt_t r = cos_v165_boot_cos_lite(
        COS_V165_TARGET_RPI5, 0.5f);
    if (!r.booted) return 4;
    if (r.server_port != 8080) return 5;

    /* android is tighter: 3072 MB avail; default budget still fits. */
    r = cos_v165_boot_cos_lite(COS_V165_TARGET_ANDROID, 0.5f);
    if (!r.booted) return 6;

    /* iOS is v165.1 — should refuse */
    r = cos_v165_boot_cos_lite(COS_V165_TARGET_IOS, 0.5f);
    if (r.booted) return 7;

    /* fit refusal: impossible budget */
    cos_v165_budget_t huge = { .binary_mb = 5, .weights_mb = 99999,
                                .kvcache_mb = 0, .sigma_overhead_mb = 0 };
    cos_v165_fit_t f = cos_v165_fit(&profs[1], &huge, 0.5f);
    if (f.fits) return 8;

    /* crosscompile: rpi5 needs QEMU on an x86 CI */
    cos_v165_crosscompile_t cc = cos_v165_crosscompile(COS_V165_TARGET_RPI5);
    if (!cc.needs_qemu) return 9;

    return 0;
}
