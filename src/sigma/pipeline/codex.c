/* Atlantean Codex loader — see codex.h for contract. */

#include "codex.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------- FNV-1a-64 -------- */

uint64_t cos_sigma_codex_hash_bytes(const char *buf, size_t n) {
    /* Standard FNV-1a-64 offset basis / prime. */
    uint64_t h = 0xcbf29ce484222325ULL;
    if (buf == NULL) return h;
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint64_t)(unsigned char)buf[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

/* -------- chapter counter -------- */

static int count_chapters(const char *s, size_t n) {
    /* Count line-starts that match "CHAPTER ".  A chapter header
     * sits on its own line in the canonical Codex, so we look for
     * "\nCHAPTER " and also a leading "CHAPTER " at buf[0]. */
    if (s == NULL || n < 8) return 0;
    int cnt = 0;
    if (n >= 8 && memcmp(s, "CHAPTER ", 8) == 0) cnt++;
    for (size_t i = 1; i + 8 <= n; ++i) {
        if (s[i - 1] == '\n' && memcmp(s + i, "CHAPTER ", 8) == 0) {
            cnt++;
        }
    }
    return cnt;
}

/* -------- file I/O -------- */

static int read_file(const char *path, char **out_buf, size_t *out_n) {
    if (path == NULL || out_buf == NULL || out_n == NULL) return -1;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return -2;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -3; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -4; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -5; }
    size_t n = (size_t)sz;
    char *buf = (char *)malloc(n + 1);
    if (buf == NULL) { fclose(f); return -6; }
    size_t rd = fread(buf, 1, n, f);
    fclose(f);
    if (rd != n) { free(buf); return -7; }
    buf[n] = '\0';
    *out_buf = buf;
    *out_n = n;
    return 0;
}

/* -------- public API -------- */

int cos_sigma_codex_load(const char *path, cos_sigma_codex_t *out) {
    if (out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    const char *p = path;
    if (p == NULL) {
        const char *env = getenv("COS_CODEX_PATH");
        p = (env != NULL && env[0] != '\0') ? env : COS_CODEX_PATH_FULL;
    }
    char *buf = NULL;
    size_t n = 0;
    int rc = read_file(p, &buf, &n);
    if (rc != 0) return rc;
    out->bytes          = buf;
    out->size           = n;
    out->hash_fnv1a64   = cos_sigma_codex_hash_bytes(buf, n);
    out->chapters_found = count_chapters(buf, n);
    out->is_seed        = (p != NULL && (strstr(p, "codex_seed") != NULL
                                          || strstr(p, "compact") != NULL))
                              ? 1
                              : 0;
    return 0;
}

int cos_sigma_codex_load_seed(cos_sigma_codex_t *out) {
    int rc = cos_sigma_codex_load(COS_CODEX_PATH_SEED, out);
    if (rc == 0 && out != NULL) out->is_seed = 1;
    return rc;
}

void cos_sigma_codex_free(cos_sigma_codex_t *c) {
    if (c == NULL) return;
    if (c->bytes != NULL) free(c->bytes);
    memset(c, 0, sizeof(*c));
}

bool cos_sigma_codex_contains_invariant(const cos_sigma_codex_t *c) {
    if (c == NULL || c->bytes == NULL || c->size < 5) return false;
    return strstr(c->bytes, "1 = 1") != NULL;
}

/* -------- self-test -------- */

static int check_hash_stable(void) {
    const char *s = "1 = 1";
    uint64_t a = cos_sigma_codex_hash_bytes(s, 5);
    uint64_t b = cos_sigma_codex_hash_bytes(s, 5);
    if (a != b) { fprintf(stderr, "codex: hash unstable\n"); return 1; }
    if (a == 0) { fprintf(stderr, "codex: hash zero\n");     return 1; }
    /* NULL buffer → offset basis, never zero. */
    if (cos_sigma_codex_hash_bytes(NULL, 0) == 0) {
        fprintf(stderr, "codex: null hash zero\n"); return 1;
    }
    return 0;
}

static int check_null_guards(void) {
    if (cos_sigma_codex_load("does-not-exist.txt", NULL) >= 0) {
        fprintf(stderr, "codex: NULL out accepted\n"); return 1;
    }
    cos_sigma_codex_t c;
    memset(&c, 0, sizeof(c));
    if (cos_sigma_codex_load("does-not-exist-xyz-777.txt", &c) >= 0) {
        fprintf(stderr, "codex: missing file accepted\n"); return 1;
    }
    if (c.bytes != NULL) {
        fprintf(stderr, "codex: bytes set on failure\n"); return 1;
    }
    /* free on zeroed struct must be safe. */
    cos_sigma_codex_free(&c);
    cos_sigma_codex_free(NULL);
    return 0;
}

static int check_chapter_count(void) {
    const char *text =
        "CHAPTER I\nfoo\n"
        "CHAPTER II\nbar\n"
        "prose CHAPTER not-a-header\n"
        "CHAPTER III\nbaz\n";
    int n = count_chapters(text, strlen(text));
    if (n != 3) {
        fprintf(stderr, "codex: chapter count = %d (want 3)\n", n);
        return 1;
    }
    if (count_chapters(NULL, 0) != 0) return 1;
    if (count_chapters("short", 5) != 0) return 1;
    return 0;
}

static int check_full_load(void) {
    cos_sigma_codex_t c;
    int rc = cos_sigma_codex_load(NULL, &c);
    if (rc != 0) {
        /* Only a hard failure if the canonical file is actually
         * missing.  During in-tree `make check-codex` the file is
         * present; outside the repo this branch is expected to
         * bail out cleanly. */
        fprintf(stderr, "codex: full load failed rc=%d (is cwd the "
                        "repo root with data/codex/ present?)\n", rc);
        return 1;
    }
    int bad = 0;
    if (c.size < 8000) {
        fprintf(stderr, "codex: full size %zu < 8000\n", c.size); bad = 1;
    }
    if (c.chapters_found < COS_CODEX_MIN_CHAPTERS_FULL) {
        fprintf(stderr, "codex: chapters %d < %d\n",
                c.chapters_found, COS_CODEX_MIN_CHAPTERS_FULL);
        bad = 1;
    }
    if (!cos_sigma_codex_contains_invariant(&c)) {
        fprintf(stderr, "codex: invariant '1 = 1' missing\n"); bad = 1;
    }
    if (c.hash_fnv1a64 == 0) {
        fprintf(stderr, "codex: hash is zero\n"); bad = 1;
    }

    /* Double-load parity: same hash + same byte count. */
    cos_sigma_codex_t d;
    if (cos_sigma_codex_load(NULL, &d) != 0) {
        fprintf(stderr, "codex: second load failed\n");
        cos_sigma_codex_free(&c);
        return 1;
    }
    if (d.size != c.size || d.hash_fnv1a64 != c.hash_fnv1a64) {
        fprintf(stderr, "codex: double-load drift (%zu/%zu)\n",
                c.size, d.size);
        bad = 1;
    }
    cos_sigma_codex_free(&c);
    cos_sigma_codex_free(&d);
    return bad;
}

static int check_seed_load(void) {
    cos_sigma_codex_t c;
    int rc = cos_sigma_codex_load_seed(&c);
    if (rc != 0) {
        fprintf(stderr, "codex: seed load failed rc=%d\n", rc);
        return 1;
    }
    int bad = 0;
    if (c.size < 500 || c.size > 4000) {
        fprintf(stderr, "codex: seed size %zu out of [500,4000]\n",
                c.size);
        bad = 1;
    }
    if (!c.is_seed) {
        fprintf(stderr, "codex: is_seed not set\n"); bad = 1;
    }
    if (!cos_sigma_codex_contains_invariant(&c)) {
        fprintf(stderr, "codex: seed missing invariant\n"); bad = 1;
    }
    cos_sigma_codex_free(&c);
    return bad;
}

int cos_sigma_codex_self_test(void) {
    if (check_hash_stable())    return 1;
    if (check_null_guards())    return 2;
    if (check_chapter_count())  return 3;
    if (check_full_load())      return 4;
    if (check_seed_load())      return 5;
    return 0;
}
