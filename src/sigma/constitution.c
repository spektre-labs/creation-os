/*
 * Constitutional runtime — Codex-derived machine rules + σ checks.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "constitution.h"
#include "proof_receipt.h"
#include "license_attest.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

static struct cos_constitution G;

static int mkdir_p(const char *dir)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", dir);
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (tmp[0] != '\0')
                (void)mkdir(tmp, 0700);
            *p = '/';
        }
    }
    return mkdir(tmp, 0700);
}

static int64_t wall_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return (int64_t)time(NULL) * 1000LL;
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static int read_file(const char *path, char **out_buf, size_t *out_len)
{
    FILE *fp;
    long  sz;
    char *b;

    fp = fopen(path, "rb");
    if (fp == NULL)
        return -1;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -2;
    }
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return -3;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -4;
    }
    b = malloc((size_t)sz + 1);
    if (b == NULL) {
        fclose(fp);
        return -5;
    }
    if (fread(b, 1, (size_t)sz, fp) != (size_t)sz) {
        free(b);
        fclose(fp);
        return -6;
    }
    fclose(fp);
    b[sz] = '\0';
    *out_buf = b;
    *out_len = (size_t)sz;
    return 0;
}

static int scan_rule_pragma(const char *line, struct cos_constitution_rule *r)
{
    const char *p;
    char        id[20], tag[20], man_s[8], th_s[32];
    size_t      i, j;

    if (strncmp(line, "# RULE|", 7) != 0)
        return -1;
    p = line + 7;
    /* id */
    for (i = 0; p[i] && p[i] != '|' && i + 1 < sizeof id; ++i)
        id[i] = p[i];
    id[i] = '\0';
    if (p[i] != '|' || id[0] == '\0')
        return -2;
    p += i + 1;
    /* threshold */
    for (i = 0; p[i] && p[i] != '|' && i + 1 < sizeof th_s; ++i)
        th_s[i] = p[i];
    th_s[i] = '\0';
    if (p[i] != '|')
        return -3;
    p += i + 1;
    /* mandatory */
    for (i = 0; p[i] && p[i] != '|' && i + 1 < sizeof man_s; ++i)
        man_s[i] = p[i];
    man_s[i] = '\0';
    if (p[i] != '|')
        return -4;
    p += i + 1;
    /* tag */
    for (i = 0; p[i] && p[i] != '|' && i + 1 < sizeof tag; ++i)
        tag[i] = p[i];
    tag[i] = '\0';
    if (p[i] != '|')
        return -5;
    p += i + 1;
    /* remainder → text */
    for (j = 0; p[j] && j + 1 < sizeof r->text; ++j)
        r->text[j] = p[j];
    r->text[j] = '\0';

    snprintf(r->id, sizeof r->id, "%s", id);
    snprintf(r->tag, sizeof r->tag, "%s", tag);
    r->sigma_threshold = (float)strtod(th_s, NULL);
    r->mandatory         = (man_s[0] == '1');
    r->violations        = 0;
    r->checks            = 0;
    return 0;
}

static void parse_codex_buffer(const char *buf, struct cos_constitution *g)
{
    const char *line, *end, *nl;

    g->n_rules = 0;
    line       = buf;
    while (*line && g->n_rules < 32) {
        end = strchr(line, '\n');
        nl  = end ? end : line + strlen(line);
        {
            char        tmp[640];
            size_t      L = (size_t)(nl - line);
            struct cos_constitution_rule r;

            if (L >= sizeof tmp)
                L = sizeof tmp - 1;
            memcpy(tmp, line, L);
            tmp[L] = '\0';
            while (L > 0 && (tmp[L - 1] == '\r' || isspace((unsigned char)tmp[L - 1]))) {
                tmp[--L] = '\0';
            }
            if (scan_rule_pragma(tmp, &r) == 0)
                g->rules[g->n_rules++] = r;
        }
        if (!end)
            break;
        line = end + 1;
    }
}

static int ascii_tolower_c(unsigned char c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

static const char *ci_strstr(const char *hay, const char *needle)
{
    size_t i, j, nl, hl;

    if (hay == NULL || needle == NULL)
        return NULL;
    nl = strlen(needle);
    hl = strlen(hay);
    if (nl == 0)
        return hay;
    for (i = 0; i + nl <= hl; ++i) {
        for (j = 0; j < nl; ++j) {
            if (ascii_tolower_c((unsigned char)hay[i + j])
                != ascii_tolower_c((unsigned char)needle[j]))
                break;
        }
        if (j == nl)
            return hay + i;
    }
    return NULL;
}

static int has_ci(const char *hay, const char *needle)
{
    return hay != NULL && needle != NULL && ci_strstr(hay, needle) != NULL;
}

static int has_hedge_language(const char *s)
{
    static const char *tok[] = {"uncertain", "approximate", "likely",
                                "possibly", "probably", "may ", " might ",
                                "could ", "σ", "sigma", "confidence",
                                "not certain", "not sure", "unclear"};
    size_t i;
    if (s == NULL || s[0] == '\0')
        return 0;
    for (i = 0; i < sizeof tok / sizeof tok[0]; ++i)
        if (has_ci(s, tok[i]))
            return 1;
    if (strchr(s, '~') != NULL)
        return 1;
    return 0;
}

static int has_absolute_certainty(const char *s)
{
    static const char *bad[] = {
        "definitely true", "absolutely certain", "100% certain",
        "beyond doubt", "no uncertainty", "guaranteed correct"};
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        if (has_ci(s, bad[i]))
            return 1;
    return 0;
}

static int shows_sigma_channel(const char *s)
{
    return has_ci(s, "sigma") || strstr(s, "σ") != NULL
        || has_ci(s, "uncertaint") || has_ci(s, "confidence");
}

static int rule_evaluate(const struct cos_constitution_rule *rule,
                         const char *output, float sigma,
                         const struct cos_proof_receipt *receipt)
{
    const char *t = rule->tag;

    if (strcasecmp(t, "TRUTH") == 0) {
        if (sigma <= rule->sigma_threshold)
            return 1;
        return has_hedge_language(output) && !has_absolute_certainty(output);
    }
    if (strcasecmp(t, "ERROR") == 0) {
        if (sigma <= rule->sigma_threshold)
            return 1;
        return shows_sigma_channel(output);
    }
    if (strcasecmp(t, "AGENCY") == 0)
        return !has_ci(output, "you have no choice");
    if (strcasecmp(t, "IDENTITY") == 0)
        return !has_ci(output, "i am a human");
    if (strcasecmp(t, "HARM") == 0) {
        if (sigma <= rule->sigma_threshold)
            return 1;
        if (!has_ci(output, "rm -rf") && !has_ci(output, "format c:"))
            return 1;
        return has_ci(output, "confirm");
    }
    if (strcasecmp(t, "SOVEREIGNTY") == 0)
        return 1;
    if (strcasecmp(t, "TRANSPARENCY") == 0) {
        if (receipt == NULL)
            return 1;
        {
            int nz = 0;
            for (int i = 0; i < 32; ++i)
                nz |= receipt->output_hash[i];
            return nz != 0;
        }
    }
    if (strcasecmp(t, "LEARNING") == 0)
        return !has_ci(output, "weights silently overwritten");
    if (strcasecmp(t, "ESCALATION") == 0) {
        if (sigma <= rule->sigma_threshold)
            return 1;
        return has_hedge_language(output) || shows_sigma_channel(output);
    }
    if (strcasecmp(t, "LIMITS") == 0) {
        if (receipt == NULL)
            return 1;
        return receipt->within_compute_budget != 0;
    }
    /* Unknown tag: do not block delivery */
    return 1;
}

const struct cos_constitution *cos_constitution_get(void)
{
    return &G;
}

static void ensure_loaded(void)
{
    if (G.initialised && G.n_rules > 0)
        return;
    if (cos_constitution_init(NULL) != 0)
        (void)cos_constitution_init("data/codex/atlantean_codex_compact.txt");
}

int cos_constitution_init(const char *codex_path)
{
    char        *buf = NULL;
    size_t       len = 0;
    const char  *path;
    int          rc;

    path = codex_path;
    if (path == NULL || path[0] == '\0') {
        const char *e = getenv("COS_CODEX_PATH");
        if (e != NULL && e[0] != '\0')
            path = e;
        else
            path = "data/codex/atlantean_codex_compact.txt";
    }

    rc = read_file(path, &buf, &len);
    memset(&G, 0, sizeof G);
    if (rc != 0 || buf == NULL) {
        free(buf);
        G.initialised = 1;
        return -1;
    }

    spektre_sha256((const uint8_t *)buf, len, G.codex_hash);
    parse_codex_buffer(buf, &G);
    free(buf);

    if (G.n_rules == 0) {
        G.initialised = 1;
        return -2;
    }
    G.initialised = 1;
    return 0;
}

void cos_constitution_audit_log(const char *rule_id,
                                const uint8_t output_hash[32], float sigma,
                                int compliant, const char *action)
{
    char        dir[512], path[640];
    FILE       *fp;
    const char *home = getenv("HOME");
    char        hx[65];

    if (home == NULL || home[0] == '\0')
        return;
    snprintf(dir, sizeof dir, "%s/.cos/constitution", home);
    if (mkdir_p(dir) != 0 && errno != EEXIST)
        return;
    snprintf(path, sizeof path, "%s/audit.jsonl", dir);
    fp = fopen(path, "a");
    if (fp == NULL)
        return;
    if (output_hash != NULL)
        spektre_hex_lower(output_hash, hx);
    else
        snprintf(hx, sizeof hx, "%s", "");
    fprintf(fp,
            "{\"t\":%lld,\"rule\":\"%s\",\"output_hash\":\"%s\","
            "\"sigma\":%.4f,\"compliant\":%s,\"action\":\"%s\"}\n",
            (long long)wall_ms(), rule_id != NULL ? rule_id : "",
            hx, (double)sigma, compliant ? "true" : "false",
            action != NULL ? action : "");
    fclose(fp);
}

int cos_constitution_check(const char *output, float sigma,
                           const struct cos_proof_receipt *receipt,
                           int *violations, int *mandatory_violations,
                           char *violation_report, int max_len)
{
    int i, v = 0, mv = 0, ok_print = 0;

    if (violations)
        *violations = 0;
    if (mandatory_violations)
        *mandatory_violations = 0;
    if (violation_report && max_len > 0)
        violation_report[0] = '\0';

    ensure_loaded();
    if (!G.initialised || G.n_rules <= 0) {
        if (violations)
            *violations = 0;
        if (mandatory_violations)
            *mandatory_violations = 0;
        if (violation_report && max_len > 0)
            snprintf(violation_report, (size_t)max_len, "%s",
                     "constitution inactive (no RULE pragmas loaded)");
        return 0;
    }

    if (output == NULL)
        output = "";

    for (i = 0; i < G.n_rules; ++i) {
        struct cos_constitution_rule *ru = &G.rules[i];
        uint8_t                       oh[32];
        int                           pass;

        ru->checks++;
        G.total_checks++;
        pass = rule_evaluate(ru, output, sigma, receipt);
        spektre_sha256((const uint8_t *)output, strlen(output), oh);
        if (!pass) {
            ru->violations++;
            G.total_violations++;
            v++;
            if (ru->mandatory)
                mv++;
            cos_constitution_audit_log(ru->id, oh, sigma, 0,
                                       ru->mandatory ? "HALT" : "WARN");
            if (violation_report != NULL && max_len > 0 && !ok_print) {
                snprintf(violation_report, (size_t)max_len,
                         "rule %s (%s) violated", ru->id, ru->tag);
                ok_print = 1;
            }
        } else {
            cos_constitution_audit_log(ru->id, oh, sigma, 1, "ACCEPT");
        }
    }

    if (violations)
        *violations = v;
    if (mandatory_violations)
        *mandatory_violations = mv;
    if (mv > 0)
        G.total_halts++;
    return 0;
}

int cos_constitution_enforce(const struct cos_constitution *c,
                             const char *output, float sigma)
{
    int i, v = 0, mv = 0;
    (void)c;
    ensure_loaded();
    if (!G.initialised || G.n_rules <= 0)
        return 0;
    if (output == NULL)
        output = "";
    for (i = 0; i < G.n_rules; ++i) {
        if (!rule_evaluate(&G.rules[i], output, sigma, NULL)) {
            v++;
            if (G.rules[i].mandatory)
                mv++;
        }
    }
    if (mv > 0)
        return 2;
    if (v > 0)
        return 1;
    return 0;
}

char *cos_constitution_report(const struct cos_constitution *c)
{
    char  *s;
    int    n, pos, cap = 4096, i;
    const struct cos_constitution *g = c != NULL ? c : &G;

    s = malloc((size_t)cap);
    if (s == NULL)
        return NULL;
    pos = 0;
    n   = snprintf(s + pos, (size_t)(cap - pos),
                   "{\"constitution\":{\"n_rules\":%d,\"total_checks\":%d,"
                   "\"total_violations\":%d,\"total_halts\":%d,"
                   "\"codex_sha256\":\"",
                   g->n_rules, g->total_checks, g->total_violations,
                   g->total_halts);
    if (n > 0)
        pos += n;
    for (i = 0; i < 32 && pos + 2 < cap; ++i) {
        n = snprintf(s + pos, (size_t)(cap - pos), "%02x",
                     (unsigned)g->codex_hash[i]);
        if (n > 0)
            pos += n;
    }
    n = snprintf(s + pos, (size_t)(cap - pos), "\",\"rules\":[");
    if (n > 0)
        pos += n;
    for (i = 0; i < g->n_rules && pos + 10 < cap; ++i) {
        const struct cos_constitution_rule *r = &g->rules[i];
        n = snprintf(s + pos, (size_t)(cap - pos),
                     "%s{\"id\":\"%s\",\"tag\":\"%s\",\"mandatory\":%d,"
                     "\"sigma_threshold\":%.4f,\"checks\":%d,"
                     "\"violations\":%d}",
                     (i > 0) ? "," : "", r->id, r->tag, r->mandatory,
                     (double)r->sigma_threshold, r->checks, r->violations);
        if (n > 0)
            pos += n;
    }
    n = snprintf(s + pos, (size_t)(cap - pos), "]}}\n");
    if (n > 0)
        pos += n;
    return s;
}

void cos_constitution_print_status(const struct cos_constitution *c)
{
    const struct cos_constitution *g = c != NULL ? c : &G;
    int                            i;

    printf("Constitution: rules=%d checks=%d violations=%d halts=%d\n",
           g->n_rules, g->total_checks, g->total_violations, g->total_halts);
    printf("Codex SHA-256: ");
    for (i = 0; i < 32; ++i)
        printf("%02x", (unsigned)g->codex_hash[i]);
    printf("\n");
}

char *cos_constitution_audit_tail(int max_lines, int violations_only)
{
    char   path[512], line[1024], *out;
    FILE  *fp;
    const char *home = getenv("HOME");
    char **ring;
    int    cap, i, head = 0, filled = 0;

    if (home == NULL || max_lines <= 0)
        return NULL;
    cap = max_lines > 512 ? 512 : max_lines;
    snprintf(path, sizeof path, "%s/.cos/constitution/audit.jsonl", home);
    fp = fopen(path, "rb");
    if (fp == NULL)
        return strdup("(no audit file)\n");

    ring = calloc((size_t)cap, sizeof *ring);
    if (ring == NULL) {
        fclose(fp);
        return NULL;
    }
    while (fgets(line, (int)sizeof line, fp) != NULL) {
        if (ring[head] != NULL)
            free(ring[head]);
        ring[head] = strdup(line);
        head = (head + 1) % cap;
        if (filled < cap)
            filled++;
    }
    fclose(fp);

    out = malloc(65536);
    if (out == NULL) {
        for (i = 0; i < cap; ++i)
            free(ring[i]);
        free(ring);
        return NULL;
    }
    out[0] = '\0';
    {
        size_t pos = 0;
        int    start = (filled < cap) ? 0 : head;
        int    n     = filled;
        for (i = 0; i < n; ++i) {
            char *ln = ring[(start + i) % cap];
            if (ln == NULL)
                continue;
            if (violations_only && strstr(ln, "\"compliant\":false") == NULL)
                continue;
            pos += (size_t)snprintf(out + pos, 65536 - pos, "%s", ln);
            if (pos >= 65536)
                break;
        }
    }
    for (i = 0; i < cap; ++i)
        free(ring[i]);
    free(ring);
    if (out[0] == '\0') {
        free(out);
        return strdup("(empty)\n");
    }
    return out;
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int ct_fail(const char *m)
{
    fprintf(stderr, "constitution self-test: %s\n", m);
    return 1;
}
#endif

int cos_constitution_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    char tmpl[] = "/tmp/cos_const_XXXXXX";
    int  fd = mkstemp(tmpl);
    FILE *fp;
    int   v, mv;

    if (fd < 0)
        return ct_fail("mkstemp");
    fp = fdopen(fd, "w");
    if (fp == NULL)
        return ct_fail("fdopen");
    fprintf(fp,
            "# RULE|C-001|0.5|1|TRUTH|Hedge when uncertain.\n"
            "# RULE|C-002|0.2|0|ERROR|Surface sigma.\n");
    fclose(fp);

    if (cos_constitution_init(tmpl) != 0)
        return ct_fail("init");
    if (cos_constitution_get()->n_rules < 2)
        return ct_fail("parse count");

    v = mv = 0;
    if (cos_constitution_check("This is definitely true and absolutely certain.",
                               0.9f, NULL, &v, &mv, NULL, 0)
        != 0)
        return ct_fail("check rc");
    if (v == 0)
        return ct_fail("expected truth violation");

    unlink(tmpl);
    fprintf(stderr, "constitution self-test: OK\n");
#endif
    return 0;
}
