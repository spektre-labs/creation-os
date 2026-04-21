/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "bitnet_ppl.h"

#include "bitnet_spawn.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
float cos_bitnet_sigma_from_perplexity(const char *prompt, const char *generated_text)
{
    (void)prompt;
    (void)generated_text;
    return -1.0f;
}
#else
#include <unistd.h>

/* llama-perplexity requires at least 2*n_ctx BPE tokens in the input file.
 * Append this English filler so short chat answers still qualify when
 * using a modest ctx (see CREATION_OS_BITNET_PPL_CTX, default 128).
 * The pad is repeated N times below to clear the 256-token floor. */
static const char k_ppl_pad[] =
    "\n\nThe quick brown fox jumps over the lazy dog. "
    "Pack my box with five dozen liquor jugs. "
    "How vexingly quick daft zebras jump. "
    "Sphinx of black quartz, judge my vow. "
    "The five boxing wizards jump quickly. "
    "Jackdaws love my big sphinx of quartz. "
    "Cwm fjord bank glyphs vext quiz. "
    "Bright vixens jump; dozy fowl quack. "
    "Jaded zombies acted quaintly but kept driving their oxen ahead. "
    "We promptly judged antique ivory buckles for the next prize. "
    "A mad boxer shot a quick, gloved jab to the jaw of his dizzy opponent. "
    "The job of waxing linoleum frequently peeves children. "
    "Just keep examining every low bid quoted for zinc etchings. "
    "How razorback jumping frogs can level six piqued gymnasts. "
    "Crazy Fredericka bought many very exquisite opal jewels. "
    "Sixty zippers were quickly picked from the woven jute bag. "
    "Perhaps the quick brown fox was not tired after all. "
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
    "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
    "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
    "mollit anim id est laborum. "
    "Sed ut perspiciatis unde omnis iste natus error sit voluptatem "
    "accusantium doloremque laudantium, totam rem aperiam, eaque ipsa quae "
    "ab illo inventore veritatis et quasi architecto beatae vitae dicta sunt "
    "explicabo. Nemo enim ipsam voluptatem quia voluptas sit aspernatur aut "
    "odit aut fugit, sed quia consequuntur magni dolores eos qui ratione "
    "voluptatem sequi nesciunt. Neque porro quisquam est, qui dolorem ipsum "
    "quia dolor sit amet, consectetur, adipisci velit, sed quia non numquam "
    "eius modi tempora incidunt ut labore et dolore magnam aliquam quaerat "
    "voluptatem. Ut enim ad minima veniam, quis nostrum exercitationem ullam "
    "corporis suscipit laboriosam, nisi ut aliquid ex ea commodi consequatur. "
    "Quis autem vel eum iure reprehenderit qui in ea voluptate velit esse "
    "quam nihil molestiae consequatur, vel illum qui dolorem eum fugiat quo "
    "voluptas nulla pariatur.\n";

static int write_ppl_input(const char *prompt, const char *gen, char *path, size_t path_cap)
{
    (void)path_cap;
    char tmpl[] = "/tmp/cos_bitnet_pplXXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0)
        return -1;
    FILE *fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        unlink(tmpl);
        return -1;
    }

    size_t unit_bytes = 1u;
    if (prompt != NULL) unit_bytes += strlen(prompt);
    if (gen != NULL)    unit_bytes += strlen(gen);
    unit_bytes += 2u; /* newlines */

    /* llama-perplexity needs ≥2·n_ctx BPE tokens (~≥1024 chars at ctx=128).
     * Repeat the prompt+gen block so *only* their joint distribution drives
     * the perplexity; a generic English pad is appended as a last resort if
     * the repeated block is still too short. */
    int reps = (int)(2048u / (unit_bytes ? unit_bytes : 1u)) + 1;
    if (reps < 2) reps = 2;
    if (reps > 64) reps = 64;

    for (int i = 0; i < reps; i++) {
        if (prompt != NULL && prompt[0] != '\0') {
            fputs(prompt, fp);
            fputc('\n', fp);
        }
        if (gen != NULL && gen[0] != '\0') {
            fputs(gen, fp);
            fputc('\n', fp);
        }
    }
    /* Safety net for degenerate inputs. */
    if (reps * (int)unit_bytes < 1024) {
        fputs(k_ppl_pad, fp);
    }

    if (fclose(fp) != 0) {
        unlink(tmpl);
        return -1;
    }
    (void)snprintf(path, path_cap, "%s", tmpl);
    return 0;
}

static int derive_perplex_exe(const char *llama_cli, char *out, size_t cap)
{
    if (!llama_cli || !llama_cli[0])
        return -1;
    (void)snprintf(out, cap, "%s", llama_cli);
    char *slash = strrchr(out, '/');
    if (slash == NULL)
        return -1;
    (void)snprintf(slash + 1, cap - (size_t)(slash + 1 - out), "llama-perplexity");
    return 0;
}

static float parse_ppl_sigma(const char *merged_out)
{
    const char *needle = "Final estimate: PPL = ";
    const char *p = strstr(merged_out, needle);
    if (p == NULL)
        return -1.0f;
    p += strlen(needle);
    char *end = NULL;
    double ppl = strtod(p, &end);
    if (end == p || ppl < 1.0)
        return -1.0f;
    /* Monotone map: perplexity ~1 (very sharp) -> low σ; ~80+ -> high σ. */
    float lp = (float)log((double)ppl);
    float lo = (float)log(1.15);
    float hi = (float)log(96.0);
    float t = (lp - lo) / (hi - lo);
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;
    float sigma = 0.04f + 0.90f * t;
    if (sigma > 0.95f)
        sigma = 0.95f;
    if (sigma < 0.04f)
        sigma = 0.04f;
    return sigma;
}

float cos_bitnet_sigma_from_perplexity(const char *prompt, const char *generated_text)
{
    const char *use = getenv("CREATION_OS_BITNET_USE_PPL");
    const int debug = (getenv("CREATION_OS_BITNET_PPL_DEBUG") != NULL);
    if (use == NULL || use[0] != '1') {
        if (debug) (void)fprintf(stderr, "[cos-bitnet-ppl] skip: USE_PPL not set\n");
        return -1.0f;
    }

    const char *model = getenv("CREATION_OS_BITNET_MODEL");
    if (model == NULL || model[0] == '\0') {
        if (debug) (void)fprintf(stderr, "[cos-bitnet-ppl] skip: no model\n");
        return -1.0f;
    }

    const char *cli = getenv("CREATION_OS_BITNET_EXE");
    if (cli == NULL || cli[0] == '\0') {
        if (debug) (void)fprintf(stderr, "[cos-bitnet-ppl] skip: no exe\n");
        return -1.0f;
    }

    char perp_path[512];
    const char *pex = getenv("CREATION_OS_BITNET_PERPLEX_EXE");
    if (pex != NULL && pex[0] != '\0') {
        (void)snprintf(perp_path, sizeof perp_path, "%s", pex);
    } else if (derive_perplex_exe(cli, perp_path, sizeof perp_path) != 0) {
        if (debug) (void)fprintf(stderr, "[cos-bitnet-ppl] skip: derive failed\n");
        return -1.0f;
    }
    if (debug) (void)fprintf(stderr, "[cos-bitnet-ppl] using %s\n", perp_path);

    char tmp[512];
    if (write_ppl_input(prompt, generated_text, tmp, sizeof tmp) != 0) {
        if (debug) (void)fprintf(stderr, "[cos-bitnet-ppl] skip: write input failed\n");
        return -1.0f;
    }

    char ctx_buf[32];
    {
        const char *ec = getenv("CREATION_OS_BITNET_PPL_CTX");
        if (ec != NULL && ec[0] != '\0')
            (void)snprintf(ctx_buf, sizeof ctx_buf, "%s", ec);
        else
            (void)snprintf(ctx_buf, sizeof ctx_buf, "128");
    }

    char *argv[24];
    int ac = 0;
    argv[ac++] = perp_path;
    argv[ac++] = "--no-perf";
    argv[ac++] = "-m";
    argv[ac++] = (char *)model;
    argv[ac++] = "-f";
    argv[ac++] = tmp;
    argv[ac++] = "-c";
    argv[ac++] = ctx_buf;
    /* -ngl 0 crashes bitnet llama-perplexity; let the backend pick metal/CPU. */
    argv[ac] = NULL;

    char merged[131072];
    int rc = cos_bitnet_spawn_argv_capture(argv, merged, sizeof merged, 1);
    if (debug)
        (void)fprintf(stderr, "[cos-bitnet-ppl] spawn rc=%d len=%zu\n",
                      rc, strlen(merged));
    if (rc != 0) {
        (void)unlink(tmp);
        return -1.0f;
    }
    (void)unlink(tmp);
    float s = parse_ppl_sigma(merged);
    if (debug) (void)fprintf(stderr, "[cos-bitnet-ppl] sigma=%.4f\n", (double)s);
    return s;
}
#endif
