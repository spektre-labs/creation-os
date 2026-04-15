#pragma once
#include <stdint.h>
#include <string.h>

/* Field point — universal datum: token, pixel, decision, fact, agent. */
typedef struct {
    uint64_t id;
    uint32_t state;
    uint32_t golden;
    uint8_t  reputation;
    uint32_t shadow;
    uint8_t  sigma;
    uint8_t  tier;
    uint8_t  role;
    uint8_t  active;
} field_point_t;

typedef struct {
    uint32_t state;
    uint32_t golden;
    uint32_t assertions;
} arc_reactor_t;

typedef struct {
    field_point_t agents[7];
    uint32_t      global_sigma;
    float        *shared_memory;
    uint32_t      shared_size;
} orchestrator_t;

enum {
    ROLE_NONE    = 0,
    ROLE_PERCEIVE = 1,
    ROLE_REASON   = 2,
    ROLE_PREDICT  = 3,
    ROLE_ACT      = 4,
    ROLE_AUDIT    = 5,
    ROLE_RENDER   = 6,
    ROLE_MEMORY   = 7
};

#define BBHASH_SIZE 1024
typedef struct {
    uint64_t keys[BBHASH_SIZE];
    uint64_t values[BBHASH_SIZE];
    uint32_t count;
} bbhash_t;

typedef struct {
    uint32_t processed;
    uint32_t skipped;
    uint32_t firmware_blocked;
} render_stats_t;

/* σ = free energy gap vs equilibrium; same op every substrate. */
static inline uint32_t measure_sigma(uint32_t state, uint32_t golden) {
    return (uint32_t)__builtin_popcount(state ^ golden);
}

/* Branchless tier ladder (no if-chain for the comparison spine). */
static inline uint32_t sigma_to_tier(uint32_t sigma) {
    uint32_t t1 = (uint32_t)-(int32_t)(sigma > 0);
    uint32_t t2 = (uint32_t)-(int32_t)(sigma > 2u);
    uint32_t t3 = (uint32_t)-(int32_t)(sigma > 5u);
    uint32_t t4 = (uint32_t)-(int32_t)(sigma > 10u);
    return (t1 & 1u) + (t2 & 1u) + (t3 & 1u) + (t4 & 1u);
}

static inline void living_weights_update(field_point_t *point) {
    uint32_t bit = (point->sigma == 0) ? 1u : 0u;
    point->reputation = (uint8_t)((point->reputation << 1) | bit);
}

static inline int living_weights_score(field_point_t *point) {
    return __builtin_popcount((unsigned)point->reputation);
}

static inline void shadow_ledger_update(field_point_t *point) {
    if (point->sigma > 6)
        point->shadow |= (1u << 31);
}

static inline int is_firmware(field_point_t *point) {
    return (int)((point->shadow >> 31) & 1u);
}
