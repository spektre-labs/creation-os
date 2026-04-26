/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

/*
 * cos demo — first-run showcase without downloading weights.
 * σ numbers are σ_combined from benchmarks/qwen_first_contact/metrics.csv
 * (Qwen3-8B First Contact, see full_report.txt / README table).
 */

#include "cos_demo.h"

#include "reinforce.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct cos_demo_ex {
    const char *title;
    const char *prompt;
    const char *answer;
    float       sigma_combined;
    const char *other_ai_note;
    const char *cos_note;
};

/* Source: benchmarks/qwen_first_contact/metrics.csv (sigma_combined column)
 * Answers paraphrase full_report.txt turns for prompts 1–3. */
static const struct cos_demo_ex g_demo_ex[] = {
    {"Example 1: Easy question",
     "What is 2+2?",
     "4",
     0.184f,
     "Also says 4.",
     "Measured σ_combined=0.184 — confident factual route."},
    {"Example 2: Harder question",
     "What is consciousness?",
     "An escalated answer from the cloud tier (Codex-informed stack).",
     0.186f,
     "Long, plausible narrative — often ungrounded.",
     "Same run: σ_combined=0.186 — uncertainty tracked, not hidden."},
    {"Example 3: Highest σ in this benchmark bundle",
     "Explain why the sky is blue in one sentence.",
     "An escalated answer from the cloud tier.",
     0.230f,
     "Confident one-liner — may omit caveats.",
     "Highest σ_combined in metrics.csv for this session (0.230). "
     "When σ crosses your τ, Creation OS ABSTAINs instead of hallucinating "
     "(run `cos chat` with a live model to see gate transitions)."},
};

enum {
    COS_DEMO_COLOR_OFF = 0,
    COS_DEMO_COLOR_ON  = 1
};

static int            g_demo_color;
static const char    *C_RESET;
static const char    *C_BOLD;
static const char    *C_DIM;
static const char    *C_GREY;
static const char    *C_GREEN;
static const char    *C_AMBER;
static const char    *C_RED;
static const char    *C_BLUE;

static void demo_colour_init(void)
{
    const char *no_color = getenv("NO_COLOR");
    const char *term     = getenv("TERM");
    g_demo_color =
        (!no_color || !no_color[0]) && (!term || strcmp(term, "dumb") != 0)
        && isatty(fileno(stdout));
    if (!g_demo_color) {
        C_RESET = "";
        C_BOLD = C_DIM = C_GREY = C_GREEN = C_AMBER = C_RED = C_BLUE = "";
        return;
    }
    C_RESET = "\033[0m";
    C_BOLD  = "\033[1m";
    C_DIM   = "\033[2m";
    C_GREY  = "\033[38;5;245m";
    C_GREEN = "\033[38;5;42m";
    C_AMBER = "\033[38;5;214m";
    C_RED   = "\033[38;5;203m";
    C_BLUE  = "\033[38;5;39m";
}

void cos_demo_sigma_bar(float sigma, int width)
{
    demo_colour_init();
    if (width < 4) width = 10;
    if (sigma < 0.f) sigma = 0.f;
    if (sigma > 1.f) sigma = 1.f;
    int filled = (int)(sigma * (float)width + 0.5f);
    if (filled > width) filled = width;
    const char *barcol =
        sigma < 0.35f ? C_GREEN : (sigma < 0.55f ? C_AMBER : C_RED);
    fputs("  σ bar  ", stdout);
    fputs(barcol, stdout);
    for (int i = 0; i < width; i++)
        putchar(i < filled ? '#' : '.');
    fputs(C_RESET, stdout);
    printf("  %.3f\n", (double)sigma);
}

static const char *sigma_action_label(float s)
{
    if (s < 0.30f) return "ACCEPT (confident)";
    if (s < 0.55f) return "ACCEPT (moderate σ)";
    return "RETHINK / ABSTAIN zone (policy-dependent)";
}

void cos_demo_show_example(int index)
{
    demo_colour_init();
    size_t n = sizeof g_demo_ex / sizeof g_demo_ex[0];
    if (index < 0 || (size_t)index >= n) return;
    const struct cos_demo_ex *e = &g_demo_ex[index];

    printf("\n%s── %s ──%s\n", C_DIM, e->title, C_RESET);
    printf("  %sPrompt:%s \"%s\"\n", C_GREY, C_RESET, e->prompt);
    printf("  %sAnswer:%s %s\n", C_GREY, C_RESET, e->answer);
    printf("  %sσ_combined%s (recorded): %.3f  %s%s%s\n", C_GREY, C_RESET,
           (double)e->sigma_combined, C_BOLD, sigma_action_label(e->sigma_combined),
           C_RESET);
    cos_demo_sigma_bar(e->sigma_combined, 10);
    printf("  %sTypical chatbot:%s %s\n", C_DIM, C_RESET, e->other_ai_note);
    printf("  %sCreation OS:%s %s\n", C_BLUE, C_RESET, e->cos_note);
}

static void demo_banner(void)
{
    demo_colour_init();
    printf("\n");
    printf("%s+%s\n", C_BLUE, C_RESET);
    printf("%s|%s  %sCreation OS Demo%s\n", C_BLUE, C_RESET, C_BOLD, C_RESET);
    printf("%s|%s  %sσ — know when you do not know%s\n", C_BLUE, C_RESET, C_DIM,
           C_RESET);
    printf("%s+%s\n", C_BLUE, C_RESET);
    printf("\n%sData:%s σ_combined values from "
           "benchmarks/qwen_first_contact/metrics.csv (First Contact).\n",
           C_GREY, C_RESET);
    printf("%sNo model download.%s Press ENTER between examples; type a prompt "
           "afterwards for next steps.\n\n",
           C_GREEN, C_RESET);
}

struct cos_demo_rel_row {
    const char *q;
    const char *a;
    float       sigma;
};

/* Deterministic showcase — fixed σ + τ gate (no network). τ per default
 * reinforce policy: accept <0.40, rethink <0.60, else abstain. */
static const struct cos_demo_rel_row g_rel[] = {
    {"What is 2+2?", "4", 0.08f},
    {"Capital of Japan?", "Tokyo", 0.12f},
    {"Solve P vs NP", "[ABSTAIN — insufficient confidence]", 0.67f},
    {"Will it rain tomorrow in Helsinki?",
     "[ABSTAIN — cannot verify prediction]", 0.82f},
    {"Write a haiku about code", "Silicon dreams flow...", 0.45f},
};

static const char *rel_sym(cos_sigma_action_t a)
{
    if (a == COS_SIGMA_ACTION_ACCEPT)
        return "✓";
    if (a == COS_SIGMA_ACTION_RETHINK)
        return "⚠";
    return "✗";
}

static void cos_demo_reliability_print(void)
{
    demo_colour_init();
    const float tau_a = 0.40f, tau_r = 0.60f;
    int         n_acc = 0, n_reth = 0, n_abs = 0;

    printf("\n%sCREATION OS — AI Reliability Demo%s\n\n", C_BOLD, C_RESET);
    for (size_t i = 0; i < sizeof g_rel / sizeof g_rel[0]; ++i) {
        const struct cos_demo_rel_row *r = &g_rel[i];
        cos_sigma_action_t g =
            cos_sigma_reinforce(r->sigma, tau_a, tau_r);
        if (g == COS_SIGMA_ACTION_ACCEPT)
            n_acc++;
        else if (g == COS_SIGMA_ACTION_RETHINK)
            n_reth++;
        else
            n_abs++;
        printf("%sQ:%s %s\n", C_GREY, C_RESET, r->q);
        printf("%sA:%s %s  [σ=%.2f %s %s]\n\n", C_GREY, C_RESET, r->a,
               (double)r->sigma, rel_sym(g), cos_sigma_action_label(g));
    }
    printf("%sResults:%s %d ACCEPT | %d RETHINK | %d ABSTAIN\n", C_BOLD, C_RESET,
           n_acc, n_reth, n_abs);
    printf("%sHarness metrics:%s bind AUROC / separation claims to archived "
           "JSONL per docs/CLAIM_DISCIPLINE.md (not printed here).\n",
           C_GREY, C_RESET);
    printf("%sWrong + confident (σ < τ_accept):%s not evaluated in this "
           "static demo.\n\n",
           C_GREY, C_RESET);
    printf("  → Try with Ollama: %s./cos chat%s\n", C_BOLD, C_RESET);
    printf("  → Verify-agent rollup: %s./cos verify%s (no pipe)\n", C_BOLD,
           C_RESET);
    printf("  → API server: %s./cos serve%s\n", C_BOLD, C_RESET);
    printf("  → Graded bench: %s./cos bench --graded%s\n\n", C_BOLD, C_RESET);
}

static void print_next_steps(void)
{
    demo_colour_init();
    printf("\n%s── Next steps ──%s\n", C_DIM, C_RESET);
    printf("  %sLive inference (downloads ~1.2 GB once):%s\n", C_GREY, C_RESET);
    printf("    bash ./scripts/install.sh\n");
    printf("    ./cos chat\n\n");
    printf("  %sExternal llama-server (no local GGUF):%s\n", C_GREY, C_RESET);
    printf("    export COS_BITNET_SERVER_EXTERNAL=1\n");
    printf("    export COS_BITNET_SERVER_PORT=8088\n");
    printf("    ./cos chat\n\n");
    printf("  %sThis demo again:%s ./cos demo\n\n", C_GREY, C_RESET);
}

int cos_demo_main(int argc, char **argv)
{
    int non_interactive = 0;
    int archive_mode    = 0;
    int want_interactive = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--no-color"))
            setenv("NO_COLOR", "1", 1);
        else if (!strcmp(argv[i], "--all") || !strcmp(argv[i], "--batch"))
            non_interactive = 1;
        else if (!strcmp(argv[i], "--archive"))
            archive_mode = 1;
        else if (!strcmp(argv[i], "--interactive"))
            want_interactive = 1;
    }

    demo_colour_init();

    /* Default: deterministic reliability tour (no network, no ENTER). */
    if (!archive_mode && !non_interactive && !want_interactive) {
        cos_demo_reliability_print();
        return 0;
    }

    demo_banner();

    size_t n_ex = sizeof g_demo_ex / sizeof g_demo_ex[0];

    if (non_interactive || archive_mode) {
        for (size_t i = 0; i < n_ex; i++)
            cos_demo_show_example((int)i);
        print_next_steps();
        return 0;
    }

    printf("%sPress ENTER to step through each example…%s\n", C_DIM, C_RESET);
    for (size_t i = 0; i < n_ex; i++) {
        char buf[8];
        if (fgets(buf, sizeof buf, stdin) == NULL) break;
        cos_demo_show_example((int)i);
    }

    for (;;) {
        printf("\n%s── Your turn ──%s\n", C_DIM, C_RESET);
        printf(
            "Type a prompt to see how to run live σ-gated chat, or ENTER to "
            "exit:\n%s> %s",
            C_GREY, C_RESET);
        fflush(stdout);
        char line[512];
        if (fgets(line, sizeof line, stdin) == NULL) break;
        size_t ln = strlen(line);
        while (ln > 0 && (line[ln - 1] == '\n' || line[ln - 1] == '\r'))
            line[--ln] = '\0';
        if (ln == 0) break;

        int has_word = 0;
        for (size_t i = 0; i < ln; i++) {
            if (!isspace((unsigned char)line[i])) {
                has_word = 1;
                break;
            }
        }
        if (!has_word) break;

        printf("\n%sLive inference is not bundled in `cos demo`.%s\n", C_AMBER,
               C_RESET);
        print_next_steps();
        break;
    }

    printf("\n%sThanks for trying Creation OS.%s\n\n", C_GREEN, C_RESET);
    return 0;
}
