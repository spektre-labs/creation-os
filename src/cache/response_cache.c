/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "cache/response_cache.h"

#include "crypto/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static int cache_dir(char *dir, size_t cap)
{
    const char *h = getenv("HOME");
    if (!dir || cap < 32)
        return -1;
    if (h == NULL || h[0] == '\0')
        h = "/tmp";
    if (snprintf(dir, cap, "%s/.cos/cache", h) >= (int)cap)
        return -1;
    return 0;
}

static void cache_key_hex(const char *prompt, const char *model, char out65[65])
{
    char        block[8192];
    const char *m = (model != NULL && model[0] != '\0') ? model : "default";
    size_t      pl  = prompt ? strlen(prompt) : 0u;
    size_t      ml  = strlen(m);
    if (pl + ml + 2u >= sizeof block)
        pl = sizeof block - ml - 3u;
    memcpy(block, prompt ? prompt : "", pl);
    block[pl] = '\n';
    memcpy(block + pl + 1u, m, ml + 1u);
    cos_sha256_hex((const uint8_t *)block, pl + 1u + ml, out65);
}

static int ensure_dir(const char *dir)
{
    struct stat st;
    if (stat(dir, &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return 0;
        return -1;
    }
    return mkdir(dir, 0700);
}

int cos_cache_lookup(const char *prompt, const char *model, cos_cache_entry_t *out)
{
    char dir[512], path[640], key[65];
    if (out == NULL || prompt == NULL)
        return -1;
    if (cache_dir(dir, sizeof dir) != 0)
        return -1;
    if (ensure_dir(dir) != 0)
        return -1;
    cache_key_hex(prompt, model, key);
    if (snprintf(path, sizeof path, "%s/%s.txt", dir, key) >= (int)sizeof path)
        return -1;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->prompt_hash, sizeof out->prompt_hash, "%s", key);
    char line[512];
    int  phase = 0;
    if (fgets(line, sizeof line, fp) == NULL || strcmp(line, "cos_cache_v1\n") != 0) {
        fclose(fp);
        return -1;
    }
    while (fgets(line, sizeof line, fp) != NULL) {
        if (strcmp(line, "----\n") == 0) {
            phase = 1;
            continue;
        }
        if (phase == 0) {
            if (strncmp(line, "sigma ", 6) == 0)
                out->sigma = (float)strtod(line + 6, NULL);
            else if (strncmp(line, "action ", 7) == 0) {
                size_t n = strlen(line);
                if (n > 0 && line[n - 1] == '\n')
                    line[--n] = '\0';
                snprintf(out->action, sizeof out->action, "%s", line + 7);
            } else if (strncmp(line, "ts ", 3) == 0)
                out->timestamp = (uint64_t)strtoull(line + 3, NULL, 10);
        } else {
            size_t w = strlen(out->response);
            size_t L = strlen(line);
            if (L > 0 && line[L - 1] == '\n')
                line[--L] = '\0';
            if (w + L + 2u < sizeof out->response) {
                if (w > 0) {
                    out->response[w++] = '\n';
                    out->response[w]   = '\0';
                }
                strncat(out->response, line, sizeof out->response - w - 1u);
            }
        }
    }
    fclose(fp);
    if (out->response[0] == '\0')
        return -1;
    return 0;
}

void cos_cache_store(const char *prompt, const char *model, const char *response,
                     float sigma, const char *action)
{
    char dir[512], path[640], key[65];
    if (prompt == NULL || response == NULL)
        return;
    if (cache_dir(dir, sizeof dir) != 0)
        return;
    if (ensure_dir(dir) != 0)
        return;
    cache_key_hex(prompt, model, key);
    if (snprintf(path, sizeof path, "%s/%s.txt", dir, key) >= (int)sizeof path)
        return;
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
        return;
    fprintf(fp, "cos_cache_v1\nsigma %.6f\naction %s\nts %llu\n----\n",
            (double)sigma, action != NULL ? action : "ACCEPT",
            (unsigned long long)(uint64_t)time(NULL));
    fputs(response, fp);
    fputc('\n', fp);
    fclose(fp);
}
