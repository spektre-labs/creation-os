#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_world_belief {
    char subject[128];
    char predicate[128];
    char object[128];
    float sigma;
    int64_t last_verified;
};

int cos_world_query_belief(const char *subject,
    struct cos_world_belief *beliefs, int max, int *n_found);

float cos_world_confidence(const char *subject);

int cos_world_update_belief(const char *subject, const char *predicate,
    const char *object, float sigma);

#ifdef CREATION_OS_ENABLE_SELF_TESTS
void cos_world_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
