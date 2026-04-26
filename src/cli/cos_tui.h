/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * Minimal ANSI VT100 terminal UI for cos chat (libc only — no ncurses).
 */
#ifndef COS_TUI_H
#define COS_TUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/** Optional: stdout is a TTY (respect NO_COLOR / TERM=dumb). */
int cos_tui_is_tty(void);

void cos_tui_init(void);

/** Clear screen and move cursor home (\033[2J\033[H). */
void cos_tui_clear(void);

/** Top banner with session σ mean and K_eff estimate. */
void cos_tui_header(float sigma_mean, float k_eff);

/** Reverse-video status line (turns, cache hits, energy J, σ̄). */
void cos_tui_status_bar(int turns, int cache_hits, float energy_j,
                        float sigma_mean);

void cos_tui_print_user(const char *text);

/** Stream one tokenizer chunk (no newline). */
void cos_tui_print_token(const char *token);

/** Start assistant line (call before streamed tokens). */
void cos_tui_begin_ai_line(void);

/** Finish streamed assistant line (newline). */
void cos_tui_stream_newline(void);

/** Colored receipt from scalar σ + speed/cost metrics. */
void cos_tui_print_receipt(float sigma, const char *action, float tok_s,
                           float ttft_ms, float cost_eur,
                           const char *receipt_hex16);

void cos_tui_color_sigma(float sigma);
void cos_tui_reset_color(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_TUI_H */
