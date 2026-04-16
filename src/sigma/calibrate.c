/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "calibrate.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float sigma_platt_p_correct(float sigma_raw, float a, float b)
{
    double z = (double)a * (double)sigma_raw + (double)b;
    if (z > 60.0)
        return 1.0f;
    if (z < -60.0)
        return 0.0f;
    double e = exp(z);
    return (float)(e / (1.0 + e));
}

static void skip_ws(const char **pp)
{
    while (*pp && isspace((unsigned char)**pp))
        (*pp)++;
}

static int extract_json_float_simple(const char *json, const char *key, float *out)
{
    char pat[96];
    if (snprintf(pat, sizeof pat, "\"%s\"", key) >= (int)sizeof pat)
        return -1;
    const char *p = strstr(json, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    skip_ws(&p);
    if (*p != ':')
        return -1;
    p++;
    skip_ws(&p);
    errno = 0;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p || errno == ERANGE)
        return -1;
    *out = (float)v;
    return 0;
}

int sigma_platt_load(const char *path, sigma_platt_params_t *out)
{
    if (!out)
        return -1;
    out->a = -1.0f;
    out->b = 0.0f;
    out->valid = 0;
    if (!path || !*path)
        return 0;

    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0 || sz > 1 << 20) {
        fclose(f);
        return -1;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return -1;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    float a, b;
    if (extract_json_float_simple(buf, "platt_a", &a) == 0) {
        out->a = a;
        out->valid = 1;
    }
    if (extract_json_float_simple(buf, "platt_b", &b) == 0) {
        out->b = b;
        out->valid = 1;
    }
    free(buf);
    return 0;
}
