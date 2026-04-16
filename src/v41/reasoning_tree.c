/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "reasoning_tree.h"

#include "../sigma/decompose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void v41_scratch_logits_from_text(const char *t, float *log, int n)
{
    unsigned h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)t; p && *p; p++) {
        h = h * 16777619u ^ (unsigned)*p;
    }
    for (int i = 0; i < n; i++) {
        unsigned x = h ^ (unsigned)(i * 0x9E3779B1u);
        log[i] = (float)(x & 0xffu) * 0.01f;
    }
}

reasoning_node_t *v41_reasoning_node_create(const char *text, float sigma, int depth)
{
    reasoning_node_t *n = calloc(1, sizeof(*n));
    if (!n) {
        return NULL;
    }
    n->text = text ? strdup(text) : strdup("");
    if (!n->text) {
        free(n);
        return NULL;
    }
    n->sigma = sigma;
    n->depth = depth;
    n->n_children = 0;
    return n;
}

void v41_reasoning_tree_free(reasoning_node_t *root)
{
    if (!root) {
        return;
    }
    for (int i = 0; i < root->n_children; i++) {
        v41_reasoning_tree_free(root->children[i]);
        root->children[i] = NULL;
    }
    free(root->text);
    free(root);
}

static reasoning_node_t *create_child_text(const reasoning_node_t *parent, int branch_idx)
{
    char buf[512];
    snprintf(buf, sizeof buf, "%s|%d", parent->text, branch_idx);
    return v41_reasoning_node_create(buf, parent->sigma, parent->depth + 1);
}

void v41_reasoning_expand(reasoning_node_t *node, int max_depth, v41_branch_sigma_fn sigma_fn, void *ud)
{
    if (!node || !sigma_fn) {
        return;
    }
    if (node->depth >= max_depth) {
        return;
    }
    enum { NLOG = 32 };
    float log[NLOG];
    v41_scratch_logits_from_text(node->text, log, NLOG);
    sigma_decomposed_t s;
    sigma_decompose_dirichlet_evidence(log, NLOG, &s);
    float s_ep = s.epistemic;
    int branches = 0;
    if (s_ep > 0.6f) {
        branches = 4;
    } else if (s_ep > 0.3f) {
        branches = 2;
    } else {
        branches = 0;
    }

    for (int i = 0; i < branches; i++) {
        reasoning_node_t *ch = create_child_text(node, i);
        if (!ch) {
            continue;
        }
        ch->sigma = sigma_fn(ud, node->text, i, ch->depth);
        if (ch->sigma < node->sigma) {
            v41_reasoning_expand(ch, max_depth, sigma_fn, ud);
        }
        if (node->n_children < 4) {
            node->children[node->n_children++] = ch;
        } else {
            v41_reasoning_tree_free(ch);
        }
    }
}

const char *v41_reasoning_best_leaf(const reasoning_node_t *root, float *best_sigma_out)
{
    if (!root) {
        if (best_sigma_out) {
            *best_sigma_out = 1.0f;
        }
        return NULL;
    }
    if (root->n_children == 0) {
        if (best_sigma_out) {
            *best_sigma_out = root->sigma;
        }
        return root->text;
    }
    float best_s = 1.0f;
    const char *best_t = root->text;
    for (int i = 0; i < root->n_children; i++) {
        float cs;
        const char *t = v41_reasoning_best_leaf(root->children[i], &cs);
        if (t && cs < best_s) {
            best_s = cs;
            best_t = t;
        }
    }
    if (best_sigma_out) {
        *best_sigma_out = best_s;
    }
    return best_t;
}
