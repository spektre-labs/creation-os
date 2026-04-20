/*
 * σ-corpus demo — load data/corpus/manifest.txt into a σ-RAG index,
 * run three self-reference queries about Creation OS's own theory,
 * emit a deterministic JSON report.
 *
 * This exercises the full "Creation OS understands itself" loop:
 * the repository's own markdown/TeX prose is the ground truth,
 * the σ-RAG kernel retrieves it, σ_retrieval filters it, and the
 * top hit's source basename tells us which document answered.
 */
#define _GNU_SOURCE
#include "corpus.h"
#include "rag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void esc(const char *s, char *dst, size_t cap) {
    size_t j = 0;
    if (!s) { if (cap) dst[0] = '\0'; return; }
    for (size_t i = 0; s[i] && j + 2 < cap; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = (char)c; }
        else if (c == '\n')        { dst[j++] = '\\'; dst[j++] = 'n';      }
        else if (c == '\t')        { dst[j++] = '\\'; dst[j++] = 't';      }
        else if (c < 0x20)         {
            int w = snprintf(dst + j, cap - j, "\\u%04x", c);
            if (w < 0 || (size_t)w >= cap - j) break;
            j += (size_t)w;
        }
        else dst[j++] = (char)c;
    }
    if (j < cap) dst[j] = '\0'; else if (cap) dst[cap - 1] = '\0';
}

int main(int argc, char **argv) {
    const char *manifest = (argc > 1) ? argv[1] : "data/corpus/manifest.txt";

    cos_rag_index_t idx;
    /* Larger chunks + tiny overlap → each doc produces a handful
     * of dense chunks.  τ_rag = 1.0 so the demo reports every hit
     * and the caller decides what to admit. */
    if (cos_rag_index_init(&idx, 64, 8, 1.0f) != COS_RAG_OK) {
        fprintf(stderr, "rag init failed\n");
        return 2;
    }

    cos_corpus_report_t rep;
    if (cos_corpus_ingest_manifest(&idx, manifest, &rep) != COS_CORPUS_OK) {
        fprintf(stderr, "corpus ingest failed for %s\n", manifest);
        cos_rag_index_free(&idx);
        return 3;
    }

    static const char *QUERIES[] = {
        "what is the sigma gate and how does it work",
        "assert declared equals realized invariant one equals one",
        "local retrieval augmented generation and sigma filter",
    };
    static const int QN = (int)(sizeof QUERIES / sizeof QUERIES[0]);

    char ingest_json[16384];
    cos_corpus_report_json(&rep, ingest_json, sizeof ingest_json);

    int self_rag    = cos_rag_self_test();
    int self_corpus = cos_corpus_self_test();

    printf("{\n");
    printf("  \"kernel\": \"sigma_corpus\",\n");
    printf("  \"self_test\": {\"rag\": %d, \"corpus\": %d},\n",
           self_rag, self_corpus);
    printf("  \"manifest\": \"%s\",\n", manifest);
    printf("  \"ingest\": %s,\n", ingest_json);
    printf("  \"queries\": [\n");

    for (int q = 0; q < QN; q++) {
        cos_rag_hit_t hits[3];
        int n = 0;
        if (cos_rag_search(&idx, QUERIES[q], 3, 1, hits, &n) != COS_RAG_OK) {
            fprintf(stderr, "search failed q=%d\n", q);
            cos_rag_index_free(&idx);
            return 4;
        }
        char eq[512]; esc(QUERIES[q], eq, sizeof eq);
        printf("    {\n      \"q\": \"%s\",\n      \"hits\": [\n", eq);
        for (int h = 0; h < n; h++) {
            char esrc[256]; esc(hits[h].chunk->source_file ? hits[h].chunk->source_file : "",
                                esrc, sizeof esrc);
            char snippet[65];
            size_t tlen = strlen(hits[h].chunk->text);
            size_t slen = tlen < 64 ? tlen : 64;
            memcpy(snippet, hits[h].chunk->text, slen); snippet[slen] = '\0';
            char esnip[256]; esc(snippet, esnip, sizeof esnip);
            printf("        {\"rank\": %d, \"source\": \"%s\", \"cosine\": %.4f, "
                   "\"sigma_retrieval\": %.4f, \"snippet\": \"%s\"}%s\n",
                   h + 1, esrc,
                   (double)hits[h].score,
                   (double)hits[h].sigma_retrieval,
                   esnip,
                   (h + 1 < n) ? "," : "");
        }
        printf("      ]\n    }%s\n", (q + 1 < QN) ? "," : "");
    }

    printf("  ]\n}\n");

    cos_rag_index_free(&idx);
    return 0;
}
