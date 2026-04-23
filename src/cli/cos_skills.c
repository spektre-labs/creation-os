/*
 * cos skills — list / maintain σ-measured distilled skills.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "skill_distill.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void print_one(const struct cos_skill *s, int detail)
{
    printf("  %016llx  %s  σ=%.3f  r=%.2f  u=%d  ok=%d  last=%lldms\n",
           (unsigned long long)s->skill_hash, s->name, (double)s->sigma_mean,
           (double)s->reliability, s->times_used, s->times_succeeded,
           (long long)s->last_used_ms);
    if (detail) {
        printf("    trigger: %s\n", s->trigger_pattern);
        for (int i = 0; i < s->n_steps; i++)
            printf("    step %d: %s\n", i + 1, s->steps[i]);
    }
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_skill_self_test() != 0 ? 1 : 0;

    int   do_retire = 0;
    int   do_detail = 0;
    char *detail_id = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--retire") == 0) {
            do_retire = 1;
        } else if (strcmp(argv[i], "--detail") == 0 && i + 1 < argc) {
            do_detail  = 1;
            detail_id  = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            fprintf(
                stderr,
                "cos skills — σ skill bank\n"
                "  (default)   list all active skills\n"
                "  --retire    archive low-reliability + run maintenance\n"
                "  --detail H  show one skill (hash in hex, 16 digits)\n"
                "  --self-test\n"
                "  COS_SKILLS_DB  override database path\n");
            return 0;
        }
    }

    if (do_detail && detail_id) {
        uint64_t h = (uint64_t)strtoull(detail_id, NULL, 16);
        struct cos_skill s;
        if (cos_skill_fetch(h, &s) != 0) {
            fprintf(stderr, "cos skills: not found: %s\n", detail_id);
            return 1;
        }
        print_one(&s, 1);
        return 0;
    }

    if (do_retire) {
        (void)cos_skill_maintain(0.40f, (int64_t)time(NULL) * 1000);
        (void)cos_skill_retire_low_reliability();
        printf("Retire + maintenance pass done.\n");
    }

    struct cos_skill buf[256];
    int            n = 0;
    if (cos_skill_list(buf, 256, &n) != 0) {
        fprintf(stderr, "cos skills: list failed (open ~/.cos/skills.db?)\n");
        return 1;
    }
    printf("active skills: %d\n", n);
    for (int i = 0; i < n; i++)
        print_one(&buf[i], 0);
    return 0;
}
