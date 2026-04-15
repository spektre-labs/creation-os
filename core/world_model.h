#pragma once

#include <stddef.h>

#define WM_KEY_MAX 256
#define WM_MAX_TRANS 2048

typedef struct {
    char  next_state[WM_KEY_MAX];
    float sigma;
    float confidence;
} wm_prediction_t;

void world_model_reset(void);
/* Tallenna siirtymä; sigma on reunalla (0 = täysi luottamus eksaktissa polussa). */
void world_model_learn(const char *state, const char *action, const char *next_state, float sigma);

/* Eksaktia haku → sitten HDC-lähin tila (sama toiminto) → UNKNOWN. */
void world_model_predict(const char *state, const char *action, wm_prediction_t *out);

#define WM_GEN_HIT_DOT 0.86f

int world_model_run_eval_json(void);
