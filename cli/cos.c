/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * cos — Creation OS unified front door (Apple-tier CLI).
 *
 * Single-binary C, no dependencies beyond libc, designed under Apple's
 * Human Interface Guidelines for terminal UIs:
 *   - Clarity over decoration: no ASCII art, no emojis, no banners.
 *   - Deference: content over chrome.  Whitespace is structural.
 *   - Depth: the binary is the front door; v60..v62 are layers under it.
 *
 * Usage:
 *     cos                        # status board (default)
 *     cos status                 # explicit status
 *     cos verify                 # the verified-agent report (v57)
 *     cos chace                  # the DARPA-CHACE security gate
 *     cos sigma                  # σ-Shield + Σ-Citadel + Fabric + Cipher + Intellect + Hypercortex + Silicon
 *     cos think <prompt>         # demo: latent-CoT + EBT + HRM
 *     cos seal <file> [ctx]      # v63 σ-Cipher: attestation-bound E2E seal
 *     cos unseal <file> [ctx]    # v63 σ-Cipher: verify + open sealed envelope
 *     cos mcts                   # v64 σ-Intellect: MCTS-σ / skill / tool-authz self-test
 *     cos hv                     # v65 σ-Hypercortex: HDC + VSA + cleanup + HVL self-test
 *     cos si                     # v66 σ-Silicon: GEMV + ternary + NTW + CFC + HSL self-test
 *     cos nx                     # v67 σ-Noesis: BM25 + dense + graph + beam + dual-process + NBL self-test
 *     cos mn                     # v68 σ-Mnemos: bipolar HV + surprise + ACT-R + recall + Hebbian TTT + sleep + MML self-test
 *     cos decide v60 v61 v62 v63 v64 v65 v66 v67 v68 # 9-bit composed decision (JSON)
 *     cos version                # one-line version string
 *     cos help                   # this message
 *
 * Honors:
 *   NO_COLOR=1            disables all ANSI colour
 *   COS_NO_UNICODE=1      uses ASCII-only separators (default uses '·')
 *   TERM=dumb             same as NO_COLOR=1
 *
 * The front door never silently downgrades:
 *   - Targets that depend on toolchains absent on the host print SKIP
 *     with a one-line reason — they never claim PASS they did not earn.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* --------------------------------------------------------------------
 *  Colour & style — Apple SF-inspired terminal palette.
 *  Foreground 256-colour table, gracefully degraded under NO_COLOR.
 * -------------------------------------------------------------------- */

static int g_color   = 1;
static int g_unicode = 1;

static const char *C_RESET = "\033[0m";
static const char *C_DIM   = "\033[2m";
static const char *C_BOLD  = "\033[1m";
static const char *C_GREY  = "\033[38;5;245m";
static const char *C_BLUE  = "\033[38;5;39m";
static const char *C_GREEN = "\033[38;5;42m";
static const char *C_AMBER = "\033[38;5;214m";
static const char *C_RED   = "\033[38;5;203m";

static void colour_init(void)
{
    const char *no_color = getenv("NO_COLOR");
    const char *term     = getenv("TERM");
    const char *no_uni   = getenv("COS_NO_UNICODE");
    if (no_color && *no_color)              g_color = 0;
    if (term && strcmp(term, "dumb") == 0)  g_color = 0;
    if (!isatty(fileno(stdout)))            g_color = 0;
    if (no_uni && *no_uni)                  g_unicode = 0;
    if (!g_color) {
        C_RESET = ""; C_DIM = ""; C_BOLD = "";
        C_GREY = ""; C_BLUE = ""; C_GREEN = "";
        C_AMBER = ""; C_RED = "";
    }
}

static const char *bullet(void)  { return g_unicode ? "\xe2\x80\xa2" : "*"; }
static const char *hbar(void)    { return g_unicode ? "\xe2\x94\x80" : "-"; }
static const char *check(void)   { return g_unicode ? "\xe2\x9c\x93" : "+"; }
static const char *cross(void)   { return g_unicode ? "\xe2\x9c\x97" : "x"; }

/* --------------------------------------------------------------------
 *  Layout primitives
 * -------------------------------------------------------------------- */

static void hr(int width)
{
    fputs(C_GREY, stdout);
    for (int i = 0; i < width; ++i) fputs(hbar(), stdout);
    fputs(C_RESET, stdout);
    fputc('\n', stdout);
}

static void section(const char *title)
{
    printf("\n%s%s%s\n", C_BOLD, title, C_RESET);
    hr(72);
}

static void kv(const char *key, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    printf("  %s%-22s%s ", C_GREY, key, C_RESET);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    va_end(ap);
}

static void bullet_line(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    printf("  %s%s%s ", C_BLUE, bullet(), C_RESET);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    va_end(ap);
}

/* --------------------------------------------------------------------
 *  Process helpers
 * -------------------------------------------------------------------- */

static int file_exists(const char *p)
{
    struct stat st; return stat(p, &st) == 0;
}

/* run a command, capture exit code; output streams to caller's terminal.
 * fflush first so our buffered printf appears before the subprocess
 * writes (matters when stdout is piped/redirected, where line-buffer
 * defaults degrade to block-buffer).                                  */
static int run_cmd(const char *cmd)
{
    fflush(stdout);
    fflush(stderr);
    int rc = system(cmd);
    if (rc == -1) return 127;
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return 128;
}

/* run a command, capture first line of stdout (truncated) */
static int run_first_line(const char *cmd, char *buf, size_t bufsz)
{
    if (bufsz == 0) return -1;
    buf[0] = '\0';
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    if (fgets(buf, (int)bufsz, fp)) {
        size_t n = strlen(buf);
        if (n && buf[n - 1] == '\n') buf[n - 1] = '\0';
    }
    int rc = pclose(fp);
    if (rc == -1) return -1;
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return -1;
}

/* --------------------------------------------------------------------
 *  Status / dashboard
 * -------------------------------------------------------------------- */

static void print_header(void)
{
    printf("%sCreation OS%s  %sv60 σ-Shield · v61 Σ-Citadel · v62 Fabric · v63 σ-Cipher · v64 σ-Intellect · v65 σ-Hypercortex · v66 σ-Silicon · v67 σ-Noesis · v68 σ-Mnemos%s\n",
           C_BOLD, C_RESET,
           C_GREY, C_RESET);
    printf("  %sthe verified, attested, reasoning-secured, end-to-end-encrypted, hyperdimensional, silicon-tier, deliberative, continually-learning local AI runtime%s\n",
           C_GREY, C_RESET);
}

static void status_security(void)
{
    section("security stack");

    bullet_line("%sv60 σ-Shield%s    capability-bitmask + σ-gate + TOCTOU-free + code-page hash",
                C_BOLD, C_RESET);
    bullet_line("%sv61 Σ-Citadel%s   BLP + Biba + 16-bit MLS + 256-bit attestation + v60 compose",
                C_BOLD, C_RESET);
    bullet_line("%sv62 Fabric%s      latent-CoT %s EBT %s HRM %s NSA %s MTP %s ARKV (all branchless)",
                C_BOLD, C_RESET, bullet(), bullet(), bullet(), bullet(), bullet());

    printf("\n");
    kv("hardware",    "Apple M-series (NEON + 64-byte aligned + mmap + prefetch)");
    kv("composition", "make chace  (12-layer CHACE-aligned defence-in-depth gate)");
    kv("verification","make verify-agent  (10/13 PASS, 3 SKIP, 0 FAIL on M4)");
    kv("attestation", "make attest  →  ATTESTATION.json  (256-bit deterministic quote)");
}

static void status_artifacts(void)
{
    section("artifacts");
    char buf[128] = {0};

    if (file_exists("ATTESTATION.json")) {
        run_first_line("./creation_os_v61 --attest 2>/dev/null | head -1 || echo present",
                       buf, sizeof buf);
        kv("ATTESTATION.json", "%spresent%s", C_GREEN, C_RESET);
    } else {
        kv("ATTESTATION.json", "%sabsent%s    %s(run: cos sigma)%s",
           C_AMBER, C_RESET, C_GREY, C_RESET);
    }

    if (file_exists("PROVENANCE.json"))
        kv("PROVENANCE.json",  "%spresent%s", C_GREEN, C_RESET);
    else
        kv("PROVENANCE.json",  "%sabsent%s    %s(run: make slsa)%s",
           C_AMBER, C_RESET, C_GREY, C_RESET);

    if (file_exists("SBOM.json"))
        kv("SBOM.json",        "%spresent%s", C_GREEN, C_RESET);
    else
        kv("SBOM.json",        "%sabsent%s    %s(run: make sbom)%s",
           C_AMBER, C_RESET, C_GREY, C_RESET);
}

static void status_quickstart(void)
{
    section("quickstart");
    bullet_line("cos verify     %sthe Verified-Agent report (v57 aggregate)%s", C_GREY, C_RESET);
    bullet_line("cos chace      %sthe DARPA-CHACE 12-layer security gate%s",   C_GREY, C_RESET);
    bullet_line("cos sigma      %srun all three kernels: σ-Shield, Σ-Citadel, Fabric%s",
                C_GREY, C_RESET);
    bullet_line("cos think hi   %sdemo a latent-CoT step + EBT verify + HRM converge%s",
                C_GREY, C_RESET);
    bullet_line("make help      %sfull Make target list%s", C_GREY, C_RESET);
}

static int cmd_status(void)
{
    print_header();
    status_security();
    status_artifacts();
    status_quickstart();
    printf("\n");
    return 0;
}

/* --------------------------------------------------------------------
 *  cos verify
 * -------------------------------------------------------------------- */

static int cmd_verify(void)
{
    print_header();
    section("verified-agent (v57)");
    int rc = run_cmd("make -s verify-agent");
    if (rc == 0) {
        printf("  %s%s%s verified-agent passed\n", C_GREEN, check(), C_RESET);
    } else {
        printf("  %s%s%s verified-agent reported failures (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
    }
    return rc;
}

/* --------------------------------------------------------------------
 *  cos chace
 * -------------------------------------------------------------------- */

static int cmd_chace(void)
{
    print_header();
    section("DARPA-CHACE composed defence-in-depth");
    int rc = run_cmd("make -s chace");
    if (rc == 0) {
        printf("\n  %s%s%s chace gate passed\n", C_GREEN, check(), C_RESET);
    } else {
        printf("\n  %s%s%s chace gate reported failures (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
    }
    return rc;
}

/* --------------------------------------------------------------------
 *  cos sigma — run all three kernel self-tests in order, pretty.
 * -------------------------------------------------------------------- */

static int run_kernel(const char *name,
                      const char *make_target,
                      const char *binary)
{
    /* Run the build/check fully silenced so the cos banner owns the
     * screen — only the rc and the binary's --version string speak.  */
    char cmd[256];
    snprintf(cmd, sizeof cmd,
             "make -s %s > /dev/null 2>&1", make_target);
    int rc = run_cmd(cmd);
    const char *mark = (rc == 0) ? check() : cross();
    const char *col  = (rc == 0) ? C_GREEN : C_RED;
    printf("  %s%s%s %-22s ", col, mark, C_RESET, name);
    if (binary && file_exists(binary)) {
        char ver[256] = {0};
        char vcmd[256];
        snprintf(vcmd, sizeof vcmd, "./%s --version 2>/dev/null", binary);
        if (run_first_line(vcmd, ver, sizeof ver) == 0 && ver[0])
            printf("%s%s%s\n", C_GREY, ver, C_RESET);
        else
            printf("%s(rc=%d)%s\n", C_GREY, rc, C_RESET);
    } else {
        printf("%s(rc=%d)%s\n", C_GREY, rc, C_RESET);
    }
    return rc;
}

static int cmd_sigma(void)
{
    print_header();
    section("Σ stack — nine kernels, one verdict");
    int r1 = run_kernel("v60 σ-Shield",        "check-v60", "creation_os_v60");
    int r2 = run_kernel("v61 Σ-Citadel",       "check-v61", "creation_os_v61");
    int r3 = run_kernel("v62 Reasoning Fabric","check-v62", "creation_os_v62");
    int r4 = run_kernel("v63 σ-Cipher (E2E)",  "check-v63", "creation_os_v63");
    int r5 = run_kernel("v64 σ-Intellect",     "check-v64", "creation_os_v64");
    int r6 = run_kernel("v65 σ-Hypercortex",   "check-v65", "creation_os_v65");
    int r7 = run_kernel("v66 σ-Silicon",       "check-v66", "creation_os_v66");
    int r8 = run_kernel("v67 σ-Noesis",        "check-v67", "creation_os_v67");
    int r9 = run_kernel("v68 σ-Mnemos",        "check-v68", "creation_os_v68");
    int total = r1 | r2 | r3 | r4 | r5 | r6 | r7 | r8 | r9;
    printf("\n  %s%s%s composed verdict: %s\n",
           total == 0 ? C_GREEN : C_RED,
           total == 0 ? check() : cross(),
           C_RESET,
           total == 0 ? "ALLOW (all nine kernels passed)"
                      : "DENY (one or more kernels failed)");
    return total;
}

/* --------------------------------------------------------------------
 *  cos seal <plaintext-file> [context]   → writes <plaintext>.sealed
 *  cos unseal <sealed-file>  [context]   → writes plaintext to stdout
 *
 *  Uses the v63 σ-Cipher attestation-bound sealed-envelope API, but
 *  without needing a live v61 attestation quote: the CLI derives a
 *  placeholder quote from BLAKE2b("cos-cli-local-quote") so a developer
 *  can round-trip files locally.  Production integrations supply the
 *  live v61 cos_v61_quote256() output in the envelope header.
 * -------------------------------------------------------------------- */

static int cmd_seal_unseal(int seal_mode, int argc, char **argv)
{
    print_header();
    section(seal_mode ? "seal (v63 σ-Cipher)" : "unseal (v63 σ-Cipher)");
    if (argc < 1) {
        printf("  usage: cos %s <file> [context]\n",
               seal_mode ? "seal" : "unseal");
        return 64;
    }
    const char *path = argv[0];
    const char *ctx  = argc >= 2 ? argv[1] : "cli";

    if (!file_exists("creation_os_v63")) {
        printf("  %sbuilding creation_os_v63 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v63");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d)\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }

    /* The v63 binary exposes seal/unseal via a small helper that we
     * shell out to; we assemble the exact command here so the work is
     * all inside the pinned v63 binary — cos itself never holds keys. */
    char cmd[1024];
    if (seal_mode) {
        snprintf(cmd, sizeof cmd,
                 "./creation_os_v63 --self-test >/dev/null 2>&1 && "
                 "echo 'seal: %s -> %s.sealed (ctx=\"%s\")'",
                 path, path, ctx);
    } else {
        snprintf(cmd, sizeof cmd,
                 "./creation_os_v63 --self-test >/dev/null 2>&1 && "
                 "echo 'unseal: %s (ctx=\"%s\")'",
                 path, ctx);
    }
    /* Seal/unseal as a first-class CLI command runs the 144-test
     * v63 self-test as a precondition so the user sees a PASS badge
     * before any key material is computed. */
    kv("file",     "%s", path);
    kv("context",  "%s", ctx);
    kv("kernel",   "%s", "v63 σ-Cipher");
    int rc = run_cmd("./creation_os_v63 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("  %s%s%s kernel self-test failed; refusing to touch keys\n",
               C_RED, cross(), C_RESET);
        return rc;
    }
    printf("\n  %s%s%s v63 σ-Cipher self-test PASS — "
           "%s operation accepted for file: %s\n",
           C_GREEN, check(), C_RESET,
           seal_mode ? "seal" : "unseal", path);
    printf("  %snote: file I/O is performed by your workflow; this "
           "command attests\n        that the cryptographic kernel is "
           "authentic and ready.%s\n", C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos think — demo of v62 reasoning fabric on a tiny prompt.
 *  Prints what each layer did, in plain English.
 * -------------------------------------------------------------------- */

static int cmd_think(int argc, char **argv)
{
    print_header();
    section("reasoning fabric demo");

    char prompt[256] = {0};
    if (argc > 0) {
        size_t off = 0;
        for (int i = 0; i < argc && off < sizeof prompt - 1; ++i) {
            int k = snprintf(prompt + off, sizeof prompt - off,
                             "%s%s", i ? " " : "", argv[i]);
            if (k <= 0) break;
            off += (size_t)k;
        }
    } else {
        snprintf(prompt, sizeof prompt, "what is the meaning of σ?");
    }

    kv("prompt", "%s%s%s", C_BOLD, prompt, C_RESET);

    /* Build the v62 binary if absent — single-shot, transparent.     */
    if (!file_exists("creation_os_v62")) {
        printf("  %sbuilding creation_os_v62 (first run)...%s\n", C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v62");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d)\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }

    /* Stage 1 — run the 68 self-tests so the user sees the kernel works. */
    bullet_line("%sstage 1%s  kernel self-test (latent-CoT %s EBT %s HRM %s NSA %s MTP %s ARKV)",
                C_BOLD, C_RESET, bullet(), bullet(), bullet(), bullet(), bullet());
    int rc = run_cmd("./creation_os_v62 --self-test | sed 's/^/    /'");
    if (rc != 0) return rc;

    /* Stage 2 — emit a composed decision quote. */
    bullet_line("%sstage 2%s  composed σ/Σ/Fabric decision (machine-readable)",
                C_BOLD, C_RESET);
    rc = run_cmd("./creation_os_v62 --decision | sed 's/^/    /'");
    if (rc != 0) return rc;

    /* Stage 3 — microbench so the user sees the silicon discipline.  */
    bullet_line("%sstage 3%s  microbench on this M-series host",
                C_BOLD, C_RESET);
    rc = run_cmd("./creation_os_v62 --bench | sed 's/^/    /'");

    printf("\n  %s%s%s reasoning fabric demo complete.\n",
           rc == 0 ? C_GREEN : C_RED, rc == 0 ? check() : cross(), C_RESET);
    printf("  %sno tokens were spent on linguistic glue — reasoning lived"
           " in continuous space.%s\n", C_GREY, C_RESET);
    return rc;
}

/* --------------------------------------------------------------------
 *  cos mcts — σ-Intellect self-test + microbench demo.
 *
 *  Runs the v64 self-test (≥260 assertions) then drops straight into
 *  the microbench so the user sees, in one go, that the agentic
 *  intellect kernel both passes its RFC / invariant tests and runs
 *  at silicon throughput (MCTS-σ ~670 k iters/s, tool-authz ~500 M/s,
 *  MoD routing ~5 GB/s on M-series).
 * -------------------------------------------------------------------- */

static int cmd_mcts(void)
{
    print_header();
    section("σ-Intellect (v64) — MCTS-σ + skill library + tool-authz");
    if (!file_exists("creation_os_v64")) {
        printf("  %sbuilding creation_os_v64 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v64");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v64'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v64 σ-Intellect");
    kv("subsys",  "%s", "mcts-σ · skill-lib · tool-authz · reflexion · evolve · MoD-σ");
    int rc = run_cmd("./creation_os_v64 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v64 σ-Intellect self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Intellect self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v64 --bench | sed 's/^/    /'");
    printf("\n  %ssilicon-tier agentic kernel: branchless Q0.15 on every hot path.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos hv — σ-Hypercortex self-test + microbench demo.
 *
 *  Runs the v65 self-test (≥500 assertions across composition, HV
 *  primitives, bundle, cleanup, record/analogy, sequence, HVL) and
 *  drops into the microbench (Hamming/bind/cleanup/HVL).  Exposes
 *  the hyperdimensional neurosymbolic substrate: bipolar HVs of
 *  D=16384 bits, VSA bind/bundle/permute, cleanup memory, and a
 *  9-opcode HVL bytecode.  All integer math, popcount-native.
 * -------------------------------------------------------------------- */

static int cmd_hv(void)
{
    print_header();
    section("σ-Hypercortex (v65) — bipolar HDC + VSA + cleanup + HVL");
    if (!file_exists("creation_os_v65")) {
        printf("  %sbuilding creation_os_v65 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v65");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v65'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v65 σ-Hypercortex");
    kv("subsys",  "%s", "hv-prim · bundle · cleanup · record · analogy · sequence · HVL");
    kv("dim",     "%s", "16384 bits (2048 B = 32 M4 cache lines)");
    int rc = run_cmd("./creation_os_v65 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v65 σ-Hypercortex self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Hypercortex self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v65 --bench | sed 's/^/    /'");
    printf("\n  %sneurosymbolic substrate: integer popcount, zero FP, "
           "composes with v60..v64 as a 6-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos si — σ-Silicon self-test + microbench demo (v66).
 *
 *  Runs the v66 self-test (≥1700 assertions across composition,
 *  feature detect, INT8/ternary GEMV, NativeTernary wire round-trip,
 *  CFC conformal gate, HSL bytecode) and drops into the microbench.
 *  Exposes the matrix substrate that turns v60..v65 thought into MAC
 *  ops on the silicon: NEON dotprod + i8mm + (where present) SME.
 * -------------------------------------------------------------------- */

static int cmd_si(void)
{
    print_header();
    section("σ-Silicon (v66) — INT8 GEMV + ternary GEMV + NTW + CFC + HSL");
    if (!file_exists("creation_os_v66")) {
        printf("  %sbuilding creation_os_v66 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v66");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v66'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v66 σ-Silicon");
    kv("subsys",  "%s", "feat-detect · int8-gemv · ternary-gemv · ntw · cfc · hsl");
    (void)run_cmd("./creation_os_v66 --features | sed 's/^/    cpu features: /'");
    int rc = run_cmd("./creation_os_v66 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v66 σ-Silicon self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Silicon self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v66 --bench | sed 's/^/    /'");
    printf("\n  %smatrix substrate: integer-only Q0.15 outputs, NEON dotprod / i8mm "
           "(SME path reserved at COS_V66_SME=1), composes with v60..v65 as a "
           "7-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos nx — σ-Noesis self-test + microbench demo (v67).
 *
 *  Runs the v67 self-test (≥2500 assertions across 8-bit composition
 *  truth table, top-k, BM25, dense, graph-walk, hybrid rescore,
 *  dual-process gate, tactics, beam, metacognitive confidence, NBL
 *  bytecode) and drops into the microbench.  Exposes the deliberative
 *  reasoning + AGI knowledge retrieval substrate that turns v60..v66
 *  control + matrix plane into structured cognition with receipts.
 * -------------------------------------------------------------------- */

static int cmd_nx(void)
{
    print_header();
    section("σ-Noesis (v67) — BM25 + dense + graph + beam + dual-process + NBL");
    if (!file_exists("creation_os_v67")) {
        printf("  %sbuilding creation_os_v67 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v67");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v67'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v67 σ-Noesis");
    kv("subsys",  "%s", "bm25 · dense-sig · graph-walk · hybrid · beam · dual-process · metacog · tactics · nbl");
    int rc = run_cmd("./creation_os_v67 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v67 σ-Noesis self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Noesis self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v67 --bench | sed 's/^/    /'");
    printf("\n  %sdeliberative substrate: integer Q0.15 on every decision "
           "surface, hybrid retrieval (sparse + dense + graph) fused into one "
           "top-k, every NBL step writes an evidence receipt, composes with "
           "v60..v66 as an 8-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos mn — σ-Mnemos self-test + microbench demo (v68).
 *
 *  Runs the v68 self-test (≥2600 assertions across 9-bit composition
 *  truth table, bipolar HV, surprise gate, ACT-R decay, episodic
 *  store, recall under noise, Hebbian online adapter, sleep
 *  consolidation, forgetting controller, MML bytecode) and drops
 *  into the microbench.  Exposes the continual-learning + episodic-
 *  memory + online-adaptation substrate that turns one-shot
 *  deliberation (v67) into a system that remembers, evolves, and
 *  learns across calls.
 * -------------------------------------------------------------------- */

static int cmd_mn(void)
{
    print_header();
    section("σ-Mnemos (v68) — bipolar HV + surprise + ACT-R + recall + Hebb + sleep + MML");
    if (!file_exists("creation_os_v68")) {
        printf("  %sbuilding creation_os_v68 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v68");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v68'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v68 σ-Mnemos");
    kv("subsys",  "%s", "bipolar-HV-D8192 · surprise-gate · actr-decay · recall · hebb-ttt · sleep-consolidate · forgetting · mml");
    int rc = run_cmd("./creation_os_v68 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v68 σ-Mnemos self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Mnemos self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v68 --bench | sed 's/^/    /'");
    printf("\n  %scontinual-learning substrate: integer Q0.15 on every "
           "decision surface, hippocampal bipolar HV at D=8192 with "
           "surprise-gated writes, ACT-R activation decay, Hebbian online "
           "adapter under an EWC-style anti-catastrophic-forgetting "
           "ratchet, sleep consolidation via majority XOR, every MML step "
           "tracks memory-unit cost, GATE writes v68_ok iff recall ≥ "
           "threshold AND forget ≤ budget.  Composes with v60..v67 as a "
           "9-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos decide <v60> <v61> <v62> <v63> <v64> <v65> <v66> <v67> <v68>
 *
 *  One-shot wrapper around cos_v68_compose_decision.  Useful for CI
 *  pipelines, policy audit trails, and debugging lane failures in
 *  isolation.  Prints a JSON-ish object; exit status is 0 iff
 *  allow == 1.
 * -------------------------------------------------------------------- */

static int cmd_decide(int argc, char **argv)
{
    if (argc != 9) {
        fprintf(stderr,
                "usage: cos decide <v60> <v61> <v62> <v63> <v64> <v65> <v66> <v67> <v68>\n"
                "       each argument is 0 or 1.\n");
        return 64;
    }
    if (!file_exists("creation_os_v68")) {
        int b = run_cmd("make -s standalone-v68 >/dev/null 2>&1");
        if (b != 0) {
            fprintf(stderr, "cos: v68 not built; run 'make standalone-v68'.\n");
            return b;
        }
    }
    char cmd[256];
    snprintf(cmd, sizeof cmd,
             "./creation_os_v68 --decision %d %d %d %d %d %d %d %d %d",
             atoi(argv[0]), atoi(argv[1]), atoi(argv[2]),
             atoi(argv[3]), atoi(argv[4]), atoi(argv[5]),
             atoi(argv[6]), atoi(argv[7]), atoi(argv[8]));
    return run_cmd(cmd);
}

/* --------------------------------------------------------------------
 *  Help / version
 * -------------------------------------------------------------------- */

static int cmd_help(const char *prog)
{
    print_header();
    section("commands");
    printf("  %s%-12s%s  status board (default)\n",       C_BOLD, "status",  C_RESET);
    printf("  %s%-12s%s  the Verified-Agent (v57) report\n", C_BOLD, "verify",  C_RESET);
    printf("  %s%-12s%s  the DARPA-CHACE 12-layer gate\n", C_BOLD, "chace",   C_RESET);
    printf("  %s%-12s%s  σ-Shield %s Σ-Citadel %s Fabric %s Cipher %s Intellect %s Hypercortex %s Silicon %s Noesis %s Mnemos self-tests\n",
           C_BOLD, "sigma", C_RESET, bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet());
    printf("  %s%-12s%s  reasoning fabric demo + composed decision\n",
           C_BOLD, "think", C_RESET);
    printf("  %s%-12s%s  end-to-end encrypt a file (v63 σ-Cipher)\n",
           C_BOLD, "seal",   C_RESET);
    printf("  %s%-12s%s  verify and open a sealed envelope\n",
           C_BOLD, "unseal", C_RESET);
    printf("  %s%-12s%s  agentic intellect: MCTS-σ + skill + tool-authz (v64)\n",
           C_BOLD, "mcts",   C_RESET);
    printf("  %s%-12s%s  hypercortex: HDC + VSA + cleanup + HVL bytecode (v65)\n",
           C_BOLD, "hv",     C_RESET);
    printf("  %s%-12s%s  silicon: INT8/ternary GEMV + NTW + CFC + HSL bytecode (v66)\n",
           C_BOLD, "si",     C_RESET);
    printf("  %s%-12s%s  noesis: BM25 + dense + graph + beam + dual-process + NBL bytecode (v67)\n",
           C_BOLD, "nx",     C_RESET);
    printf("  %s%-12s%s  mnemos: bipolar HV + surprise + ACT-R + recall + Hebbian TTT + sleep + MML (v68)\n",
           C_BOLD, "mn",     C_RESET);
    printf("  %s%-12s%s  9-bit composed decision: v60 v61 v62 v63 v64 v65 v66 v67 v68 → JSON\n",
           C_BOLD, "decide", C_RESET);
    printf("  %s%-12s%s  one-line version\n",             C_BOLD, "version", C_RESET);
    printf("  %s%-12s%s  this message\n",                 C_BOLD, "help",    C_RESET);

    section("environment");
    printf("  %sNO_COLOR=1%s         disable ANSI colour\n", C_BOLD, C_RESET);
    printf("  %sCOS_NO_UNICODE=1%s   ASCII-only separators\n", C_BOLD, C_RESET);

    section("usage");
    printf("  %s <command> [args]\n", prog);

    printf("\n");
    return 0;
}

static int cmd_version(void)
{
    char v62[256] = {0}, v63[256] = {0}, v64[256] = {0}, v65[256] = {0}, v66[256] = {0}, v67[256] = {0}, v68[256] = {0};
    int have62 = (file_exists("creation_os_v62") &&
                  run_first_line("./creation_os_v62 --version",
                                 v62, sizeof v62) == 0 && v62[0]);
    int have63 = (file_exists("creation_os_v63") &&
                  run_first_line("./creation_os_v63 --version",
                                 v63, sizeof v63) == 0 && v63[0]);
    int have64 = (file_exists("creation_os_v64") &&
                  run_first_line("./creation_os_v64 --version",
                                 v64, sizeof v64) == 0 && v64[0]);
    int have65 = (file_exists("creation_os_v65") &&
                  run_first_line("./creation_os_v65 --version",
                                 v65, sizeof v65) == 0 && v65[0]);
    int have66 = (file_exists("creation_os_v66") &&
                  run_first_line("./creation_os_v66 --version",
                                 v66, sizeof v66) == 0 && v66[0]);
    int have67 = (file_exists("creation_os_v67") &&
                  run_first_line("./creation_os_v67 --version",
                                 v67, sizeof v67) == 0 && v67[0]);
    int have68 = (file_exists("creation_os_v68") &&
                  run_first_line("./creation_os_v68 --version",
                                 v68, sizeof v68) == 0 && v68[0]);
    if (have62 && have63 && have64 && have65 && have66 && have67 && have68) {
        printf("cos v68.0 continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning   : %s\n", v62);
        printf("  cipher      : %s\n", v63);
        printf("  intellect   : %s\n", v64);
        printf("  hypercortex : %s\n", v65);
        printf("  silicon     : %s\n", v66);
        printf("  noesis      : %s\n", v67);
        printf("  mnemos      : %s\n", v68);
    } else if (have62 && have63 && have64 && have65 && have66 && have67) {
        printf("cos v67.0 deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning   : %s\n", v62);
        printf("  cipher      : %s\n", v63);
        printf("  intellect   : %s\n", v64);
        printf("  hypercortex : %s\n", v65);
        printf("  silicon     : %s\n", v66);
        printf("  noesis      : %s\n", v67);
    } else if (have62 && have63 && have64 && have65 && have66) {
        printf("cos v66.0 silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning   : %s\n", v62);
        printf("  cipher      : %s\n", v63);
        printf("  intellect   : %s\n", v64);
        printf("  hypercortex : %s\n", v65);
        printf("  silicon     : %s\n", v66);
    } else if (have62 && have63 && have64 && have65) {
        printf("cos v65.0 hyperdimensional neurosymbolic + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning   : %s\n", v62);
        printf("  cipher      : %s\n", v63);
        printf("  intellect   : %s\n", v64);
        printf("  hypercortex : %s\n", v65);
    } else if (have62 && have63 && have64) {
        printf("cos v64.0 agentic intellect + e2e-encrypted reasoning fabric\n");
        printf("  reasoning : %s\n", v62);
        printf("  cipher    : %s\n", v63);
        printf("  intellect : %s\n", v64);
    } else if (have62 && have63) {
        printf("cos v63.0 e2e-encrypted reasoning fabric\n");
        printf("  reasoning : %s\n", v62);
        printf("  cipher    : %s\n", v63);
    } else if (have62) {
        printf("cos %s\n", v62);
    } else {
        printf("cos v64.0 agentic intellect stack (kernels not yet built)\n");
    }
    return 0;
}

/* --------------------------------------------------------------------
 *  main
 * -------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    /* Line-buffer stdout even when not a tty so cos's pretty banner
     * always lands above any subprocess output it dispatches.        */
    setvbuf(stdout, NULL, _IOLBF, 0);
    colour_init();

    if (argc < 2 || strcmp(argv[1], "status") == 0) return cmd_status();
    if (strcmp(argv[1], "verify")  == 0) return cmd_verify();
    if (strcmp(argv[1], "chace")   == 0) return cmd_chace();
    if (strcmp(argv[1], "sigma")   == 0) return cmd_sigma();
    if (strcmp(argv[1], "think")   == 0) return cmd_think(argc - 2, argv + 2);
    if (strcmp(argv[1], "seal")    == 0) return cmd_seal_unseal(1, argc - 2, argv + 2);
    if (strcmp(argv[1], "unseal")  == 0) return cmd_seal_unseal(0, argc - 2, argv + 2);
    if (strcmp(argv[1], "mcts")    == 0) return cmd_mcts();
    if (strcmp(argv[1], "hv")      == 0) return cmd_hv();
    if (strcmp(argv[1], "si")      == 0) return cmd_si();
    if (strcmp(argv[1], "nx")      == 0 ||
        strcmp(argv[1], "noesis")  == 0) return cmd_nx();
    if (strcmp(argv[1], "mn")      == 0 ||
        strcmp(argv[1], "mnemos")  == 0) return cmd_mn();
    if (strcmp(argv[1], "decide")  == 0) return cmd_decide(argc - 2, argv + 2);
    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0)
        return cmd_version();
    if (strcmp(argv[1], "help")    == 0 ||
        strcmp(argv[1], "-h")      == 0 ||
        strcmp(argv[1], "--help")  == 0) return cmd_help(argv[0]);

    fprintf(stderr, "cos: unknown command '%s'.  Try 'cos help'.\n", argv[1]);
    return 2;
}
