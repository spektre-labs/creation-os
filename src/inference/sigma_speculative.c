/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/* ACSL companion (clause ledger): hw/formal/v133/sigma_stack_contracts.acsl */
#include "sigma_speculative.h"
#include <stdint.h>
#include <string.h>

static int models_share_weights(const inference_state_t *a,
                               const inference_state_t *b)
{
    const ternary_model_t *ma;
    const ternary_model_t *mb;
    if (!a || !b)
        return 0;
    ma = &a->model;
    mb = &b->model;
    if (ma->embed != mb->embed)
        return 0;
    if (ma->layers != mb->layers)
        return 0;
    if (ma->n_layer_blocks != mb->n_layer_blocks
        || ma->hidden_dim != mb->hidden_dim || ma->vocab_size != mb->vocab_size
        || ma->head_dim != mb->head_dim)
        return 0;
    return 1;
}

static void copy_kv_trace(inference_state_t *dst, const inference_state_t *src)
{
    size_t plane;
    if (!dst || !src || !models_share_weights(dst, src))
        return;
    plane = (size_t)src->model.n_layer_blocks * (size_t)src->max_cache
            * (size_t)src->model.head_dim * sizeof(int32_t);
    if (src->kv_cache_k && dst->kv_cache_k && plane > 0) {
        memcpy(dst->kv_cache_k, src->kv_cache_k, plane);
        memcpy(dst->kv_cache_v, src->kv_cache_v, plane);
    }
    dst->cache_len = src->cache_len;
}

void sigma_speculative_init(sigma_speculative_t *s, inference_state_t *draft,
                           inference_state_t *target, int32_t draft_len,
                           int32_t sigma_skip_tau_q16)
{
    if (!s)
        return;
    memset(s, 0, sizeof *s);
    s->draft               = draft;
    s->target              = target;
    s->draft_len           = draft_len;
    s->sigma_skip_tau_q16  = sigma_skip_tau_q16;
    cos_sigma_q16_init(&s->gate);
    if (draft && target)
        s->same_geometry = models_share_weights(draft, target);
}

int32_t sigma_speculative_step(sigma_speculative_t *spec, int32_t cur_token,
                              int32_t *draft_tokens_out, int32_t draft_cap,
                              int32_t *accepted, int32_t accepted_cap)
{
    inference_state_t *dr;
    inference_state_t *tg;
    int32_t dlen;
    int32_t k;
    int32_t n_acc;
    int32_t cur;
    int32_t td;
    int32_t sig;
    int32_t k_raw;
    cos_sigma_verdict_t ver;
    int32_t skip;
    int32_t tt;

    if (!spec || !spec->draft || !spec->target || !accepted || accepted_cap < 1)
        return 0;
    dr   = spec->draft;
    tg   = spec->target;
    dlen = spec->draft_len;
    if (dlen < 1)
        dlen = 1;
    if (draft_cap < dlen)
        dlen = draft_cap;

    inference_state_init(dr, &dr->model, dr->kv_cache_k, dr->kv_cache_v,
                        dr->max_cache);
    inference_state_init(tg, &tg->model, tg->kv_cache_k, tg->kv_cache_v,
                        tg->max_cache);

    k_raw = (int32_t)((int64_t)9 * COS_SIGMA_Q16_ONE / 10);
    cur   = cur_token;
    n_acc = 0;

    for (k = 0; k < dlen; k++) {
        td = generate_token(dr, cur);
        if (draft_tokens_out)
            draft_tokens_out[k] = td;
        sig = dr->gate.sigma;
        cos_sigma_q16_update(&spec->gate, sig, k_raw);
        ver  = cos_sigma_mirror_verdict(&spec->gate);
        skip = spec->same_geometry && (ver == COS_SIGMA_VERDICT_ACCEPT)
            && (sig < spec->sigma_skip_tau_q16);

        if (skip) {
            accepted[n_acc++] = td;
            spec->target_skipped++;
            copy_kv_trace(tg, dr);
            cur = td;
            if (n_acc >= accepted_cap)
                break;
            continue;
        }

        spec->target_called++;
        tt = generate_token(tg, cur);
        if (ver == COS_SIGMA_VERDICT_RETHINK && tt == td) {
            accepted[n_acc++] = td;
            cur = td;
            copy_kv_trace(dr, tg);
        } else if (ver == COS_SIGMA_VERDICT_RETHINK) {
            accepted[n_acc++] = tt;
            cur = tt;
            copy_kv_trace(dr, tg);
            break;
        } else {
            accepted[n_acc++] = tt;
            cur = tt;
            copy_kv_trace(dr, tg);
            break;
        }
        if (n_acc >= accepted_cap)
            break;
    }

    return n_acc;
}

int32_t sigma_adaptive_draft_len_q16(const sigma_speculative_t *spec,
                                    int32_t base_len)
{
    uint32_t den;
    int32_t  rate_q16;
    int32_t  out;
    if (!spec || base_len < 2)
        return base_len > 0 ? base_len : 2;
    den = spec->target_skipped + spec->target_called + 1u;
    rate_q16 =
        (int32_t)(((uint64_t)spec->target_skipped * (uint64_t)COS_SIGMA_Q16_ONE)
                  / (uint64_t)den);
    out = base_len;
    if (rate_q16 > (COS_SIGMA_Q16_ONE * 8) / 10)
        out = base_len + 2;
    else if (rate_q16 > COS_SIGMA_Q16_ONE / 2)
        out = base_len;
    else if (base_len > 2)
        out = base_len - 1;
    if (out < 2)
        out = 2;
    return out;
}
