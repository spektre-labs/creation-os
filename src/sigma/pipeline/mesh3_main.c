/*
 * σ-mesh3 demo — fixed three-query batch + a tamper probe.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "mesh3.h"

#include <stdio.h>
#include <string.h>

static const char *kind_str(int k) {
    switch (k) {
        case COS_MESH3_QUERY:        return "QUERY";
        case COS_MESH3_ANSWER:       return "ANSWER";
        case COS_MESH3_ESCALATE:     return "ESCALATE";
        case COS_MESH3_ANSWER_FINAL: return "ANSWER_FINAL";
        case COS_MESH3_FEDERATION:   return "FEDERATION";
        default:                     return "?";
    }
}

static void esc(const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n",  stdout); break;
            default:   putchar(*s);
        }
    }
}

int main(void) {
    int st = cos_mesh3_self_test();

    cos_mesh3_mesh_t m;
    cos_mesh3_init(&m);
    const char *queries[] = {
        "Who wrote Tähtien sota?",
        "Mikä on σ?",
        "Selitä Finnish coffee culture.",
    };
    float final_sigmas[3] = {0};
    for (int i = 0; i < 3; i++) {
        cos_mesh3_run_query(&m, queries[i], &final_sigmas[i]);
    }
    int tamper = cos_mesh3_tamper_probe(&m);

    printf("{\n");
    printf("  \"kernel\": \"sigma_mesh3\",\n");
    printf("  \"self_test\": %d,\n", st);
    printf("  \"tau_escalate\": %.4f,\n", (double)m.tau_escalate);
    printf("  \"dp_noise_sigma\": %.4f,\n", (double)m.dp_noise_sigma);
    printf("  \"nodes\": [\n");
    for (int i = 0; i < COS_MESH3_NODES; i++) {
        const cos_mesh3_node_t *n = &m.nodes[i];
        printf("    { \"index\": %d, \"name\": \"%s\", \"role\": \"%s\","
               " \"queries_seen\": %d, \"answers_given\": %d,"
               " \"escalated_out\": %d, \"federation_updates_applied\": %d,"
               " \"local_sigma_mean\": %.4f }%s\n",
               i, n->name, n->role,
               n->queries_seen, n->answers_given,
               n->escalated_out, n->federation_updates_applied,
               (double)n->local_sigma_mean,
               i == COS_MESH3_NODES - 1 ? "" : ",");
    }
    printf("  ],\n");
    printf("  \"messages\": [\n");
    for (int i = 0; i < m.n_messages; i++) {
        const cos_mesh3_msg_t *msg = &m.log[i];
        /* Ed25519 sig is 64 bytes; show the first 8 as a stable
         * fingerprint in the JSON trace.  Full sig remains
         * in-memory for verify(). */
        unsigned long long sig_hi = 0;
        for (int b = 0; b < 8; ++b)
            sig_hi = (sig_hi << 8) | msg->signature[b];
        printf("    { \"i\": %d, \"from\": %d, \"to\": %d, \"kind\": \"%s\","
               " \"sigma\": %.4f, \"verified\": %s, \"dropped\": %s,"
               " \"sig_hi64\": \"0x%016llx\", \"payload\": \"",
               i, msg->from, msg->to, kind_str(msg->kind),
               (double)msg->sigma,
               msg->verified ? "true" : "false",
               msg->dropped ? "true" : "false",
               sig_hi);
        esc(msg->payload);
        printf("\" }%s\n", i + 1 == m.n_messages ? "" : ",");
    }
    printf("  ],\n");
    printf("  \"summary\": { \"messages\": %d, \"escalations\": %d,"
           " \"federations\": %d, \"signatures_rejected\": %d,"
           " \"tamper_probe\": %d },\n",
           m.n_messages, m.escalations, m.federations,
           m.signatures_rejected, tamper);
    printf("  \"final_sigmas\": [%.4f, %.4f, %.4f]\n",
           (double)final_sigmas[0], (double)final_sigmas[1],
           (double)final_sigmas[2]);
    printf("}\n");
    return st == 0 ? 0 : 1;
}
