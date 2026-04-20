/*
 * σ-RAG demo — index a tiny in-memory corpus, run three queries,
 * emit a deterministic JSON report.
 */
#define _GNU_SOURCE
#include "rag.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void json_escape(const char *s, char *dst, size_t cap) {
    size_t j = 0;
    if (!s) { if (cap) dst[0] = '\0'; return; }
    for (size_t i = 0; s[i] && j + 2 < cap; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= cap) break;
            dst[j++] = '\\'; dst[j++] = (char)c;
        } else if (c == '\n') { if (j + 2 >= cap) break; dst[j++]='\\'; dst[j++]='n'; }
        else if (c == '\t')   { if (j + 2 >= cap) break; dst[j++]='\\'; dst[j++]='t'; }
        else if (c < 0x20)    { if (j + 6 >= cap) break; j += (size_t)snprintf(dst+j, cap-j, "\\u%04x", c); }
        else dst[j++] = (char)c;
    }
    if (j < cap) dst[j] = '\0'; else if (cap) dst[cap-1] = '\0';
}

static const char *CORPUS[][2] = {
    { "docs/sigma_gate.md",
      "The sigma gate measures model confidence. High sigma means rethink. "
      "The gate is a pure function: given logits it returns a scalar in [0, 1]. "
      "Creation OS uses sigma gates to route between BitNet student and teacher." },
    { "docs/rag.md",
      "Local retrieval augmented generation runs entirely on disk. Chunks are "
      "embedded deterministically and filtered by sigma_retrieval. No cloud "
      "vector store is required. The user's corpus never leaves the machine." },
    { "docs/voice.md",
      "Voice interface uses whisper.cpp for speech-to-text and piper for "
      "speech synthesis. Both run locally. sigma_stt reflects recognizer "
      "confidence; when it is high the model asks the user to repeat." },
    { "docs/persona.md",
      "Persona profiles adapt verbosity and formality to the user. Expert "
      "users see terse answers with jargon; beginners get expanded context. "
      "Persona is part of the Codex prompt envelope." },
    { "docs/offline.md",
      "Offline mode verifies that BitNet weights, engram database, RAG index, "
      "codex file and persona are all local. No DNS, no API calls, no "
      "network fallbacks. Flight mode for sovereign reasoning." },
    { "docs/prometheus.md",
      "The Prometheus release adds continuous distillation from escalation "
      "pairs, chain-of-thought alignment, sandboxed tool execution, token "
      "streaming with in-band rethink, and a plugin architecture." },
};
static const int CORPUS_N = (int)(sizeof CORPUS / sizeof CORPUS[0]);

static const char *QUERIES[] = {
    "how does the sigma gate decide when to rethink",
    "can creation os run without a network",
    "what does voice use for speech recognition",
};
static const int QUERIES_N = (int)(sizeof QUERIES / sizeof QUERIES[0]);

int main(void) {
    cos_rag_index_t idx;
    if (cos_rag_index_init(&idx, 16, 4, 0.90f) != COS_RAG_OK) {
        fprintf(stderr, "rag init failed\n");
        return 2;
    }

    for (int i = 0; i < CORPUS_N; i++) {
        int rc = cos_rag_append_text(&idx, CORPUS[i][1], CORPUS[i][0]);
        if (rc < 0) {
            fprintf(stderr, "ingest failed at %d: rc=%d\n", i, rc);
            cos_rag_index_free(&idx);
            return 3;
        }
    }

    cos_rag_stats_t stats;
    cos_rag_stats(&idx, &stats);

    int self_rc = cos_rag_self_test();

    printf("{\n");
    printf("  \"kernel\": \"sigma_rag\",\n");
    printf("  \"version\": 1,\n");
    printf("  \"dim\": %d,\n", COS_RAG_DIM);
    printf("  \"tau_rag\": %.3f,\n", idx.tau_rag);
    printf("  \"chunk_words\": %d,\n", idx.chunk_words);
    printf("  \"chunk_overlap\": %d,\n", idx.chunk_overlap);
    printf("  \"self_test\": %d,\n", self_rc);
    printf("  \"corpus\": {\"docs\": %d, \"chunks\": %d, \"mean_bytes\": %.1f},\n",
           CORPUS_N, stats.n_chunks, stats.mean_text_bytes);
    printf("  \"queries\": [\n");

    for (int q = 0; q < QUERIES_N; q++) {
        cos_rag_hit_t hits[5];
        int n = 0;
        if (cos_rag_search(&idx, QUERIES[q], 5, 1, hits, &n) != COS_RAG_OK) {
            fprintf(stderr, "search failed on query %d\n", q);
            cos_rag_index_free(&idx);
            return 4;
        }
        char esc_q[512];
        json_escape(QUERIES[q], esc_q, sizeof esc_q);
        printf("    {\n");
        printf("      \"q\": \"%s\",\n", esc_q);
        printf("      \"hits\": [\n");
        for (int h = 0; h < n; h++) {
            char esc_src[256];
            char esc_snip[256];
            json_escape(hits[h].chunk->source_file ? hits[h].chunk->source_file : "",
                        esc_src, sizeof esc_src);
            /* 64-char snippet from the normalised chunk text */
            char snippet[65];
            size_t tlen = strlen(hits[h].chunk->text);
            size_t slen = tlen < 64 ? tlen : 64;
            memcpy(snippet, hits[h].chunk->text, slen);
            snippet[slen] = '\0';
            json_escape(snippet, esc_snip, sizeof esc_snip);
            int admitted = (hits[h].sigma_retrieval <= idx.tau_rag) ? 1 : 0;
            printf("        {\"rank\": %d, \"chunk_id\": %d, \"source\": \"%s\", "
                   "\"cosine\": %.4f, \"sigma_retrieval\": %.4f, "
                   "\"admitted\": %s, \"snippet\": \"%s\"}%s\n",
                   h + 1,
                   hits[h].chunk_id,
                   esc_src,
                   (double)hits[h].score,
                   (double)hits[h].sigma_retrieval,
                   admitted ? "true" : "false",
                   esc_snip,
                   (h + 1 < n) ? "," : "");
        }
        printf("      ]\n");
        printf("    }%s\n", (q + 1 < QUERIES_N) ? "," : "");
    }
    printf("  ],\n");
    printf("  \"invariant\": \"sigma_retrieval = clip01(1 - cosine)\"\n");
    printf("}\n");

    cos_rag_index_free(&idx);
    return 0;
}
