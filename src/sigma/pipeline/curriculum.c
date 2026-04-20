/* σ-Curriculum primitive (level + streak state machine). */

#include "curriculum.h"

#include <string.h>

void cos_sigma_curriculum_init(cos_curriculum_state_t *s,
                               int max_level,
                               int min_rounds,
                               float sigma_threshold)
{
    if (!s) return;
    memset(s, 0, sizeof *s);
    s->level            = 1;
    s->max_level        = max_level < 1 ? 1 : max_level;
    s->min_rounds       = min_rounds < 1 ? 1 : min_rounds;
    s->sigma_threshold  = sigma_threshold;
}

cos_curriculum_event_t cos_sigma_curriculum_step(
    cos_curriculum_state_t *s, float sigma_answer)
{
    if (!s) return COS_CUR_NO_CHANGE;
    s->total_rounds++;
    if (sigma_answer < s->sigma_threshold) {
        s->total_success++;
        s->current_streak++;
        s->streak_sigma_sum += sigma_answer;
        if (s->current_streak >= s->min_rounds) {
            if (s->level < s->max_level) {
                s->level++;
                s->current_streak = 0;
                s->streak_sigma_sum = 0.0f;
                s->total_promotions++;
                s->last_promotion_at = s->total_rounds;
                return COS_CUR_PROMOTED;
            }
            /* At cap — success but no room to promote. */
            return COS_CUR_MAXED;
        }
        return COS_CUR_NO_CHANGE;
    }
    /* Failure → break the streak but do not demote. */
    if (s->current_streak > 0) {
        s->current_streak = 0;
        s->streak_sigma_sum = 0.0f;
        return COS_CUR_RESET;
    }
    return COS_CUR_NO_CHANGE;
}

const char *cos_sigma_curriculum_level_name(int level) {
    switch (level) {
        case 1: return "FACTUAL";
        case 2: return "REASONING";
        case 3: return "MULTISTEP";
        case 4: return "CREATIVE";
        case 5: return "META";
        default: return "UNKNOWN";
    }
}

float cos_sigma_curriculum_acceptance_rate(const cos_curriculum_state_t *s) {
    if (!s || s->total_rounds <= 0) return 0.0f;
    return (float)s->total_success / (float)s->total_rounds;
}

/* ---------- self-test ---------- */

static int check_init_clamps(void) {
    cos_curriculum_state_t s;
    cos_sigma_curriculum_init(&s, -3, -1, 0.30f);
    if (s.level != 1)        return 10;
    if (s.max_level != 1)    return 11;
    if (s.min_rounds != 1)   return 12;
    return 0;
}

static int check_promote_and_reset(void) {
    cos_curriculum_state_t s;
    cos_sigma_curriculum_init(&s, 5, 3, 0.30f);

    /* Three successes in a row → promotion 1 → 2. */
    if (cos_sigma_curriculum_step(&s, 0.10f) != COS_CUR_NO_CHANGE) return 20;
    if (cos_sigma_curriculum_step(&s, 0.15f) != COS_CUR_NO_CHANGE) return 21;
    if (cos_sigma_curriculum_step(&s, 0.20f) != COS_CUR_PROMOTED)  return 22;
    if (s.level != 2)                                               return 23;
    if (s.current_streak != 0)                                      return 24;
    if (s.total_promotions != 1)                                    return 25;
    if (s.last_promotion_at != 3)                                   return 26;

    /* Two successes, one fail → streak resets, level stays. */
    if (cos_sigma_curriculum_step(&s, 0.10f) != COS_CUR_NO_CHANGE) return 27;
    if (cos_sigma_curriculum_step(&s, 0.10f) != COS_CUR_NO_CHANGE) return 28;
    if (cos_sigma_curriculum_step(&s, 0.80f) != COS_CUR_RESET)     return 29;
    if (s.level != 2)                                               return 30;
    if (s.current_streak != 0)                                      return 31;
    if (s.total_success != 5)                                       return 32;

    /* Another fail with streak already 0 → NO_CHANGE (not RESET). */
    if (cos_sigma_curriculum_step(&s, 0.70f) != COS_CUR_NO_CHANGE) return 33;
    return 0;
}

static int check_max_cap(void) {
    cos_curriculum_state_t s;
    cos_sigma_curriculum_init(&s, 2, 2, 0.30f);
    /* Two successes → promote to 2. */
    cos_sigma_curriculum_step(&s, 0.10f);
    if (cos_sigma_curriculum_step(&s, 0.10f) != COS_CUR_PROMOTED)  return 40;
    /* At cap.  Two more successes must stamp MAXED, stay at 2. */
    cos_sigma_curriculum_step(&s, 0.10f);
    if (cos_sigma_curriculum_step(&s, 0.10f) != COS_CUR_MAXED)     return 41;
    if (s.level != 2)                                               return 42;
    if (s.total_promotions != 1)                                    return 43;
    return 0;
}

static int check_acceptance_rate(void) {
    cos_curriculum_state_t s;
    cos_sigma_curriculum_init(&s, 5, 3, 0.30f);
    /* Zero rounds → rate 0. */
    if (cos_sigma_curriculum_acceptance_rate(&s) != 0.0f) return 50;
    cos_sigma_curriculum_step(&s, 0.10f);  /* success */
    cos_sigma_curriculum_step(&s, 0.70f);  /* fail    */
    cos_sigma_curriculum_step(&s, 0.10f);  /* success */
    cos_sigma_curriculum_step(&s, 0.80f);  /* fail    */
    float r = cos_sigma_curriculum_acceptance_rate(&s);
    if (!(r > 0.499f && r < 0.501f))                     return 51;
    return 0;
}

static int check_level_names(void) {
    if (strcmp(cos_sigma_curriculum_level_name(1), "FACTUAL")   != 0) return 60;
    if (strcmp(cos_sigma_curriculum_level_name(5), "META")      != 0) return 61;
    if (strcmp(cos_sigma_curriculum_level_name(9), "UNKNOWN")   != 0) return 62;
    return 0;
}

int cos_sigma_curriculum_self_test(void) {
    int rc;
    if ((rc = check_init_clamps()))        return rc;
    if ((rc = check_promote_and_reset()))  return rc;
    if ((rc = check_max_cap()))            return rc;
    if ((rc = check_acceptance_rate()))    return rc;
    if ((rc = check_level_names()))        return rc;
    return 0;
}
