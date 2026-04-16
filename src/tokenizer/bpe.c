/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include <string.h>

#include "cos_tokenizer.h"

static uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void bpe_default_merges(BpeMergeTable *t)
{
    memset(t, 0, sizeof(*t));
    /* Toy merges on byte-token space (Tier-1 stand-in for trained BPE). */
    t->n_merges = 8;
    t->left[0] = (uint32_t)'t' + 1;
    t->right[0] = (uint32_t)'h' + 1;
    t->left[1] = (uint32_t)'h' + 1;
    t->right[1] = (uint32_t)'e' + 1;
    t->left[2] = (uint32_t)'e' + 1;
    t->right[2] = (uint32_t)'l' + 1;
    t->left[3] = (uint32_t)'l' + 1;
    t->right[3] = (uint32_t)'l' + 1;
    t->left[4] = (uint32_t)'i' + 1;
    t->right[4] = (uint32_t)'n' + 1;
    t->left[5] = (uint32_t)'n' + 1;
    t->right[5] = (uint32_t)'g' + 1;
    t->left[6] = (uint32_t)'o' + 1;
    t->right[6] = (uint32_t)'s' + 1;
    t->left[7] = (uint32_t)' ' + 1;
    t->right[7] = (uint32_t)'c' + 1;
}

static int apply_one_merge(uint32_t *buf, int n, uint32_t L, uint32_t R, uint32_t out_id)
{
    int w = 0;
    for (int i = 0; i < n; i++) {
        if (i + 1 < n && buf[i] == L && buf[i + 1] == R) {
            buf[w++] = out_id;
            i++;
        } else {
            buf[w++] = buf[i];
        }
    }
    return w;
}

int bpe_encode_greedy(const char *text, uint32_t *ids, int max_ids, const BpeMergeTable *merges)
{
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p && n < max_ids; p++) {
        if (*p > 0)
            ids[n++] = (uint32_t)*p + 1u; /* reserve 0 */
    }
    uint32_t tmp[COS_TOK_MAX_IDS];
    memcpy(tmp, ids, (size_t)n * sizeof(uint32_t));
    int cur = n;
    for (uint32_t m = 0; m < merges->n_merges; m++) {
        uint32_t L = merges->left[m];
        uint32_t R = merges->right[m];
        uint32_t nid = 300u + m; /* synthetic merged ids */
        cur = apply_one_merge(tmp, cur, L, R, nid);
        if (cur > COS_TOK_MAX_IDS)
            cur = COS_TOK_MAX_IDS;
    }
    n = cur < max_ids ? cur : max_ids;
    memcpy(ids, tmp, (size_t)n * sizeof(uint32_t));
    return n;
}

void bpe_id_to_hypervector(uint32_t id, uint64_t seed, uint64_t *hv)
{
    uint64_t st = seed ^ ((uint64_t)id * 0xD6E8FEB8669FD5D5ULL);
    for (int i = 0; i < COS_W; i++)
        hv[i] = splitmix64(&st);
}
