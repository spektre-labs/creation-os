/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "text_similarity.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAX_WORDS   256
#define MAX_WORD_LEN 64
#define MAX_WORDS_JACCARD 200

void cos_text_normalize(const char *in, char *out, int out_len)
{
    int j       = 0;
    int in_space = 1;
    if (out == NULL || out_len < 2)
        return;
    out[0] = '\0';
    if (in == NULL)
        return;
    for (int i = 0; in[i] != '\0' && j < out_len - 1; ++i) {
        unsigned char uc = (unsigned char)in[i];
        char          c  = (char)uc;
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + 32);
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' '))
            continue;
        if (c == ' ') {
            if (!in_space && j > 0)
                out[j++] = ' ';
            in_space = 1;
        } else {
            out[j++] = c;
            in_space = 0;
        }
    }
    while (j > 0 && out[j - 1] == ' ')
        j--;
    out[j] = '\0';
}

static int tokenize(const char *text, char words[][MAX_WORD_LEN], int max)
{
    int         n = 0;
    const char *p = text;
    while (*p != '\0' && n < max) {
        while (*p == ' ')
            p++;
        if (*p == '\0')
            break;
        int len = 0;
        while (*p != '\0' && *p != ' ' && len < MAX_WORD_LEN - 1)
            words[n][len++] = *p++;
        words[n][len] = '\0';
        n++;
    }
    return n;
}

static int word_in_set(const char *word, char set[][MAX_WORD_LEN], int n)
{
    for (int i = 0; i < n; ++i) {
        if (strcmp(word, set[i]) == 0)
            return 1;
    }
    return 0;
}

static int unique_words(char words[][MAX_WORD_LEN], int n,
                        char out[][MAX_WORD_LEN])
{
    int m = 0;
    for (int i = 0; i < n; ++i) {
        if (!word_in_set(words[i], out, m)) {
            if (m >= MAX_WORDS)
                break;
            (void)strncpy(out[m], words[i], (size_t)MAX_WORD_LEN - 1u);
            out[m][MAX_WORD_LEN - 1] = '\0';
            m++;
        }
    }
    return m;
}

float cos_text_jaccard(const char *a, const char *b)
{
    char norm_a[4096], norm_b[4096];
    cos_text_normalize(a != NULL ? a : "", norm_a, (int)sizeof norm_a);
    cos_text_normalize(b != NULL ? b : "", norm_b, (int)sizeof norm_b);

    char wa[MAX_WORDS][MAX_WORD_LEN];
    char wb[MAX_WORDS][MAX_WORD_LEN];
    int  na = tokenize(norm_a, wa, MAX_WORDS_JACCARD);
    int  nb = tokenize(norm_b, wb, MAX_WORDS_JACCARD);

    char ua[MAX_WORDS][MAX_WORD_LEN];
    char ub[MAX_WORDS][MAX_WORD_LEN];
    int  nua = unique_words(wa, na, ua);
    int  nub = unique_words(wb, nb, ub);

    if (nua == 0 && nub == 0)
        return 1.0f;
    if (nua == 0 || nub == 0)
        return 0.0f;

    int inter = 0;
    for (int i = 0; i < nua; ++i) {
        if (word_in_set(ua[i], ub, nub))
            inter++;
    }
    int uni = nua + nub - inter;
    if (uni <= 0)
        return 0.0f;
    return (float)inter / (float)uni;
}

int cos_text_similarity_self_check(void)
{
    int  pass = 0, fail = 0;
    float s;

    s = cos_text_jaccard("hello world", "hello world");
    if (s > 0.99f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL identical\n");
    }

    s = cos_text_jaccard("hello world", "foo bar");
    if (s < 0.01f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL no_overlap\n");
    }

    s = cos_text_jaccard("the cat sat", "the dog sat");
    if (s > 0.3f && s < 0.8f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL partial (got %g)\n", (double)s);
    }

    s = cos_text_jaccard("Hello World", "hello world");
    if (s > 0.99f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL case\n");
    }

    s = cos_text_jaccard("hello, world!", "hello world");
    if (s > 0.99f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL punct\n");
    }

    s = cos_text_jaccard("", "");
    if (s > 0.99f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL empty\n");
    }

    s = cos_text_jaccard("hello", "");
    if (s < 0.01f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL one_empty\n");
    }

    s = cos_text_jaccard("8", "a spider has 8 legs");
    if (s > 0.1f && s < 0.5f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL containment (got %g)\n", (double)s);
    }

    fprintf(stderr, "text_similarity: %d pass, %d fail\n", pass, fail);
    return fail;
}

void cos_text_similarity_self_test(void)
{
    (void)cos_text_similarity_self_check();
}
