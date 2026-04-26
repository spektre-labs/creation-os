/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * ANSI-only TUI primitives for cos chat (--tui).
 */

#include "cos_tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COS_TUI_INNER 54

static int cos_tui_color_allowed(void)
{
    const char *no = getenv("NO_COLOR");
    if (no != NULL && no[0] != '\0')
        return 0;
    const char *term = getenv("TERM");
    if (term != NULL && strcmp(term, "dumb") == 0)
        return 0;
    return 1;
}

int cos_tui_is_tty(void)
{
    return isatty(STDOUT_FILENO);
}

void cos_tui_init(void)
{
    /* Reserved for future termios / alternate buffer; keep no-op for now. */
}

void cos_tui_clear(void)
{
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

static void clip_unit(char *dst, size_t cap, const char *src)
{
    size_t i, j;
    if (!src || !dst || cap < 2)
        return;
    for (i = 0, j = 0; src[j] != '\0' && i + 1 < cap; ++j) {
        unsigned char c = (unsigned char)src[j];
        if (c >= 32 && c != 127)
            dst[i++] = (char)c;
        else if (c == '\t')
            dst[i++] = ' ';
    }
    dst[i] = '\0';
}

void cos_tui_header(float sigma_mean, float k_eff)
{
    char left[96];
    snprintf(left, sizeof left,
             "\033[0m\033[1m┌─ Creation OS ─────────────── σ=%.2f k=%.2f ──┐\033[0m\n",
             (double)sigma_mean, (double)k_eff);
    fputs(left, stdout);
    fputs("│\033[K\n", stdout);
    fflush(stdout);
}

void cos_tui_status_bar(int turns, int cache_hits, float energy_j,
                        float sigma_mean)
{
    char ln[240];
    snprintf(ln, sizeof ln,
             "\033[7m│ mean=%.2f | turns:%d | cache:%d | %.3fJ            \033[0m\n",
             (double)sigma_mean, turns, cache_hits, (double)energy_j);
    fputs(ln, stdout);
    fputs("\033[0m└────────────────────────────────────────────────┘\n",
          stdout);
    fflush(stdout);
}

static void emit_wrapped_prefix(const char *pfx, const char *body)
{
    size_t bi = 0;
    size_t len = strlen(body);
    fprintf(stdout, "%s", pfx);
    size_t col = strlen(pfx) - 11; /* rough printed width */
    while (bi < len) {
        unsigned char c = (unsigned char)body[bi];
        if (c == '\r' || c == '\n') {
            bi++;
            continue;
        }
        if (col >= COS_TUI_INNER) {
            fputs("\n│      ", stdout);
            col = 7;
        }
        fputc((int)c, stdout);
        bi++;
        col++;
    }
    fputc('\n', stdout);
    fflush(stdout);
}

void cos_tui_print_user(const char *text)
{
    char clip[4096];
    clip_unit(clip, sizeof clip, text != NULL ? text : "");
    emit_wrapped_prefix("\033[0m│ You: ", clip);
}

static int g_ai_line_open;

void cos_tui_begin_ai_line(void)
{
    fputs("\033[0m│ AI: ", stdout);
    fflush(stdout);
    g_ai_line_open = 1;
}

void cos_tui_print_token(const char *token)
{
    if (token == NULL || token[0] == '\0')
        return;
    fputs(token, stdout);
    fflush(stdout);
}

void cos_tui_stream_newline(void)
{
    if (g_ai_line_open) {
        fputc('\n', stdout);
        fflush(stdout);
        g_ai_line_open = 0;
    }
}

void cos_tui_color_sigma(float sigma)
{
    if (!cos_tui_color_allowed()) {
        return;
    }
    if (sigma < 0.2f)
        fputs("\033[32m", stdout);
    else if (sigma <= 0.5f)
        fputs("\033[33m", stdout);
    else
        fputs("\033[31m", stdout);
}

void cos_tui_reset_color(void)
{
    fputs("\033[0m", stdout);
}

void cos_tui_print_receipt(float sigma, const char *action, float tok_s,
                           float ttft_ms, float cost_eur,
                           const char *receipt_hex16)
{
    const char *act = action != NULL ? action : "?";
    const char *tier = "";
    const char *sdr  = getenv("COS_SPEC_DECODE_ROUTE");
    if (sdr != NULL && strcmp(sdr, "DRAFT") == 0)
        tier = " DRAFT(2B)";
    else if (sdr != NULL && strcmp(sdr, "VERIFY") == 0)
        tier = " VERIFY(9B)";
    cos_tui_color_sigma(sigma);
    fprintf(stdout,
            "│ [\033[1mσ=%.3f %s%s%s%s %.1f tok/s TTFT:%.0fms €%.4f\033[0m]\033[K\n",
            (double)sigma, act, tier,
            (receipt_hex16 != NULL && receipt_hex16[0] != '\0') ? " receipt="
                                                               : "",
            (receipt_hex16 != NULL && receipt_hex16[0] != '\0') ? receipt_hex16
                                                               : "",
            (double)tok_s, (double)ttft_ms, (double)cost_eur);
    cos_tui_reset_color();
    fputs("│\033[K\n", stdout);
    fflush(stdout);
}
