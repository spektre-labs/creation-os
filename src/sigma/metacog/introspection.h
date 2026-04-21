/* ULTRA-5 — four-channel meta-σ; max drives diagnostic policy. */

#ifndef COS_ULTRA_METACOG_H
#define COS_ULTRA_METACOG_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sigma_perception;
    float sigma_self;
    float sigma_social;
    float sigma_situational;
} cos_ultra_metacog_levels_t;

float cos_ultra_metacog_max_sigma(const cos_ultra_metacog_levels_t *lv);

/* Writes a short English recommendation into buf. */
void cos_ultra_metacog_recommend(const cos_ultra_metacog_levels_t *lv,
                                 char *buf, size_t cap);

void cos_ultra_metacog_emit_report(FILE *fp,
                                   const cos_ultra_metacog_levels_t *lv);

int cos_ultra_metacog_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
