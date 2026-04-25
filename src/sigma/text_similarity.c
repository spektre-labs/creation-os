/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "text_similarity.h"

#include <stdio.h>
#include <string.h>

#define MAX_WORDS   256
#define MAX_WORD_LEN 64
#define MAX_WORDS_JACCARD 200

static const char *STOP_WORDS[] = {
    "the", "a", "an", "is", "are", "was", "were", "be", "been",
    "it", "its", "this", "that", "these", "those",
    "of", "in", "to", "for", "on", "at", "by", "with", "from",
    "and", "or", "but", "not", "no", "so", "if", "as",
    "i", "you", "he", "she", "we", "they", "my", "your",
    "do", "does", "did", "has", "have", "had", "will", "would",
    "can", "could", "may", "might", "shall", "should",
    NULL
};

static const char *NUM_WORDS[][2] = {
    {"zero", "0"},   {"one", "1"},     {"two", "2"},   {"three", "3"},
    {"four", "4"},   {"five", "5"},    {"six", "6"},   {"seven", "7"},
    {"eight", "8"},  {"nine", "9"},   {"ten", "10"},
    {"eleven", "11"}, {"twelve", "12"}, {"thirteen", "13"},
    {"hundred", "100"}, {"thousand", "1000"},
    {NULL, NULL}
};

void cos_text_normalize(const char *in, char *out, int out_len)
{
    int         j        = 0;
    int         in_space = 1;
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

static int is_stop_word(const char *word)
{
    if (word == NULL || word[0] == '\0')
        return 0;
    for (int i = 0; STOP_WORDS[i] != NULL; ++i) {
        if (strcmp(word, STOP_WORDS[i]) == 0)
            return 1;
    }
    return 0;
}

static void normalize_number_word(char *word)
{
    if (word == NULL || word[0] == '\0')
        return;
    for (int i = 0; NUM_WORDS[i][0] != NULL; ++i) {
        if (strcmp(word, NUM_WORDS[i][0]) == 0) {
            (void)strncpy(word, NUM_WORDS[i][1], (size_t)MAX_WORD_LEN - 1u);
            word[MAX_WORD_LEN - 1] = '\0';
            return;
        }
    }
}

/** Writes non-stop tokens into words[][]. Returns count (may be 0). */
static int tokenize_filtered(const char *text, char words[][MAX_WORD_LEN],
                             int max)
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
        normalize_number_word(words[n]);
        if (!is_stop_word(words[n]))
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
    int  na = tokenize_filtered(norm_a, wa, MAX_WORDS_JACCARD);
    int  nb = tokenize_filtered(norm_b, wb, MAX_WORDS_JACCARD);

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
    int   pass = 0, fail = 0;
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

    s = cos_text_jaccard("eight legs", "8 legs");
    if (s > 0.99f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL number_norm (got %g)\n", (double)s);
    }

    s = cos_text_jaccard("the answer is yes", "yes is the answer");
    if (s > 0.99f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL stop_reorder (got %g)\n", (double)s);
    }

    s = cos_text_jaccard("4", "the answer to 2 2 is 4");
    if (s > 0.0f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL short_long (got %g)\n", (double)s);
    }

    s = cos_text_jaccard("spider legs eight", "quantum gravity theory");
    if (s < 0.01f)
        pass++;
    else {
        fail++;
        fprintf(stderr, "FAIL unrelated (got %g)\n", (double)s);
    }

    fprintf(stderr, "text_similarity: %d pass, %d fail\n", pass, fail);
    return fail;
}

void cos_text_similarity_self_test(void)
{
    (void)cos_text_similarity_self_check();
}
