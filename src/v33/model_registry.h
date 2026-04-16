/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v33: configurable model table (BitNet default + optional API/local fallback).
 */
#ifndef CREATION_OS_V33_MODEL_REGISTRY_H
#define CREATION_OS_V33_MODEL_REGISTRY_H

#include <stddef.h>

#define COS_MODEL_MAX_ENTRIES 8

typedef struct {
    char name[64];
    char binary[512];
    char model_path[512];
    int is_local;
    char api_base[512];
    float cost_per_token;
} cos_model_entry_t;

typedef struct {
    cos_model_entry_t entries[COS_MODEL_MAX_ENTRIES];
    size_t n;
} cos_model_registry_t;

void cos_model_registry_init_defaults(cos_model_registry_t *reg);
/** Optional JSON list at path; merges / overrides by name. Returns 0 or -1. */
int cos_model_registry_load(const char *path, cos_model_registry_t *reg);

const cos_model_entry_t *cos_model_find(const cos_model_registry_t *reg, const char *name);
int cos_model_has_named_fallback(const cos_model_registry_t *reg, const char *fallback_name);

#endif /* CREATION_OS_V33_MODEL_REGISTRY_H */
