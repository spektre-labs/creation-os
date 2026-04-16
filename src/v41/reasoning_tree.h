/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CREATION_OS_V41_REASONING_TREE_H
#define CREATION_OS_V41_REASONING_TREE_H

typedef struct reasoning_node {
    char *text;
    float sigma;
    struct reasoning_node *children[4];
    int n_children;
    int depth;
} reasoning_node_t;

reasoning_node_t *v41_reasoning_node_create(const char *text, float sigma, int depth);
void v41_reasoning_tree_free(reasoning_node_t *root);

typedef float (*v41_branch_sigma_fn)(void *ud, const char *parent_text, int branch_idx, int child_depth);

/** Expand up to max_depth using σ_fn to decide branching (0/2/4) and child σ. */
void v41_reasoning_expand(reasoning_node_t *node, int max_depth, v41_branch_sigma_fn sigma_fn, void *ud);

/** DFS: returns pointer to text inside tree; best_sigma_out optional. */
const char *v41_reasoning_best_leaf(const reasoning_node_t *root, float *best_sigma_out);

#endif /* CREATION_OS_V41_REASONING_TREE_H */
