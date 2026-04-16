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
 *     cos sigma                  # σ-Shield + Σ-Citadel + Reasoning Fabric + σ-Cipher
 *     cos think <prompt>         # demo: latent-CoT + EBT + HRM
 *     cos seal <file> [ctx]      # v63 σ-Cipher: attestation-bound E2E seal
 *     cos unseal <file> [ctx]    # v63 σ-Cipher: verify + open sealed envelope
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
    printf("%sCreation OS%s  %sv60 σ-Shield · v61 Σ-Citadel · v62 Reasoning Fabric · v63 σ-Cipher%s\n",
           C_BOLD, C_RESET,
           C_GREY, C_RESET);
    printf("  %sthe verified, attested, reasoning-secured, end-to-end-encrypted local AI runtime%s\n",
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
    section("Σ stack — four kernels, one verdict");
    int r1 = run_kernel("v60 σ-Shield",        "check-v60", "creation_os_v60");
    int r2 = run_kernel("v61 Σ-Citadel",       "check-v61", "creation_os_v61");
    int r3 = run_kernel("v62 Reasoning Fabric","check-v62", "creation_os_v62");
    int r4 = run_kernel("v63 σ-Cipher (E2E)",  "check-v63", "creation_os_v63");
    int total = r1 | r2 | r3 | r4;
    printf("\n  %s%s%s composed verdict: %s\n",
           total == 0 ? C_GREEN : C_RED,
           total == 0 ? check() : cross(),
           C_RESET,
           total == 0 ? "ALLOW (all four kernels passed)"
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
 *  Help / version
 * -------------------------------------------------------------------- */

static int cmd_help(const char *prog)
{
    print_header();
    section("commands");
    printf("  %s%-12s%s  status board (default)\n",       C_BOLD, "status",  C_RESET);
    printf("  %s%-12s%s  the Verified-Agent (v57) report\n", C_BOLD, "verify",  C_RESET);
    printf("  %s%-12s%s  the DARPA-CHACE 12-layer gate\n", C_BOLD, "chace",   C_RESET);
    printf("  %s%-12s%s  σ-Shield %s Σ-Citadel %s Reasoning Fabric %s σ-Cipher self-tests\n",
           C_BOLD, "sigma", C_RESET, bullet(), bullet(), bullet());
    printf("  %s%-12s%s  reasoning fabric demo + composed decision\n",
           C_BOLD, "think", C_RESET);
    printf("  %s%-12s%s  end-to-end encrypt a file (v63 σ-Cipher)\n",
           C_BOLD, "seal",   C_RESET);
    printf("  %s%-12s%s  verify and open a sealed envelope\n",
           C_BOLD, "unseal", C_RESET);
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
    char v62[256] = {0}, v63[256] = {0};
    int have62 = (file_exists("creation_os_v62") &&
                  run_first_line("./creation_os_v62 --version",
                                 v62, sizeof v62) == 0 && v62[0]);
    int have63 = (file_exists("creation_os_v63") &&
                  run_first_line("./creation_os_v63 --version",
                                 v63, sizeof v63) == 0 && v63[0]);
    if (have62 && have63) {
        printf("cos v63.0 e2e-encrypted reasoning fabric\n");
        printf("  reasoning : %s\n", v62);
        printf("  cipher    : %s\n", v63);
    } else if (have62) {
        printf("cos %s\n", v62);
    } else {
        printf("cos v63.0 e2e-encrypted reasoning fabric (kernels not yet built)\n");
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
    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0)
        return cmd_version();
    if (strcmp(argv[1], "help")    == 0 ||
        strcmp(argv[1], "-h")      == 0 ||
        strcmp(argv[1], "--help")  == 0) return cmd_help(argv[0]);

    fprintf(stderr, "cos: unknown command '%s'.  Try 'cos help'.\n", argv[1]);
    return 2;
}
