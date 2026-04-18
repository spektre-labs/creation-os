/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v132 σ-Persona — CLI wrapper.
 */
#include "persona.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v132_persona --self-test\n"
        "  creation_os_v132_persona --adapt topic σ N   # observe N samples @σ on topic\n"
        "  creation_os_v132_persona --feedback FEED     # feed one of:\n"
        "                                               #   too-long,too-short,\n"
        "                                               #   too-technical,too-simple,\n"
        "                                               #   too-direct,too-formal\n"
        "  creation_os_v132_persona --demo              # run a small scenario, emit JSON\n");
    return 2;
}

static int parse_feedback(const char *s) {
    if (!strcmp(s, "too-long"))      return COS_V132_FEEDBACK_TOO_LONG;
    if (!strcmp(s, "too-short"))     return COS_V132_FEEDBACK_TOO_SHORT;
    if (!strcmp(s, "too-technical")) return COS_V132_FEEDBACK_TOO_TECHNICAL;
    if (!strcmp(s, "too-simple"))    return COS_V132_FEEDBACK_TOO_SIMPLE;
    if (!strcmp(s, "too-direct"))    return COS_V132_FEEDBACK_TOO_DIRECT;
    if (!strcmp(s, "too-formal"))    return COS_V132_FEEDBACK_TOO_FORMAL;
    return 0;
}

static int cmd_demo(void) {
    cos_v132_persona_t p;
    cos_v132_persona_init(&p);

    /* 12 confident math interactions, 8 struggling biology ones. */
    for (int i = 0; i < 12; ++i) cos_v132_observe(&p, "math",    0.10f);
    for (int i = 0; i <  8; ++i) cos_v132_observe(&p, "biology", 0.80f);

    /* Apply two feedback corrections. */
    cos_v132_apply_feedback(&p, COS_V132_FEEDBACK_TOO_LONG);
    cos_v132_apply_feedback(&p, COS_V132_FEEDBACK_TOO_TECHNICAL);

    char js[1024];
    cos_v132_to_json(&p, js, sizeof js);
    printf("%s\n", js);
    printf("---\n");
    cos_v132_write_toml(&p, stdout);
    return 0;
}

static int cmd_adapt(const char *topic, float sig, int n) {
    cos_v132_persona_t p; cos_v132_persona_init(&p);
    float ema = 0.0f;
    for (int i = 0; i < n; ++i) ema = cos_v132_observe(&p, topic, sig);
    cos_v132_topic_expertise_t *t = NULL;
    for (int i = 0; i < p.n_topics; ++i)
        if (!strcmp(p.topics[i].topic, topic)) t = &p.topics[i];
    if (!t) { fprintf(stderr, "topic not created\n"); return 1; }
    printf("{\"topic\":\"%s\",\"samples\":%d,\"ema_sigma\":%.4f,"
           "\"level\":\"%s\",\"adaptations\":%d}\n",
           t->topic, t->samples, (double)ema,
           cos_v132_level_label(t->level), p.adaptation_count);
    return 0;
}

static int cmd_feedback(const char *name) {
    int fb = parse_feedback(name);
    if (!fb) { fprintf(stderr, "unknown feedback\n"); return 2; }

    cos_v132_persona_t p; cos_v132_persona_init(&p);
    int applied = cos_v132_apply_feedback(&p, (cos_v132_feedback_t)fb);
    printf("{\"feedback\":\"%s\",\"applied\":%d,"
           "\"length\":\"%s\",\"tone\":\"%s\",\"detail\":\"%s\"}\n",
           name, applied,
           cos_v132_length_label(p.style_length),
           cos_v132_tone_label  (p.style_tone),
           cos_v132_detail_label(p.style_detail));
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) return cos_v132_self_test();
    if (!strcmp(argv[1], "--demo"))      return cmd_demo();
    if (!strcmp(argv[1], "--adapt")) {
        if (argc < 5) return usage();
        return cmd_adapt(argv[2], (float)atof(argv[3]), atoi(argv[4]));
    }
    if (!strcmp(argv[1], "--feedback")) {
        if (argc < 3) return usage();
        return cmd_feedback(argv[2]);
    }
    return usage();
}
