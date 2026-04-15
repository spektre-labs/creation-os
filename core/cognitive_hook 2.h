#pragma once

#include "alm.h"

void alm_cognitive_reset_session(void);
void cognitive_after_query(const char *question_raw, alm_response_t *r);
void alm_meta_print_report(void);
