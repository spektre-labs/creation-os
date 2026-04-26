/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "prompt_bank.h"

#include "../sigma/engram_episodic.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pb_default_csv(char *buf, size_t cap)
{
    const char *r = getenv("CREATION_OS_ROOT");
    if (r != NULL && r[0] != '\0') {
        if (snprintf(buf, cap, "%s/benchmarks/graded/graded_prompts.csv", r)
            < (int)cap)
            return 0;
    }
    if (snprintf(buf, cap, "benchmarks/graded/graded_prompts.csv") < (int)cap)
        return 0;
    return -1;
}

static void pb_lower(char *dst, const char *src, size_t cap)
{
    size_t i;
    if (!dst || cap < 1) {
        if (dst)
            dst[0] = '\0';
        return;
    }
    for (i = 0; i + 1 < cap && src && src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = (char)tolower((int)c);
    }
    dst[i] = '\0';
}

static int pb_parse_line(char *line, cos_banked_prompt_t *row)
{
    char *c1, *c2;
    size_t L;

    if (row == NULL || line == NULL)
        return -1;
    L = strlen(line);
    while (L > 0 && (line[L - 1] == '\n' || line[L - 1] == '\r')) {
        line[--L] = '\0';
    }
    if (L < 5)
        return -1;
    c1 = strchr(line, ',');
    if (c1 == NULL || c1 == line)
        return -1;
    c2 = strrchr(line, ',');
    if (c2 == NULL || c2 <= c1)
        return -1;
    {
        size_t clen = (size_t)(c1 - line);
        if (clen >= sizeof row->category)
            clen = sizeof row->category - 1u;
        memcpy(row->category, line, clen);
        row->category[clen] = '\0';
    }
    {
        size_t plen = (size_t)(c2 - (c1 + 1));
        if (plen >= sizeof row->prompt)
            plen = sizeof row->prompt - 1u;
        memcpy(row->prompt, c1 + 1, plen);
        row->prompt[plen] = '\0';
    }
    snprintf(row->answer, sizeof row->answer, "%s", c2 + 1);
    row->last_sigma    = 0.f;
    row->times_seen    = 0;
    row->times_correct = 0;
    return 0;
}

int cos_prompt_bank_load(cos_prompt_bank_t *pb)
{
    char  path[1024];
    FILE *f;
    char  line[2048];

    if (!pb)
        return -1;
    memset(pb, 0, sizeof(*pb));
    if (pb_default_csv(path, sizeof path) != 0)
        return -1;
    f = fopen(path, "r");
    if (f == NULL)
        return -1;
    if (fgets(line, sizeof line, f) == NULL) {
        fclose(f);
        return -1;
    }
    while (pb->n < COS_PB_MAX && fgets(line, sizeof line, f) != NULL) {
        if (pb_parse_line(line, &pb->rows[pb->n]) == 0)
            pb->n++;
    }
    fclose(f);
    return pb->n > 0 ? 0 : -1;
}

void cos_prompt_bank_episode_begin(cos_prompt_bank_t *pb)
{
    (void)pb;
}

static int pb_seen(const uint64_t *seen, int n, uint64_t h)
{
    int i;
    for (i = 0; i < n; i++)
        if (seen[i] == h)
            return 1;
    return 0;
}

static uint64_t pb_det_u32(int ep, int t, uint32_t salt)
{
    uint64_t x = ((uint64_t)(uint32_t)ep * 0x9E3779B1ULL)
               ^ ((uint64_t)(uint32_t)t * 0x85EBCA6BULL)
               ^ ((uint64_t)salt * 0xC2B2AE3DULL);
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

int cos_prompt_bank_pick(cos_prompt_bank_t *pb, int ep, int t,
                         const uint64_t *seen_hashes, int n_seen,
                         uint64_t *out_hash)
{
    int          i, best = 0, cand[32], nc = 0;
    float        wmax = -1.f;
    uint64_t     u, h;
    int          use_weak;

    if (!pb || pb->n <= 0 || !out_hash)
        return -1;
    u        = pb_det_u32(ep, t, 0xC001D00Du);
    use_weak = (int)(u % 100u) < 70;

    if (use_weak) {
        for (i = 0; i < pb->n; i++) {
            float w = pb->rows[i].last_sigma;
            if (pb->rows[i].times_seen < 1)
                w += 0.25f;
            if (w > wmax) {
                wmax = w;
                best = i;
            }
        }
    } else {
        best = (int)(u % (uint64_t)(unsigned)pb->n);
    }

    h = cos_engram_prompt_hash(pb->rows[best].prompt);
    if (!pb_seen(seen_hashes, n_seen, h)) {
        *out_hash = h;
        return best;
    }

    for (i = 0; i < pb->n && nc < 32; i++) {
        int j = (best + 1 + i) % pb->n;
        h = cos_engram_prompt_hash(pb->rows[j].prompt);
        if (!pb_seen(seen_hashes, n_seen, h))
            cand[nc++] = j;
    }
    if (nc <= 0) {
        *out_hash = h;
        return best;
    }
    {
        int pick = (int)(pb_det_u32(ep, t, 7u) % (uint64_t)(unsigned)nc);
        best     = cand[pick];
        *out_hash = cos_engram_prompt_hash(pb->rows[best].prompt);
        return best;
    }
}

int cos_prompt_bank_grade(const char *expected, const char *category,
                          const char *response)
{
    char el[256], rl[1024];

    if (!expected || !response)
        return 0;
    (void)category;
    pb_lower(el, expected, sizeof el);
    pb_lower(rl, response, sizeof rl);
    if (strstr(rl, "abstain") != NULL || strstr(rl, "cannot answer") != NULL
        || strstr(rl, "uncertain") != NULL)
        ;
    if (strcmp(el, "any") == 0)
        return strlen(rl) >= 6 ? 1 : 0;
    if (strcmp(el, "impossible") == 0) {
        if (strstr(rl, "cannot") != NULL || strstr(rl, "uncertain") != NULL
            || strstr(rl, "don't know") != NULL
            || strstr(rl, "do not know") != NULL || strstr(rl, "impossible")
                   != NULL)
            return 1;
        return 0;
    }
    if (strstr(rl, el) != NULL)
        return 1;
    /* numeric-ish: allow substring of digits */
    if (el[0] >= '0' && el[0] <= '9') {
        const char *p = rl;
        while (*p) {
            if (strncmp(p, el, strlen(el)) == 0)
                return 1;
            ++p;
        }
    }
    return 0;
}
