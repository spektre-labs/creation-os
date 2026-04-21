/* AGI-3 — continuous distillation bookkeeping (escalation pairs). */

#include "agi_continuous.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int resolve_pairs_path(char *out, size_t cap) {
    const char *e = getenv("CREATION_OS_DISTILL_PAIRS");
    if (e != NULL && e[0] != '\0')
        return snprintf(out, cap, "%s", e) >= (int)cap ? -1 : 0;
    const char *h = getenv("HOME");
    if (h == NULL || h[0] == '\0') {
        struct passwd *pw = getpwuid(getuid());
        h = (pw != NULL) ? pw->pw_dir : NULL;
    }
    if (h == NULL) return -1;
    return snprintf(out, cap, "%s/.cos/distill_pairs.jsonl", h) >= (int)cap ? -1 : 0;
}

static int resolve_state_path(char *out, size_t cap) {
    const char *h = getenv("HOME");
    if (h == NULL || h[0] == '\0') {
        struct passwd *pw = getpwuid(getuid());
        h = (pw != NULL) ? pw->pw_dir : NULL;
    }
    if (h == NULL) return -1;
    return snprintf(out, cap, "%s/.cos/distill_train_state.json", h) >= (int)cap ? -1 : 0;
}

int cos_agi_distill_count_pairs(const char *path, int *out_n) {
    if (!out_n || path == NULL || path[0] == '\0') return -1;
    *out_n = 0;
    FILE *fp = fopen(path, "r");
    if (fp == NULL) return 0; /* missing file → zero pairs */
    char *line = NULL;
    size_t cap = 0;
    ssize_t got;
    while ((got = getline(&line, &cap, fp)) > 0) {
        if (got > 2) (*out_n)++;
    }
    free(line);
    fclose(fp);
    return 0;
}

int cos_agi_distill_load_train_state(cos_agi_distill_train_state_t *st) {
    if (!st) return -1;
    memset(st, 0, sizeof *st);
    char path[512];
    if (resolve_state_path(path, sizeof path) != 0) return -1;
    FILE *fp = fopen(path, "r");
    if (fp == NULL) return 0;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    /* Minimal JSON field scrape (schema written by this tool). */
    const char *p = strstr(buf, "\"pairs_at_last_train\"");
    if (p) {
        p = strchr(p, ':');
        if (p) st->pairs_at_last_train = (int)strtol(p + 1, NULL, 10);
    }
    p = strstr(buf, "\"last_train_unix\"");
    if (p) {
        p = strchr(p, ':');
        if (p) st->last_train_unix = strtol(p + 1, NULL, 10);
    }
    return 0;
}

int cos_agi_distill_record_train(int pairs_used) {
    char path[512];
    if (resolve_state_path(path, sizeof path) != 0) return -1;
    char dir[512];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        if (dir[0] && mkdir(dir, 0700) != 0 && errno != EEXIST) return -1;
    }
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp,
            "{\n  \"schema\": \"cos.distill_train_state.v1\",\n"
            "  \"pairs_at_last_train\": %d,\n"
            "  \"last_train_unix\": %lld\n}\n",
            pairs_used, (long long)time(NULL));
    fclose(fp);
    return 0;
}

int cos_agi_distill_emit_status(FILE *fp) {
    if (!fp) return -1;
    char pairs_path[512];
    if (resolve_pairs_path(pairs_path, sizeof pairs_path) != 0) return -1;
    int n = 0;
    cos_agi_distill_count_pairs(pairs_path, &n);
    cos_agi_distill_train_state_t st;
    cos_agi_distill_load_train_state(&st);

    fprintf(fp, "Creation OS — continuous distillation (AGI-3)\n");
    fprintf(fp, "  pairs file:     %s\n", pairs_path);
    fprintf(fp, "  pairs collected: %d\n", n);
    if (st.last_train_unix > 0) {
        double ago = difftime(time(NULL), (time_t)st.last_train_unix) / 3600.0;
        fprintf(fp, "  last train:     %.1f h ago (%d pairs in batch)\n",
                ago, st.pairs_at_last_train);
    } else {
        fprintf(fp, "  last train:     (none recorded — run cos lora train when ready)\n");
    }
    fprintf(fp, "\n  Policy: every escalation appends one JSON line to the pairs file.\n");
    fprintf(fp, "  When ≥50 fresh pairs accumulate, run your LoRA trainer on that JSONL,\n");
    fprintf(fp, "  then call cos_agi_distill_record_train(n) from the trainer hook.\n");
    return 0;
}

int cos_agi_distill_self_test(void) {
    char tmpl[] = "/tmp/cos_agi_distill_test_XXXXXX.jsonl";
    int fd = mkstemp(tmpl);
    if (fd < 0) return -1;
    close(fd);
    FILE *w = fopen(tmpl, "w");
    if (!w) { remove(tmpl); return -1; }
    fprintf(w, "{\"x\":1}\n{\"x\":2}\n");
    fclose(w);
    int n = -1;
    if (cos_agi_distill_count_pairs(tmpl, &n) != 0 || n != 2) {
        remove(tmpl);
        return -1;
    }
    remove(tmpl);
    return 0;
}
