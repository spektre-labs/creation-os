/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Multimodal primitive. */

#include "multimodal.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static float clamp01(float x) {
    if (x != x)        return 1.0f;   /* NaN → 1.0 (safe) */
    if (x < 0.0f)      return 0.0f;
    if (x > 1.0f)      return 1.0f;
    return x;
}

const char *cos_sigma_multimodal_label(cos_modality_t m) {
    switch (m) {
        case COS_MODAL_TEXT:        return "TEXT";
        case COS_MODAL_CODE:        return "CODE";
        case COS_MODAL_STRUCTURED:  return "STRUCTURED";
        case COS_MODAL_IMAGE:       return "IMAGE";
        case COS_MODAL_AUDIO:       return "AUDIO";
    }
    return "?";
}

float cos_sigma_measure_text(float sigma_in) {
    return clamp01(sigma_in);
}

/* Balance parens / brackets / braces; parity of double and single
 * quotes.  Tracks string state so "(" inside a string doesn't count.
 * Backslash escapes one character inside strings. */
float cos_sigma_measure_code(const char *src, size_t src_len) {
    if (!src) return 1.0f;
    if (src_len == 0) src_len = strlen(src);

    int p = 0, b = 0, c = 0;   /* () [] {} */
    int in_dq = 0, in_sq = 0;

    for (size_t i = 0; i < src_len; ++i) {
        unsigned char ch = (unsigned char)src[i];

        /* Handle backslash escapes inside either quote. */
        if (ch == '\\' && (in_dq || in_sq)) {
            if (i + 1 < src_len) ++i;   /* skip next */
            continue;
        }

        if (in_dq) {
            if (ch == '"')  in_dq = 0;
            continue;
        }
        if (in_sq) {
            if (ch == '\'') in_sq = 0;
            continue;
        }

        switch (ch) {
            case '"':   in_dq = 1; break;
            case '\'':  in_sq = 1; break;
            case '(':   ++p; break;
            case ')':   --p; break;
            case '[':   ++b; break;
            case ']':   --b; break;
            case '{':   ++c; break;
            case '}':   --c; break;
            default: break;
        }
        if (p < 0 || b < 0 || c < 0) return 1.0f;   /* closed before open */
    }
    if (p != 0 || b != 0 || c != 0 || in_dq || in_sq) return 1.0f;
    /* Syntactically balanced: small non-zero residual so downstream
     * stages still get a chance to overlay real build/test results. */
    return 0.10f;
}

/* Substring scan: does ``blob`` contain ``"<field>":`` somewhere?
 * O(blob_len × field_len); good enough for a cheap sanity check. */
static int blob_has_field(const char *blob, size_t blob_len,
                          const char *field)
{
    if (!blob || !field) return 0;
    size_t fl = strlen(field);
    if (fl == 0) return 0;
    /* Build the `"<field>":` pattern (on the stack; small). */
    char pat[128];
    if (fl + 4 >= sizeof pat) return 0;   /* refuse huge names */
    size_t pl = 0;
    pat[pl++] = '"';
    memcpy(pat + pl, field, fl); pl += fl;
    pat[pl++] = '"';
    pat[pl++] = ':';
    /* Scan.  Accepts optional whitespace between " and :. */
    for (size_t i = 0; i + pl <= blob_len; ++i) {
        /* fast check on the leading quote */
        if (blob[i] != '"') continue;
        /* require the "<field>" part followed by optional ws + ":" */
        if (memcmp(blob + i, pat, fl + 2) != 0) continue;
        size_t j = i + fl + 2;
        while (j < blob_len &&
               (blob[j] == ' ' || blob[j] == '\t' ||
                blob[j] == '\n' || blob[j] == '\r')) ++j;
        if (j < blob_len && blob[j] == ':') return 1;
    }
    return 0;
}

float cos_sigma_measure_schema(const char *blob, size_t blob_len,
                               const char *const *required,
                               int n_required) {
    if (!blob) return 1.0f;
    if (blob_len == 0) blob_len = strlen(blob);
    if (!required || n_required <= 0) return 0.10f;

    int missing = 0;
    for (int i = 0; i < n_required; ++i) {
        if (!required[i] || !blob_has_field(blob, blob_len, required[i]))
            ++missing;
    }
    /* Linear degradation: missing fraction maps to σ in [0.10, 1.0]. */
    float frac = (float)missing / (float)n_required;
    float sig  = 0.10f + 0.90f * frac;
    return clamp01(sig);
}

float cos_sigma_measure_image(float similarity) {
    if (similarity != similarity) return 1.0f;
    return clamp01(1.0f - similarity);
}

float cos_sigma_measure_audio(float similarity) {
    return cos_sigma_measure_image(similarity);
}

float cos_sigma_measure_multimodal(cos_modality_t m,
                                   const void *extra, size_t len) {
    switch (m) {
        case COS_MODAL_TEXT: {
            if (!extra) return 1.0f;
            float s = *(const float *)extra;
            return cos_sigma_measure_text(s);
        }
        case COS_MODAL_CODE:
            return cos_sigma_measure_code((const char *)extra, len);
        case COS_MODAL_STRUCTURED:
            /* Dispatcher can't pass required[] — caller should use the
             * typed entry point.  Returning 0.10 as a balanced default. */
            if (!extra) return 1.0f;
            return 0.10f;
        case COS_MODAL_IMAGE:
        case COS_MODAL_AUDIO: {
            if (!extra) return 1.0f;
            float s = *(const float *)extra;
            return (m == COS_MODAL_IMAGE)
                ? cos_sigma_measure_image(s)
                : cos_sigma_measure_audio(s);
        }
    }
    return 1.0f;
}

/* --- self-test --------------------------------------------------------- */

static int check_text(void) {
    if (cos_sigma_measure_text(0.0f)  != 0.0f) return 10;
    if (cos_sigma_measure_text(0.5f)  != 0.5f) return 11;
    if (cos_sigma_measure_text(1.0f)  != 1.0f) return 12;
    if (cos_sigma_measure_text(-0.1f) != 0.0f) return 13;
    if (cos_sigma_measure_text(2.0f)  != 1.0f) return 14;
    float nan_v = 0.0f / 0.0f;
    if (cos_sigma_measure_text(nan_v) != 1.0f) return 15;
    return 0;
}

static int check_code(void) {
    /* Balanced. */
    const char *good = "int f(int x) { return x * 2; }";
    if (cos_sigma_measure_code(good, 0) != 0.10f) return 20;

    /* Unbalanced paren. */
    const char *bad1 = "int f(int x { return x; }";
    if (cos_sigma_measure_code(bad1, 0) != 1.0f) return 21;

    /* Unbalanced brace. */
    const char *bad2 = "int f(int x) { return x;";
    if (cos_sigma_measure_code(bad2, 0) != 1.0f) return 22;

    /* Close before open. */
    const char *bad3 = "}{";
    if (cos_sigma_measure_code(bad3, 0) != 1.0f) return 23;

    /* Parens inside a string shouldn't count. */
    const char *sgood = "printf(\"( ( ( \");";
    if (cos_sigma_measure_code(sgood, 0) != 0.10f) return 24;

    /* Unterminated string. */
    const char *sbad = "printf(\"hello";
    if (cos_sigma_measure_code(sbad, 0) != 1.0f) return 25;

    /* Backslash-escaped quote in string. */
    const char *egood = "x = \"he said \\\"hi\\\"\";";
    if (cos_sigma_measure_code(egood, 0) != 0.10f) return 26;

    /* Char literal with escaped quote. */
    const char *cgood = "c = '\\'';";
    if (cos_sigma_measure_code(cgood, 0) != 0.10f) return 27;

    if (cos_sigma_measure_code(NULL, 0) != 1.0f) return 28;
    return 0;
}

static int check_schema(void) {
    const char *blob =
        "{\"id\":1,\"name\":\"alice\",\"age\":30}";
    const char *required_ok[] = {"id", "name"};
    if (cos_sigma_measure_schema(blob, 0, required_ok, 2) != 0.10f)
        return 30;

    const char *required_miss1[] = {"id", "name", "email"};
    float s = cos_sigma_measure_schema(blob, 0, required_miss1, 3);
    /* 1 missing / 3 → 0.10 + 0.90·(1/3) = 0.40 */
    if (s < 0.39f || s > 0.41f) return 31;

    const char *required_miss_all[] = {"a", "b", "c"};
    float s2 = cos_sigma_measure_schema(blob, 0, required_miss_all, 3);
    if (s2 < 0.99f || s2 > 1.001f) return 32;

    /* NULL blob → 1.0. */
    if (cos_sigma_measure_schema(NULL, 0, required_ok, 2) != 1.0f)
        return 33;

    /* NULL required → 0.10 (neutral). */
    if (cos_sigma_measure_schema(blob, 0, NULL, 0) != 0.10f)
        return 34;

    /* field "id" must not match inside a string value "id" — the
     * predicate requires the trailing colon. */
    const char *tricky = "{\"note\":\"mentions id only in text\"}";
    const char *req_id[] = {"id"};
    float s3 = cos_sigma_measure_schema(tricky, 0, req_id, 1);
    /* "id" is not a key → missing → σ = 1.0.  */
    if (s3 < 0.99f || s3 > 1.001f) return 35;
    return 0;
}

static int check_image_audio(void) {
    /* σ = 1 − clamp(s). */
    if (cos_sigma_measure_image(1.00f) != 0.00f) return 40;
    if (cos_sigma_measure_image(0.50f) != 0.50f) return 41;
    if (cos_sigma_measure_image(0.00f) != 1.00f) return 42;
    if (cos_sigma_measure_image(-0.5f) != 1.00f) return 43;
    if (cos_sigma_measure_image( 1.5f) != 0.00f) return 44;
    float nan_v = 0.0f / 0.0f;
    if (cos_sigma_measure_image(nan_v) != 1.00f) return 45;
    if (cos_sigma_measure_audio(0.75f) != 0.25f) return 46;
    return 0;
}

static int check_dispatch(void) {
    float s = 0.42f;
    if (cos_sigma_measure_multimodal(COS_MODAL_TEXT,  &s, sizeof s) != 0.42f)
        return 50;
    float sim = 0.9f;
    if (cos_sigma_measure_multimodal(COS_MODAL_IMAGE, &sim, sizeof sim)
        < 0.099f
     || cos_sigma_measure_multimodal(COS_MODAL_IMAGE, &sim, sizeof sim)
        > 0.101f)
        return 51;
    const char *code = "void f(){}";
    if (cos_sigma_measure_multimodal(COS_MODAL_CODE,
                                     code, strlen(code)) != 0.10f)
        return 52;
    /* NULL extra → safe-default 1.0. */
    if (cos_sigma_measure_multimodal(COS_MODAL_TEXT, NULL, 0) != 1.0f)
        return 53;
    return 0;
}

int cos_sigma_multimodal_self_test(void) {
    int rc;
    if ((rc = check_text()))          return rc;
    if ((rc = check_code()))          return rc;
    if ((rc = check_schema()))        return rc;
    if ((rc = check_image_audio()))   return rc;
    if ((rc = check_dispatch()))      return rc;
    return 0;
}
