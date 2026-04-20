/*
 * σ-Persona demo — walk a short chat transcript, emit a stable
 * JSON snapshot of the learned profile.
 */
#define _GNU_SOURCE
#include "persona.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *query;
    const char *feedback;
} turn_t;

int main(void) {
    static const turn_t TURNS[] = {
        { "How does the sigma gate decide when to rethink?",      "too long"      },
        { "Explain rag retrieval and sigma filtering please",      "too technical" },
        { "Yes that's clearer, continue with persona",             "perfect"       },
        { "Mitä sigma gate mittaa käytännössä?",                   NULL            },
        { "Show me the offline preparation steps",                 "too short"     },
        { "What is the codex envelope exactly",                    "too casual"    },
    };
    static const int NT = (int)(sizeof TURNS / sizeof TURNS[0]);

    cos_persona_t p;
    if (cos_persona_init(&p, "athena_user") != COS_PERSONA_OK) {
        fprintf(stderr, "persona init failed\n");
        return 2;
    }

    for (int i = 0; i < NT; i++) {
        if (cos_persona_learn(&p, TURNS[i].query, TURNS[i].feedback) !=
            COS_PERSONA_OK) {
            fprintf(stderr, "learn failed on turn %d\n", i);
            return 3;
        }
    }

    char json[2048];
    int  n = cos_persona_to_json(&p, json, sizeof json);
    if (n < 0) { fprintf(stderr, "to_json failed\n"); return 4; }

    char env[512];
    int  en = cos_persona_to_envelope(&p, env, sizeof env);
    if (en < 0) { fprintf(stderr, "to_envelope failed\n"); return 5; }

    int self_rc = cos_persona_self_test();

    /* Wrap the canonical JSON so the demo has some metadata. */
    printf("{\n");
    printf("  \"kernel\": \"sigma_persona\",\n");
    printf("  \"self_test\": %d,\n", self_rc);
    printf("  \"turns\": %d,\n", NT);
    printf("  \"envelope\": \"");
    /* envelope is plain ASCII in the demo → safe to pass through */
    for (int i = 0; i < en; i++) {
        char c = env[i];
        if (c == '"' || c == '\\') putchar('\\');
        putchar(c);
    }
    printf("\",\n");
    printf("  \"profile\": %s\n", json);
    printf("}\n");
    return 0;
}
