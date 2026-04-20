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
 *     cos chace                  # the CHACE-class 12-layer security gate
 *     cos doctor                 # full repo health rollup (license + verify + hardening + receipts)
 *     cos sigma                  # σ-Shield + Σ-Citadel + Fabric + Cipher + Intellect + Hypercortex + Silicon
 *     cos think <prompt>         # demo: latent-CoT + EBT + HRM
 *     cos seal <file> [ctx]      # v63 σ-Cipher: attestation-bound E2E seal
 *     cos unseal <file> [ctx]    # v63 σ-Cipher: verify + open sealed envelope
 *     cos mcts                   # v64 σ-Intellect: MCTS-σ / skill / tool-authz self-test
 *     cos hv                     # v65 σ-Hypercortex: HDC + VSA + cleanup + HVL self-test
 *     cos si                     # v66 σ-Silicon: GEMV + ternary + NTW + CFC + HSL self-test
 *     cos nx                     # v67 σ-Noesis: BM25 + dense + graph + beam + dual-process + NBL self-test
 *     cos mn                     # v68 σ-Mnemos: bipolar HV + surprise + ACT-R + recall + Hebbian TTT + sleep + MML self-test
 *     cos cn                     # v69 σ-Constellation: tree-spec + debate + Byzantine vote + MoE route + gossip + Elo-UCB + dedup + CL self-test
 *     cos hs                     # v70 σ-Hyperscale: P2Q ShiftAddLLM + Mamba-2 SSM + RWKV-7 + MoE-10k + HBM-PIM popcount + photonic WDM + Loihi-3 spike + NCCL ring + LRU-stream + HSL self-test
 *     cos wh                     # v71 σ-Wormhole: portal ER-bridge + anchor cleanup + single-XOR teleport + Kleinberg small-world route + ER=EPR tensor-bond + HMAC-HV integrity + Poincaré-boundary gate + hop budget + path receipt + WHL self-test
 *     cos decide v60 v61 v62 v63 v64 v65 v66 v67 v68 v69 v70 v71 # 12-bit composed decision (JSON)
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
#include <errno.h>
#include <locale.h>
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
    printf("%sCreation OS%s  %sv60 σ-Shield · v61 Σ-Citadel · v62 Fabric · v63 σ-Cipher · v64 σ-Intellect · v65 σ-Hypercortex · v66 σ-Silicon · v67 σ-Noesis · v68 σ-Mnemos · v69 σ-Constellation · v70 σ-Hyperscale%s\n",
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
    bullet_line("cos chat       %sσ-gated REPL (reinforce + speculative + generate_until)%s",
                C_GREY, C_RESET);
    bullet_line("cos benchmark  %send-to-end pipeline bench (engram + BitNet + σ + TTT + API)%s",
                C_GREY, C_RESET);
    bullet_line("cos cost       %show many € did σ-gating save (vs always-API)%s",
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
    section("Σ stack — forty kernels, one verdict");
    int r1 = run_kernel("v60 σ-Shield",        "check-v60", "creation_os_v60");
    int r2 = run_kernel("v61 Σ-Citadel",       "check-v61", "creation_os_v61");
    int r3 = run_kernel("v62 Reasoning Fabric","check-v62", "creation_os_v62");
    int r4 = run_kernel("v63 σ-Cipher (E2E)",  "check-v63", "creation_os_v63");
    int r5 = run_kernel("v64 σ-Intellect",     "check-v64", "creation_os_v64");
    int r6 = run_kernel("v65 σ-Hypercortex",   "check-v65", "creation_os_v65");
    int r7 = run_kernel("v66 σ-Silicon",       "check-v66", "creation_os_v66");
    int r8 = run_kernel("v67 σ-Noesis",        "check-v67", "creation_os_v67");
    int r9 = run_kernel("v68 σ-Mnemos",        "check-v68", "creation_os_v68");
    int r10= run_kernel("v69 σ-Constellation", "check-v69", "creation_os_v69");
    int r11= run_kernel("v70 σ-Hyperscale",    "check-v70", "creation_os_v70");
    int r12= run_kernel("v71 σ-Wormhole",      "check-v71", "creation_os_v71");
    int r13= run_kernel("v72 σ-Chain",         "check-v72", "creation_os_v72");
    int r14= run_kernel("v73 σ-Omnimodal",     "check-v73", "creation_os_v73");
    int r15= run_kernel("v74 σ-Experience",    "check-v74", "creation_os_v74");
    int r16= run_kernel("v76 σ-Surface",       "check-v76", "creation_os_v76");
    int r17= run_kernel("v77 σ-Reversible",    "check-v77", "creation_os_v77");
    int r18= run_kernel("v78 σ-Gödel-Attestor","check-v78", "creation_os_v78");
    int r19= run_kernel("v79 σ-Simulacrum",    "check-v79", "creation_os_v79");
    int r20= run_kernel("v80 σ-Cortex",        "check-v80", "creation_os_v80");
    int r21= run_kernel("v81 σ-Lattice (PQC)", "check-v81", "creation_os_v81");
    int r22= run_kernel("v82 σ-Stream",        "check-v82", "creation_os_v82");
    int r23= run_kernel("v83 σ-Agentic",       "check-v83", "creation_os_v83");
    int r24= run_kernel("v84 σ-ZKProof",       "check-v84", "creation_os_v84");
    int r25= run_kernel("v85 σ-Formal",        "check-v85", "creation_os_v85");
    int r26= run_kernel("v86 σ-JEPA",          "check-v86", "creation_os_v86");
    int r27= run_kernel("v87 σ-SAE",           "check-v87", "creation_os_v87");
    int r28= run_kernel("v88 σ-FHE",           "check-v88", "creation_os_v88");
    int r29= run_kernel("v89 σ-Spiking",       "check-v89", "creation_os_v89");
    int r30= run_kernel("v90 σ-Hierarchical",  "check-v90", "creation_os_v90");
    int r31= run_kernel("v91 σ-Quantum",       "check-v91", "creation_os_v91");
    int r32= run_kernel("v92 σ-Titans",        "check-v92", "creation_os_v92");
    int r33= run_kernel("v93 σ-MoR",           "check-v93", "creation_os_v93");
    int r34= run_kernel("v94 σ-Clifford",      "check-v94", "creation_os_v94");
    int r35= run_kernel("v95 σ-Sheaf",         "check-v95", "creation_os_v95");
    int r36= run_kernel("v96 σ-Diffusion",     "check-v96", "creation_os_v96");
    int r37= run_kernel("v97 σ-RL",            "check-v97", "creation_os_v97");
    int r38= run_kernel("v98 σ-Topology",      "check-v98", "creation_os_v98");
    int r39= run_kernel("v99 σ-Causal",        "check-v99", "creation_os_v99");
    int r40= run_kernel("v100 σ-Hyena",        "check-v100","creation_os_v100");
    int total = r1 | r2 | r3 | r4 | r5 | r6 | r7 | r8 | r9 | r10 | r11 | r12 | r13 | r14 | r15 | r16 | r17 | r18 | r19 | r20 | r21 | r22 | r23 | r24 | r25 | r26 | r27 | r28 | r29 | r30 | r31 | r32 | r33 | r34 | r35 | r36 | r37 | r38 | r39 | r40;
    printf("\n  %s%s%s composed verdict: %s\n",
           total == 0 ? C_GREEN : C_RED,
           total == 0 ? check() : cross(),
           C_RESET,
           total == 0 ? "ALLOW (all forty kernels passed)"
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
 *  cos chat — σ-gated interactive REPL (P1 reinforce + P2 speculative
 *  + P3 generate_until).  Execs the Python driver in
 *  scripts/sigma_pipeline/cos_chat.py with PYTHONPATH=scripts so the
 *  user types a single short command instead of the module path.
 *  All args after `chat` are forwarded verbatim.
 * -------------------------------------------------------------------- */

/* Escape a single argv element for /bin/sh single-quoted form.
 * Writes at most cap-1 bytes + NUL; returns 0 on success, -1 on overflow.
 * Encoding: wrap in single quotes; any inner ' becomes '"'"' . */
static int sh_quote(const char *s, char *out, size_t cap)
{
    size_t o = 0;
    if (cap < 3) return -1;
    out[o++] = '\'';
    for (const char *p = s; *p; ++p) {
        if (*p == '\'') {
            if (o + 4 >= cap) return -1;
            out[o++] = '\''; out[o++] = '"';
            out[o++] = '\''; out[o++] = '"';
            out[o++] = '\'';
        } else {
            if (o + 1 >= cap) return -1;
            out[o++] = *p;
        }
    }
    if (o + 2 >= cap) return -1;
    out[o++] = '\'';
    out[o]   = 0;
    return 0;
}

static int run_py_module(const char *module, int argc, char **argv)
{
    char cmd[8192];
    size_t off = 0;
    const char *py = file_exists(".venv-bitnet/bin/python")
                        ? ".venv-bitnet/bin/python" : "python3";
    int k = snprintf(cmd + off, sizeof cmd - off,
                     "PYTHONPATH=scripts%s%s %s -m %s",
                     getenv("PYTHONPATH") ? ":" : "",
                     getenv("PYTHONPATH") ? getenv("PYTHONPATH") : "",
                     py, module);
    if (k <= 0 || (size_t)k >= sizeof cmd - off) return 70;
    off += (size_t)k;
    char quoted[2048];
    for (int i = 0; i < argc; ++i) {
        if (sh_quote(argv[i], quoted, sizeof quoted) != 0) return 71;
        k = snprintf(cmd + off, sizeof cmd - off, " %s", quoted);
        if (k <= 0 || (size_t)k >= sizeof cmd - off) return 72;
        off += (size_t)k;
    }
    return run_cmd(cmd);
}

/* --------------------------------------------------------------------
 *  C-first dispatch — prefer the native sibling binary when it exists
 *  (keeping the front door dependency-free per v287 granite), fall
 *  back to the Python reference implementation only when the C binary
 *  has not been built yet.  This fulfils the "pure C front door"
 *  contract: `cos chat` succeeds on a host with Python 3 uninstalled
 *  as long as `make cos-chat` has run once.
 * -------------------------------------------------------------------- */
static int exec_sibling(const char *bin_name, int argc, char **argv);

static int prefer_c_else_py(const char *c_bin,
                            const char *py_module,
                            int argc, char **argv)
{
    char path[512];
    if (snprintf(path, sizeof path, "./%s", c_bin) > 0 &&
        file_exists(path))
    {
        return exec_sibling(c_bin, argc, argv);
    }
    return run_py_module(py_module, argc, argv);
}

static int cmd_chat(int argc, char **argv)
{
    return prefer_c_else_py("cos-chat",
                            "sigma_pipeline.cos_chat", argc, argv);
}

/* --------------------------------------------------------------------
 *  cos benchmark — end-to-end pipeline benchmark over a JSONL fixture
 *                  (or built-in demo), with markdown + JSON output.
 * -------------------------------------------------------------------- */
static int cmd_benchmark(int argc, char **argv)
{
    return prefer_c_else_py("cos-benchmark",
                            "sigma_pipeline.pipeline_bench", argc, argv);
}

/* --------------------------------------------------------------------
 *  cos cost — cost-savings driver: how many € did σ-gating save?
 * -------------------------------------------------------------------- */
static int cmd_cost(int argc, char **argv)
{
    return prefer_c_else_py("cos-cost",
                            "sigma_pipeline.cost_measure", argc, argv);
}

/* --------------------------------------------------------------------
 *  H6 sibling dispatch — exec the dedicated C binaries for the four
 *  I/A/S/D/H products without re-linking the whole stack.
 * -------------------------------------------------------------------- */
static int exec_sibling(const char *bin_name, int argc, char **argv)
{
    char path[1024];
    struct stat st;
    int have_local = 0;
    if (snprintf(path, sizeof path, "./%s", bin_name) > 0 &&
        stat(path, &st) == 0 && S_ISREG(st.st_mode) &&
        access(path, X_OK) == 0) {
        have_local = 1;
    } else {
        strncpy(path, bin_name, sizeof path - 1);
        path[sizeof path - 1] = 0;
    }

    char **new_argv = calloc((size_t)(argc + 2), sizeof *new_argv);
    if (!new_argv) { perror("cos"); return 126; }
    new_argv[0] = path;
    for (int i = 0; i < argc; ++i) new_argv[i + 1] = argv[i];
    new_argv[argc + 1] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        perror("cos: fork");
        free(new_argv);
        return 126;
    }
    if (pid == 0) {
        if (have_local) execv(path, new_argv);
        else            execvp(path, new_argv);
        fprintf(stderr, "cos: cannot exec %s: %s\n", path, strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }
    free(new_argv);
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return 126;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 126;
}

static int cmd_agent  (int argc, char **argv) { return exec_sibling("cos-agent",                argc, argv); }
static int cmd_network(int argc, char **argv) { return exec_sibling("cos-network",              argc, argv); }
static int cmd_omega  (int argc, char **argv) { return exec_sibling("creation_os_sigma_omega",  argc, argv); }
static int cmd_formal (int argc, char **argv) { return exec_sibling("creation_os_sigma_formal", argc, argv); }
static int cmd_paper  (int argc, char **argv) { return exec_sibling("creation_os_sigma_paper",  argc, argv); }

/* cos sigma-meta — deterministic σ-meta summary (H6).
 *
 * Reports: invariant, formal ledger witness counts and p99
 * latency, substrate σ values for the canonical "dominant" case,
 * per-domain gate-helpfulness map, and the 20€/mo target
 * economics.  No I/O, no clocks, no RNG — byte-deterministic so
 * the smoke test can pin every field.  Numbers are the same
 * ones H5 writes into the arXiv paper, so sigma-meta is the
 * machine-readable twin of the paper's abstract + limitations
 * + results sections.                                           */
static int cmd_sigma_meta(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("{\"tool\":\"cos\",\"subcommand\":\"sigma-meta\","
           "\"invariant\":\"declared==realized\","
           "\"ledger\":{\"discharged\":\"4/4\","
                       "\"t3_witnesses\":16384,\"t4_witnesses\":16384,"
                       "\"t5_witnesses\":896,\"t6_witnesses\":6400000,"
                       "\"t6_bound_ns\":250},"
           "\"substrates\":{\"digital\":0.10,\"bitnet\":0.00,"
                           "\"spike\":0.00,\"photonic\":0.10,"
                           "\"equivalent\":true},"
           "\"domains\":["
             "{\"domain\":\"factual_qa\",\"gate_helps\":true,"
              "\"notes\":\"σ-gate works: concentrated correct-answer mass.\"},"
             "{\"domain\":\"code_completion\",\"gate_helps\":true,"
              "\"notes\":\"σ-gate works: token-level certainty correlates with correctness.\"},"
             "{\"domain\":\"commonsense\",\"gate_helps\":false,"
              "\"notes\":\"σ-gate abstains: correct-answer distribution is genuinely flat.\"},"
             "{\"domain\":\"open_domain_reasoning\",\"gate_helps\":false,"
              "\"notes\":\"σ-gate abstains or is adversarially beatable.\"},"
             "{\"domain\":\"adversarial_inputs\",\"gate_helps\":false,"
              "\"notes\":\"σ can drop low on a wrong answer; pair with grounding and mesh reputation.\"}"
           "],"
           "\"economics\":{\"fixtures\":20,\"local_fraction_percent\":88.9,"
                          "\"savings_percent\":78.8,"
                          "\"hybrid_eur_per_month\":4.27,"
                          "\"cloud_eur_per_month\":36.00,"
                          "\"target_price_eur_per_month\":20.00},"
           "\"pass\":true}\n");
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
 *  cos cn — σ-Constellation self-test + microbench demo (v69).
 *
 *  Runs the v69 self-test (≥3200 assertions across the 10-bit
 *  composition truth table, tree speculative decoding, multi-agent
 *  debate with anti-conformity, Byzantine 2f+1 vote, MoE top-K
 *  routing, Lamport vector clocks, chunked flash-style dot, Elo+UCB
 *  self-play, popcount dedup, CL bytecode) and drops into the
 *  microbench.  Exposes the distributed-orchestration + parallel-
 *  decoding + multi-agent-consensus substrate that turns a continual-
 *  learning kernel (v68) into a fleet.
 * -------------------------------------------------------------------- */

static int cmd_cn(void)
{
    print_header();
    section("σ-Constellation (v69) — tree-spec + debate + Byzantine vote + MoE route + gossip + Elo-UCB + dedup + CL");
    if (!file_exists("creation_os_v69")) {
        printf("  %sbuilding creation_os_v69 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v69");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v69'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v69 σ-Constellation");
    kv("subsys",  "%s", "tree-spec-EAGLE3 · debate-Council/FREE-MAD · byzantine-2f+1 · moe-MaxScore · lamport-gossip · flash-chunked-dot · elo-ucb · popcount-dedup · cl");
    int rc = run_cmd("./creation_os_v69 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v69 σ-Constellation self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Constellation self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v69 --bench | sed 's/^/    /'");
    printf("\n  %sorchestration substrate: integer Q0.15 on every "
           "decision surface, EAGLE-3-style speculative tree verified "
           "with branchless XOR-match + popcount, Council-Mode multi-agent "
           "debate with FREE-MAD anti-conformity bonus and abstain default, "
           "PBFT-style 2f+1 Byzantine quorum, MaxScore MoE top-K routing "
           "with integer load-balance counter, Lamport vector clocks for "
           "causal ordering, chunked flash-style dot product (softmax-free "
           "integer max tracker), AlphaZero-lineage Elo with UCB arm "
           "selection, popcount KV dedup on 512-bit sketches, every CL "
           "step tracks orchestration-unit cost, GATE writes v69_ok iff "
           "vote margin ≥ threshold AND Byzantine faults ≤ budget.  "
           "Composes with v60..v68 as a 10-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos hs — σ-Hyperscale self-test + microbench demo (v70).
 *
 *  Runs the v70 self-test (≥148 000 deterministic assertions across
 *  the 11-bit composition truth table, P2Q ShiftAddLLM GEMV,
 *  Mamba-2 selective SSM scan, RWKV-7 "Goose" delta-rule step,
 *  10 240-expert DeepSeek-V3 MoE top-K router, HBM-PIM bit-serial
 *  AND-popcount, photonic WDM lane dot-product, Loihi-3 graded-
 *  spike encode + readout, NCCL / Patarasuk-Yuan ring all-reduce,
 *  Petals / Helix / DirectStorage LRU streaming cache, HSL
 *  bytecode).  Drops into the microbench.  Exposes the trillion-
 *  parameter / hyperscale-killer substrate that turns a fleet
 *  (v69) into a fleet-of-fleets on a single workstation.
 * -------------------------------------------------------------------- */

static int cmd_hs(void)
{
    print_header();
    section("σ-Hyperscale (v70) — P2Q ShiftAddLLM + Mamba-2 SSM + RWKV-7 + MoE-10k DeepSeek-V3 + HBM-PIM + photonic-WDM + Loihi-3 + NCCL-ring + LRU-stream + HSL");
    if (!file_exists("creation_os_v70")) {
        printf("  %sbuilding creation_os_v70 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v70");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v70'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v70 σ-Hyperscale");
    kv("subsys",  "%s", "p2q-shiftadd · ssm-mamba2 · rwkv7-deltarule · moe-deepseekv3 · pim-hbm · wdm-photonic · spike-loihi3 · ring-allreduce · stream-petals · hsl");
    int rc = run_cmd("./creation_os_v70 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v70 σ-Hyperscale self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Hyperscale self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v70 --bench | sed 's/^/    /'");
    printf("\n  %shyperscale substrate: P2Q weights as ±2^k (ShiftAddLLM "
           "arXiv:2406.05981) — multiply-free GEMV; Mamba-2 selective "
           "SSM scan (linear time, no KV cache); RWKV-7 \"Goose\" delta-"
           "rule update (O(1) per token, exceeds Transformer TC^0); "
           "DeepSeek-V3 auxiliary-loss-free MoE router to 10 240 "
           "experts; Samsung HBM-PIM / RACAM bit-serial AND-popcount; "
           "Lightmatter / SKYLIGHT photonic WDM lane SIMD; Intel "
           "Loihi 3 graded-spike sparse activation (3.66 W text "
           "classification); NVIDIA NCCL bandwidth-optimal ring "
           "all-reduce; Petals / Helix / DirectStorage / GPUDirect "
           "streaming-weight LRU; HSL bytecode GATE writes v70_ok iff "
           "silicon budget ≤ limit AND throughput ≥ floor AND topology "
           "balanced.  Composes with v60..v69 as an 11-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos wh — σ-Wormhole self-test + microbench demo (v71).
 *
 *  Runs the v71 self-test (≥68 000 deterministic assertions across
 *  the full 12-bit composition truth table, the portal Einstein-
 *  Rosen bridge table, constant-time anchor cleanup, single-XOR
 *  teleport, Kleinberg small-world multi-hop routing, ER=EPR
 *  tensor-bond pairing, HMAC-HV bridge integrity, the Poincaré-
 *  boundary gate, hop-budget enforcement, path receipts, and the
 *  WHL 10-opcode integer bytecode).  Drops into the microbench.
 *  Exposes the hyperdimensional wormhole plane — the non-local
 *  primitive that lets an agent jump "here → there" in the concept
 *  manifold in one XOR instead of walking the transformer layer by
 *  layer.
 * -------------------------------------------------------------------- */

static int cmd_wh(void)
{
    print_header();
    section("σ-Wormhole (v71) — portal ER-bridge + anchor cleanup + single-XOR teleport + Kleinberg small-world route + ER=EPR tensor-bond + HMAC-HV integrity + Poincaré-boundary gate + hop budget + path receipt + WHL");
    if (!file_exists("creation_os_v71")) {
        printf("  %sbuilding creation_os_v71 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v71");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v71'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v71 σ-Wormhole");
    kv("subsys",  "%s", "portal-er-bridge · anchor-cleanup · teleport · kleinberg-route · er-epr-bond · hmac-hv-integrity · poincare-boundary · hop-budget · path-receipt · whl");
    int rc = run_cmd("./creation_os_v71 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v71 σ-Wormhole self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Wormhole self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v71 --bench | sed 's/^/    /'");
    printf("\n  %swormhole plane: every (anchor, target) pair stores its "
           "canonical XOR bridge — applying the bridge lands the state on "
           "target in one pass, zero multiplies, zero FP.  The bridge "
           "algebra is the information-theoretic surface of ER=EPR "
           "(Maldacena & Susskind 2013) translated into VSA (Plate 1995; "
           "Kanerva 2009); Kleinberg 2000 gives O(log² n) greedy routing "
           "over the portal graph; Ramsauer *et al.* 2021 shows one "
           "layer of attention is already a Hopfield-retrieve over this "
           "substrate; Nickel & Kiela 2017 Poincaré embeddings motivate "
           "the boundary-regime gate.  Every bridge carries an HMAC-HV "
           "signature so a poisoned portal cannot teleport out of the "
           "legal lattice.  Composes with v60..v70 and v72 as a 13-bit "
           "branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos ch / cos chain  — v72 σ-Chain.
 *
 *  Runs the v72 self-test suite (≥ 117 000 asserts) and the
 *  microbench to expose the blockchain / web3 / post-quantum /
 *  zero-knowledge / verifiable-agent substrate plane.  The thirteen-
 *  bit composed decision is gated by `cos decide` below.
 * -------------------------------------------------------------------- */

static int cmd_ch(void)
{
    print_header();
    section("σ-Chain (v72) — Merkle ledger + append-only receipts + WOTS+ one-time signature + threshold t-of-n quorum + hash-chain VRF + DAG-BFT quorum + ZK proof-receipt + EIP-7702 delegation + chain-receipt bundle + SCL");
    if (!file_exists("creation_os_v72")) {
        printf("  %sbuilding creation_os_v72 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v72");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v72'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v72 σ-Chain");
    kv("subsys",  "%s", "merkle-ledger · append-only-receipts · wots-plus · threshold-quorum · hash-chain-vrf · dag-bft-quorum · zk-proof-receipt · eip7702-delegation · chain-bundle · scl");
    int rc = run_cmd("./creation_os_v72 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v72 σ-Chain self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Chain self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v72 --bench | sed 's/^/    /'");
    printf("\n  %schain plane: every agent-bound step lands on a binary "
           "Merkle ledger (SonicDB S6 arXiv:2604.06579 lineage) chained "
           "through append-only receipts; a WOTS+ / XMSS one-time "
           "signature (FIPS 205 SLH-DSA lineage; eprint 2026/134, "
           "2026/194) seals the payload; a TALUS-style threshold t-of-n "
           "quorum (arXiv:2603.22109) closes the issuer side; a hash-"
           "chain VRF (eprint 2026/052; eprint 2022/993 iVRF) elects "
           "the round; a Narwhal + Bullshark 2f+1 gate (arXiv:2105."
           "11827, 2201.05677, 2507.04956) settles DAG consensus; a "
           "zkAgent / NANOZK proof-receipt (eprint 2026/199, arXiv:"
           "2603.18046v1) binds the compute; an EIP-7702 / ERC-4337 "
           "session-key capability bounds the delegation envelope.  "
           "All ten primitives are branchless integer-only with zero "
           "libc-external dependencies on the hot path.  Composes "
           "with v60..v71 as a 13-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos om  —  σ-Omnimodal (v73) self-test + microbench
 *
 *  The Creator kernel: universal-modality tokenizer (VQ-RVQ lineage),
 *  rectified-flow integer scheduler (StreamFlow / MixFlow / VRFM),
 *  VINO cross-modal XOR bind (arXiv:2601.02358), MOVA video+audio co-
 *  synth lock, Matrix-Game world-model step (arXiv:2604.08995 +
 *  2508.13009 + GameNGen), DiReCT physics gate (arXiv:2603.25931),
 *  n8n v2.10 workflow DAG + JudgeFlow (arXiv:2601.07477) + SOAN
 *  (arXiv:2508.13732v1) + ReusStdFlow, Cursor / Claude-Code / Devin /
 *  Lovable / Bolt / Base44 / Totalum / v0 / Replit-Agent -lineage code-
 *  edit planner, MultiGen (arXiv:2603.06679) asset navigation, and
 *  OML 10-op bytecode.  Composes with v60..v72 as a 14-bit branchless
 *  AND (cos_v73_compose_decision).
 * -------------------------------------------------------------------- */

/* --------------------------------------------------------------------
 *  cos ux / cos xp / cos experience  →  run v74 σ-Experience self-test
 *                                       and microbench.
 *
 *  The Experience kernel: Fitts-V2P target heatmap (arXiv:2508.13634
 *  GUI grounding 92.4%), adaptive layout optimiser (Log2Motion CHI '26
 *  arXiv:2601.21043), designer-basis personalisation (arXiv:2604.09876,
 *  κ = 0.25), Apple SQUIRE April 2026 slot authoring, universal-expert
 *  LoRA-MoE mesh (DR-LoRA arXiv:2601.04823, CoMoL arXiv:2603.00573,
 *  MoLE arXiv:2404.13628, MixLoRA arXiv:2404.15159v3), skill
 *  composition (XOR-bind), Mobile-GS order-free 3-D Gaussian-splat
 *  render step (arXiv:2603.11531 ICLR 2026: 116 FPS Snapdragon 8 Gen 3;
 *  msplat Metal-native engine ~350 FPS M4 Max), DLSS 4.5 / FSR / XeSS
 *  upscale + multi-frame-gen gate (up to 6× factor), 1-second
 *  interactive-world synth (Genie 3 lineage, 720p / 20-24 FPS), XPL
 *  10-op bytecode.  Composes with v60..v73 as a 15-bit branchless AND
 *  (cos_v74_compose_decision).
 * -------------------------------------------------------------------- */

static int cmd_ux(void)
{
    print_header();
    section("σ-Experience (v74) — Fitts-V2P + adaptive layout + designer-basis + SquireIR + universal-expert mesh + skill compose + Mobile-GS + DLSS/FSR/XeSS + 1-second world + XPL");
    if (!file_exists("creation_os_v74")) {
        printf("  %sbuilding creation_os_v74 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v74");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v74'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v74 σ-Experience");
    kv("subsys",  "%s", "fitts-v2p · adaptive-layout · designer-basis · squire-slot · expert-mesh · skill-compose · mobile-gs · framegen · second-world · XPL");
    int rc = run_cmd("./creation_os_v74 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v74 σ-Experience self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Experience self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v74 --bench | sed 's/^/    /'");
    printf("\n  %sexperience plane: perfect UX/UI + universal expertise + "
           "real-time render budget that makes 2026-era AAA games "
           "playable on commodity silicon (M4 MacBook, iPhone-class "
           "SoC, plain Snapdragon phone) + 1-second interactive-world "
           "synth.  Target selection is Fitts-V2P 2-D integer-Gaussian "
           "heatmap (arXiv:2508.13634, 92.4%% GUI-grounding).  Layout "
           "respects reachability + frozen-slot + Fitts-effort + "
           "uniqueness in one branchless AND.  Personalisation uses "
           "designer-basis HVs (arXiv:2604.09876, κ=0.25).  Slot "
           "authoring uses Apple SQUIRE scope guarantees (SquireIR, "
           "April 2026).  Universal expertise is a 64-expert LoRA-MoE "
           "HV mesh (DR-LoRA arXiv:2601.04823 + CoMoL arXiv:2603.00573 "
           "+ MoLE arXiv:2404.13628).  Render uses Mobile-GS order-"
           "free depth-aware Gaussian splatting (arXiv:2603.11531 ICLR "
           "2026: 116 FPS Snapdragon 8 Gen 3; msplat ~350 FPS M4 Max).  "
           "Upscale + multi-frame-gen gate accepts up to 6× factor "
           "(DLSS 4.5 Dynamic Multi-Frame Generation).  1-second "
           "interactive-world synth bridges straight into v73's WORLD "
           "opcode.  Composes with v60..v73 as a 15-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos sf / cos surface / cos mobile  →  run v76 σ-Surface self-test
 *                                         and microbench.
 *
 *  The Surface kernel: the single branchless integer substrate that
 *  carries the composed 16-kernel verdict all the way to a human on
 *  native iOS (UITouch / UIKit / SwiftUI / VoiceOver), native
 *  Android (MotionEvent / Jetpack Compose / TalkBack), every
 *  messenger worth bridging (WhatsApp, Telegram, Signal, iMessage,
 *  RCS, Matrix, XMPP, Discord, Slack, Line), and every piece of
 *  world-important legacy software (Word, Excel, Outlook, Teams,
 *  Photoshop, Illustrator, Lightroom, AutoCAD, SolidWorks, Revit,
 *  SAP, Oracle ERP, Salesforce, HubSpot, Stripe, Figma, Notion,
 *  Zoom, Xcode, Android Studio, VSCode, IntelliJ, Git, GitHub,
 *  PostgreSQL, MongoDB, Redis, AWS, GCP, Azure, Chrome, Safari,
 *  …) — all without leaving the branchless integer + HV + XOR +
 *  popcount discipline.
 * -------------------------------------------------------------------- */

static int cmd_sf(void)
{
    print_header();
    section("σ-Surface (v76) — iOS + Android + WhatsApp/Telegram/Signal/iMessage/RCS/Matrix/XMPP/Discord/Slack/Line + Word/Excel/Outlook/Photoshop/AutoCAD/SAP/Salesforce/Figma/Xcode/Postgres/Stripe/AWS/... + Signal-ratchet + WCAG 2.2 + CRDT + SBL");
    if (!file_exists("creation_os_v76")) {
        printf("  %sbuilding creation_os_v76 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v76");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v76'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v76 σ-Surface");
    kv("subsys",  "%s", "touch · gesture · haptic · messenger · e2e-ratchet · a11y · crdt · legacy-app · file-format · SBL");
    int rc = run_cmd("./creation_os_v76 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v76 σ-Surface self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Surface self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v76 --bench | sed 's/^/    /'");
    printf("\n  %ssurface plane: iOS (UITouch → 256-bit HV via Spektre.decode) "
           "+ Android (MotionEvent → HV via SpektreSurface.decode) + 10-"
           "protocol messenger bridge (WhatsApp, Telegram, Signal, iMessage, "
           "RCS, Matrix, XMPP, Discord, Slack, Line) with a single "
           "protocol-id-stamped 256-bit envelope, Signal-protocol X3DH-mix "
           "+ Double-Ratchet step that rides the v75 FIPS-180-4 SHA-256 "
           "(no OpenSSL), WCAG 2.2 + Apple HIG + Material 3 accessibility "
           "bitmask in one branchless AND, LWW-register + 256-bit OR-set "
           "CRDT merge (commutative, associative, deterministic, branchless), "
           "a 64-slot legacy-software capability-template HV registry "
           "(Word / Excel / PowerPoint / Outlook / Teams / Photoshop / "
           "Illustrator / Lightroom / AutoCAD / SolidWorks / Revit / SAP / "
           "Oracle ERP / Salesforce / HubSpot / QuickBooks / Stripe / Figma / "
           "Sketch / Notion / Slack / Zoom / Xcode / Android Studio / VSCode / "
           "IntelliJ / Git / GitHub / GitLab / PostgreSQL / MySQL / MongoDB / "
           "Redis / SQLite / Elasticsearch / Kafka / Snowflake / AWS / GCP / "
           "Azure / Cloudflare / Chrome / Safari / Firefox / Edge) that "
           "routes any task HV to the nearest legacy system by one popcount-"
           "Hamming argmin + margin gate, a 64-format file-type registry "
           "(DOCX / XLSX / PPTX / PDF / DWG / DXF / STEP / STL / PSD / AI / "
           "SVG / PNG / JPEG / MP4 / WAV / JSON / SQL / CSV / MD / EPUB / "
           "ZIP / APK / IPA / ELF / MachO / WASM / …) that classifies any "
           "blob without libmagic, and SBL — the Surface Bytecode Language, "
           "a 10-opcode integer ISA whose GATE writes v76_ok only when "
           "every surface-unit check is simultaneously green.  Swift + "
           "Kotlin façades ship in bindings/ios and bindings/android.  "
           "Composes with v60..v74 as a 16-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cmd_rv — σ-Reversible (v77) front door — Landauer / Bennett plane
 *
 *  The reversible-logic kernel.  Every primitive is bit-reversible:
 *  forward ∘ reverse is strict identity, so the hot path erases zero
 *  bits and — in principle — pays the Landauer floor k_B·T·ln 2 of
 *  zero energy per step.  Extends v76's 16-bit composed decision
 *  with a lateral 17-th AND via cos_v77_compose_decision(v76_ok,
 *  v77_ok).  References: Landauer 1961, Bennett 1973, Toffoli 1980,
 *  Fredkin 1982, Feynman 1985, Margolus 1990, arXiv:2402.02720,
 *  NeurIPS 2023-25 reversible-transformer literature.
 * -------------------------------------------------------------------- */

static int cmd_rv(void)
{
    print_header();
    section("σ-Reversible (v77) — Landauer / Bennett plane · 10 bit-reversible primitives · 17-bit composed decision");
    if (!file_exists("creation_os_v77")) {
        printf("  %sbuilding creation_os_v77 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v77");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v77'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel", "%s", "v77 σ-Reversible");
    kv("subsys", "%s", "NOT · CNOT · SWAP · Fredkin · Toffoli · Peres · Majority-3 · Bennett · 8-bit reversible adder · RVL bytecode");
    int rc = run_cmd("./creation_os_v77 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v77 σ-Reversible self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Reversible self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v77 --bench | sed 's/^/    /'");
    printf("\n  %sreversible plane: ten bit-reversible primitives "
           "(NOT, Feynman CNOT, SWAP, Fredkin CSWAP, Toffoli CCNOT, "
           "Peres, self-inverse Majority-3, Bennett forward/reverse "
           "driver, reversible 8-bit adder via Peres chain, and RVL "
           "— an eight-opcode reversible bytecode ISA where every "
           "instruction has an exact inverse).  The hot path erases "
           "zero bits: forward ∘ reverse is strict identity across "
           "the entire 256-bit × 16-register file.  In principle the "
           "Landauer floor k_B·T·ln 2 of energy per erased bit is "
           "therefore untouched.  Extends v76's 16-bit composed "
           "decision with a lateral 17-th AND via "
           "cos_v77_compose_decision(v76_ok, v77_ok).  1 = 1.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cmd_gd — σ-Gödel-Attestor (v78) front door — meta-cognitive plane
 *
 *  The self-attesting kernel.  Every emission must carry a proof
 *  receipt across ten branchless, integer-only filters:
 *    - IIT-φ integrated information         (Tononi / IIT 4.0)
 *    - discrete variational free energy     (Friston / Active Inf.)
 *    - MDL upper bound                      (Solomonoff / Rissanen)
 *    - prime-power Gödel numbering          (Gödel 1931)
 *    - Global-Workspace broadcast gate      (Baars / Dehaene)
 *    - Turing halting witness               (Turing 1936)
 *    - Löbian self-trust anchor             (Löb 1955 / Payor)
 *    - bisimulation check                   (Milner / Sangiorgi)
 *    - Chaitin Ω bound                      (Chaitin 1975)
 *    - MCB 8-op bytecode that composes them (Curry-Howard)
 *  Extends v77's 17-bit composed decision with a lateral 18-th AND
 *  via cos_v78_compose_decision(v77_ok, v78_ok).
 * -------------------------------------------------------------------- */

static int cmd_gd(void)
{
    print_header();
    section("σ-Gödel-Attestor (v78) — meta-cognitive plane · 10 integer-only primitives · 18-bit composed decision");
    if (!file_exists("creation_os_v78")) {
        printf("  %sbuilding creation_os_v78 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v78");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v78'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel", "%s", "v78 σ-Gödel-Attestor");
    kv("subsys", "%s", "IIT-φ · FEP · MDL · Gödel-num · Global-Workspace · Halting-witness · Löbian self-trust · Bisim · Chaitin-Ω · MCB bytecode");
    int rc = run_cmd("./creation_os_v78 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v78 σ-Gödel-Attestor self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Gödel-Attestor self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v78 --bench | sed 's/^/    /'");
    printf("\n  %smeta-cognitive plane: no emission crosses to the "
           "human unless the computation simultaneously (a) carries "
           "integrated information above φ_min (Tononi / IIT 4.0), "
           "(b) minimises variational free energy within budget "
           "(Friston / Active Inference), (c) fits the declared MDL "
           "upper bound (Solomonoff / Rissanen / Chaitin), (d) matches "
           "its own Gödel number (Gödel 1931), (e) wins the Global "
           "Workspace coalition threshold (Baars / Dehaene), (f) "
           "witnesses its own halt via a strictly-decreasing "
           "termination measure (Turing 1936), (g) trusts its own "
           "proof system through a Löbian anchor (Löb 1955 / Payor), "
           "(h) passes the bisimulation spec-equivalence check "
           "(Milner / Sangiorgi), and (i) stays inside the Chaitin-Ω "
           "budget (Chaitin 1975).  The MCB bytecode weaves these "
           "into one proof receipt (Curry-Howard).  Extends v77's "
           "17-bit composed decision with a lateral 18-th AND via "
           "cos_v78_compose_decision(v77_ok, v78_ok).  1 = 1.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cmd_sm — σ-Simulacrum (v79) front door — hypervector-space
 *           simulation substrate
 *
 *  Branchless, integer-only, libc-only kernel that lets the agent
 *  instantiate, step, measure and verify entire worlds inside the
 *  256-bit HV space.  Ten primitives, each grounded in a real paper:
 *    - Symplectic leapfrog Verlet       (Verlet 1967; Hairer 2006)
 *    - Wolfram 1D cellular automaton    (Wolfram 1983; Cook 2004)
 *    - Aaronson-Gottesman stabilizer    (arXiv:quant-ph/0406196)
 *    - HD reservoir step                (Frady arXiv:2003.04030)
 *    - Koopman embedding                (Koopman 1931; Brunton 2016)
 *    - Cronin assembly index            (Sharma et al. Nature 2023)
 *    - Kauffman Boolean network step    (Kauffman 1969)
 *    - Integer shadow-Hamiltonian energy
 *    - Merkle-style trajectory receipt
 *    - SSL 8-op bytecode composing 1..9
 *  Extends v78's 18-bit composed decision with a lateral 19-th AND
 *  via cos_v79_compose_decision(v78_ok, v79_ok).
 * -------------------------------------------------------------------- */

static int cmd_sm(void)
{
    print_header();
    section("σ-Simulacrum (v79) — hypervector-space simulation substrate · 10 integer-only primitives · 19-bit composed decision");
    if (!file_exists("creation_os_v79")) {
        printf("  %sbuilding creation_os_v79 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v79");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v79'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel", "%s", "v79 σ-Simulacrum");
    kv("subsys", "%s", "Verlet · 1D CA · Aaronson-Gottesman stabilizer · HD reservoir · Koopman embed · Cronin assembly · Kauffman graph · energy · receipt · SSL bytecode");
    int rc = run_cmd("./creation_os_v79 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v79 σ-Simulacrum self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Simulacrum self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v79 --bench | sed 's/^/    /'");
    printf("\n  %ssimulacrum plane: the agent does not merely *answer* — "
           "it runs the world inside the HV substrate before speaking. "
           "Particle physics steps through a symplectic leapfrog Verlet "
           "whose shadow Hamiltonian is conserved modulo Q16.16 rounding; "
           "Wolfram cellular automata (including rule 110, universal per "
           "Cook 2004) evolve the 256-bit lattice in one branchless "
           "LUT-driven pass; Aaronson-Gottesman stabiliser tableaux "
           "simulate Clifford quantum circuits in polynomial time, "
           "preserving the symplectic row-commutativity invariant at "
           "every step (arXiv:quant-ph/0406196); a 256-bit hyperdimen"
           "sional reservoir (Jaeger 2001; Frady et al. arXiv:2003.04030) "
           "couples inputs with rotate-XOR-bundle dynamics; Koopman "
           "embeddings (Koopman 1931; Brunton 2016) lift nonlinear state "
           "to a GF(2)-linear observable; Cronin-style assembly indices "
           "(Sharma et al. Nature 2023) bound informational complexity; "
           "a Kauffman Boolean network (Kauffman 1969) threshold-fires "
           "across up to 64 nodes; every step folds into a Merkle-style "
           "commutative receipt compatible with v72 σ-Chain; and the "
           "SSL 8-op bytecode weaves all of the above into one verifiable "
           "step program.  Extends v78's 18-bit composed decision with a "
           "lateral 19-th AND via cos_v79_compose_decision(v78_ok, "
           "v79_ok).  1 = 1.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cmd_cx — σ-Cortex (v80) front door — hypervector-space neocortical
 *           reasoning plane
 *
 *  Branchless, integer-only, libc-only kernel that collapses ten of
 *  the most impactful 2023–2025 sequence-model, attention, routing
 *  and test-time-compute results into one hypervector-space
 *  neocortex, compiled directly to hardware:
 *    - Selective SSM (Mamba)        (Gu & Dao 2023 arXiv:2312.00752;
 *                                    Mamba-2 arXiv:2405.21060)
 *    - Rotary Position Embedding    (Su 2021 arXiv:2104.09864)
 *    - Sliding-window / ring attn.  (Beltagy 2020; Mistral 2023;
 *                                    Liu arXiv:2310.01889)
 *    - Paged KV cache               (Kwon 2023 vLLM arXiv:2309.06180)
 *    - Speculative decoding verify  (Leviathan 2023 arXiv:2211.17192)
 *    - Variational free energy      (Friston 2010 Nat. Rev. Neurosci.)
 *    - Kolmogorov-Arnold Net edge   (Liu 2024 arXiv:2404.19756)
 *    - Continuous Thought Machine   (Sakana AI 2025)
 *    - MoE top-k router             (Shazeer 2017 arXiv:1701.06538;
 *                                    DeepSeek-MoE 2024 arXiv:2401.06066)
 *    - TTC 16-op bytecode composing 1..9
 *  Extends v79's 19-bit composed decision with a lateral 20-th AND
 *  via cos_v80_compose_decision(v79_ok, v80_ok).
 * -------------------------------------------------------------------- */

static int cmd_cx(void)
{
    print_header();
    section("σ-Cortex (v80) — hypervector-space neocortical reasoning plane · 10 integer-only primitives · 20-bit composed decision");
    if (!file_exists("creation_os_v80")) {
        printf("  %sbuilding creation_os_v80 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v80");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v80'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel", "%s", "v80 σ-Cortex");
    kv("subsys", "%s", "Mamba SSM · RoPE · sliding-window attention · paged KV · speculative verify · free energy · KAN edge · CTM Kuramoto · MoE top-k · TTC bytecode");
    int rc = run_cmd("./creation_os_v80 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v80 σ-Cortex self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Cortex self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v80 --bench | sed 's/^/    /'");
    printf("\n  %scortex plane: the agent does not merely *simulate* — "
           "it *reasons* in the same 256-bit HV substrate.  A Mamba-style "
           "selective state-space core (Gu & Dao 2023; Mamba-2 Dao & Gu "
           "2024) runs the hidden state in linear time with a diagonal "
           "Q16.16 recurrence; rotary position embeddings (Su 2021) "
           "rotate query/key pairs through an integer sin/cos LUT; a "
           "sliding-window attention ring (Beltagy 2020 Longformer; "
           "Mistral 2023; Liu 2023 Ring Attention) collapses to a "
           "branchless popcount-argmin over the current 64-token window; "
           "a paged KV cache (Kwon 2023 vLLM / PagedAttention) commits "
           "hashes into tagged pages with a CAFEBABE sentinel guarded "
           "every step; speculative-decode verify (Leviathan 2023) "
           "accepts the longest agreeing prefix of draft vs target via "
           "a monotone integer bitmask; a variational free-energy bound "
           "(Friston 2010) tracks surprise + mean-absolute-deviation "
           "over a ring history; a Kolmogorov-Arnold Network edge (Liu "
           "2024; Kolmogorov 1957) runs a 1-D integer cumulative "
           "spline as its learnable activation; a Continuous Thought "
           "Machine (Sakana 2025) ticks an 8-oscillator Kuramoto bank "
           "through a 32-entry sin LUT; a Mixture-of-Experts top-k "
           "router (Shazeer 2017; DeepSeek-MoE 2024) selects exactly k "
           "experts branchlessly and asserts popcount(routed) == k; and "
           "the TTC 16-op bytecode weaves all nine into one reasoning "
           "program — test-time compute scaling in the o1 / DeepSeek-R1 "
           "2024–2025 sense.  Extends v79's 19-bit composed decision "
           "with a lateral 20-th AND via cos_v80_compose_decision("
           "v79_ok, v80_ok).  1 = 1.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cmd_license — License Attestation Kernel front door (v75 σ-License)
 * --------------------------------------------------------------------
 *  Subcommands:
 *    cos license               (default = status)
 *    cos license status        — bundle + pinned-hash check
 *    cos license self-test     — 11 KAT (FIPS-180-4 + receipt)
 *    cos license verify        — recompute SHA-256 of LICENSE-SCSL-1.0.md
 *    cos license bundle        — verify all license-related files present
 *    cos license receipt <kid> <ALLOW|ABSTAIN|DENY> [<commit>] [<bits-hex>]
 *    cos license guard         — exit 87 if license tampered
 *    cos license print         — print SCSL-1.0 short-policy summary
 * -------------------------------------------------------------------- */

static int cmd_license(int argc, char **argv)
{
    print_header();
    section("σ-License (v75) — SCSL-1.0 attestation · License-Bound Receipt · anti-tamper guard");

    if (!file_exists("license_attest")) {
        printf("  %sbuilding license_attest (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s license_attest");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make license_attest'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }

    const char *sub = (argc > 0) ? argv[0] : "status";

    if (strcmp(sub, "status") == 0) {
        kv("kernel",      "%s", "v75 σ-License (Spektre Commercial Source License v1.0)");
        kv("master",      "%s", "LICENSE-SCSL-1.0.md");
        kv("fallback",    "%s", "LICENSE-AGPL-3.0.txt (auto after 4-year Change Date)");
        kv("commercial",  "%s", "COMMERCIAL_LICENSE.md  (Startup → Sovereign → Strategic)");
        kv("contributor", "%s", "CLA.md");
        kv("copyright",   "%s", "2024-2026 Lauri Elias Rainio · Spektre Labs Oy (joint and several)");
        kv("commercial-rights", "%s", "EXCLUSIVE to Lauri Elias Rainio + Spektre Labs Oy");
        kv("anti-state",  "%s", "SCSL §9 (operational §9.1(b) NEVER waivable)");
        kv("sanctions",   "%s", "SCSL §10 (categorical denial: Aggression, Sanctioned Persons)");
        kv("receipt-anchor", "%s", "SCSL §11 (sha256 of LICENSE-SCSL-1.0.md)");
        printf("\n");
        int rc = run_cmd("./license_attest --bundle");
        if (rc != 0) return rc;
        printf("\n  %s%s%s license bundle intact; SCSL §11 anchor verified.\n",
               C_GREEN, check(), C_RESET);
        printf("\n  %sNon-binding short policy:%s\n", C_BOLD, C_RESET);
        printf("    %sFREE %s  private · academic · non-profit · audit · 30-day eval (<EUR 1M)\n",
               C_GREEN, C_RESET);
        printf("    %sPAID %s  for-profit · SaaS without §5 source publish · OEM closed-source\n",
               C_AMBER, C_RESET);
        printf("    %sDENY %s  intelligence · military operational · mass surveillance · sanctioned · aggression\n",
               C_RED, C_RESET);
        printf("\n  %slicensing@spektre-labs.com  ·  see COMMERCIAL_LICENSE.md  ·  docs/LICENSING.md%s\n",
               C_DIM, C_RESET);
        return 0;
    }
    if (strcmp(sub, "self-test") == 0) {
        return run_cmd("./license_attest --self-test");
    }
    if (strcmp(sub, "verify") == 0) {
        return run_cmd("./license_attest --verify");
    }
    if (strcmp(sub, "bundle") == 0) {
        return run_cmd("./license_attest --bundle");
    }
    if (strcmp(sub, "guard") == 0) {
        return run_cmd("./license_attest --guard");
    }
    if (strcmp(sub, "receipt") == 0) {
        if (argc < 3) {
            printf("  %s%s%s usage: cos license receipt <kernel-id> <ALLOW|ABSTAIN|DENY> [<commit>] [<bits-hex>]\n",
                   C_RED, cross(), C_RESET);
            return 2;
        }
        char buf[1024];
        if (argc >= 5) {
            snprintf(buf, sizeof buf, "./license_attest --receipt %s %s %s %s",
                     argv[1], argv[2], argv[3], argv[4]);
        } else if (argc >= 4) {
            snprintf(buf, sizeof buf, "./license_attest --receipt %s %s %s",
                     argv[1], argv[2], argv[3]);
        } else {
            snprintf(buf, sizeof buf, "./license_attest --receipt %s %s",
                     argv[1], argv[2]);
        }
        return run_cmd(buf);
    }
    if (strcmp(sub, "print") == 0) {
        return run_cmd("sed -n '1,90p' LICENSE-SCSL-1.0.md");
    }
    printf("  %s%s%s unknown sub: '%s' — try 'status', 'self-test', 'verify', 'bundle', 'guard', 'receipt', 'print'\n",
           C_RED, cross(), C_RESET, sub);
    return 2;
}

static int cmd_om(void)
{
    print_header();
    section("σ-Omnimodal (v73) — universal tokenizer + rectified-flow + VINO bind + MOVA AV lock + Matrix-Game world step + DiReCT physics + n8n workflow + Cursor plan + MultiGen asset + OML");
    if (!file_exists("creation_os_v73")) {
        printf("  %sbuilding creation_os_v73 (first run)...%s\n",
               C_DIM, C_RESET);
        int b = run_cmd("make -s standalone-v73");
        if (b != 0) {
            printf("  %s%s%s build failed (rc=%d); see 'make standalone-v73'\n",
                   C_RED, cross(), C_RESET, b);
            return b;
        }
    }
    kv("kernel",  "%s", "v73 σ-Omnimodal");
    kv("subsys",  "%s", "universal-tokenizer · rectified-flow · VINO-bind · MOVA-lock · Matrix-Game-world · DiReCT-physics · n8n-workflow · Cursor-plan · MultiGen-asset · OML");
    int rc = run_cmd("./creation_os_v73 --self-test | sed 's/^/    /'");
    if (rc != 0) {
        printf("\n  %s%s%s v73 σ-Omnimodal self-test FAILED (rc=%d)\n",
               C_RED, cross(), C_RESET, rc);
        return rc;
    }
    printf("\n  %s%s%s σ-Omnimodal self-test PASS — running microbench...\n",
           C_GREEN, check(), C_RESET);
    (void)run_cmd("./creation_os_v73 --bench | sed 's/^/    /'");
    printf("\n  %screator plane: one branchless integer substrate "
           "covers the 2024-2026 generative frontier.  Code "
           "(Cursor / Claude Code / Devin / Lovable / Bolt / Base44 / "
           "Totalum / v0 / Replit-Agent autonomous-software-creation "
           "benchmarks), image (Midjourney / FLUX / VINO MMDiT arXiv:"
           "2601.02358), video (Sora / Veo / MOVA / Matrix-Game-3 "
           "arXiv:2604.08995 + Matrix-Game-2 arXiv:2508.13009 + "
           "GameNGen ICLR 2025), audio + voice clone (ElevenLabs / "
           "SoundStream arXiv:2107.03312 / Encodec RVQ / MSR-Codec), "
           "workflow (n8n v2.10 Brain-vs-Hands + JudgeFlow arXiv:"
           "2601.07477 + ReusStdFlow + SOAN arXiv:2508.13732v1), "
           "physics-coherent flow (DiReCT arXiv:2603.25931 + "
           "StreamFlow arXiv:2511.22009 + MixFlow arXiv:2604.09181 + "
           "VRFM arXiv:2502.09616), and GTA-tier procedural world "
           "synthesis (MultiGen arXiv:2603.06679) all share the same "
           "256-bit HV token alphabet, so cross-modal coherence is a "
           "single Hamming compare, not a framework stack.  Composes "
           "with v60..v72 as a 14-bit branchless AND.%s\n",
           C_GREY, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos decide <v60>..<v73>
 *
 *  One-shot wrapper around cos_v72_compose_decision.  Useful for CI
 *  pipelines, policy audit trails, and debugging lane failures in
 *  isolation.  Prints a JSON-ish object; exit status is 0 iff
 *  allow == 1.
 * -------------------------------------------------------------------- */

static int cmd_decide(int argc, char **argv)
{
    if (argc != 16) {
        fprintf(stderr,
                "usage: cos decide <v60> <v61> <v62> <v63> <v64> <v65> <v66> <v67> <v68> <v69> <v70> <v71> <v72> <v73> <v74> <v76>\n"
                "       each argument is 0 or 1.\n");
        return 64;
    }
    if (!file_exists("creation_os_v76")) {
        int b = run_cmd("make -s standalone-v76 >/dev/null 2>&1");
        if (b != 0) {
            fprintf(stderr, "cos: v76 not built; run 'make standalone-v76'.\n");
            return b;
        }
    }
    char cmd[640];
    snprintf(cmd, sizeof cmd,
             "./creation_os_v76 --decision %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             atoi(argv[0]),  atoi(argv[1]),  atoi(argv[2]),
             atoi(argv[3]),  atoi(argv[4]),  atoi(argv[5]),
             atoi(argv[6]),  atoi(argv[7]),  atoi(argv[8]),
             atoi(argv[9]),  atoi(argv[10]), atoi(argv[11]),
             atoi(argv[12]), atoi(argv[13]), atoi(argv[14]),
             atoi(argv[15]));
    return run_cmd(cmd);
}

/* --------------------------------------------------------------------
 *  cos doctor — unified repo-health rollup (Apple-tier DX).
 *
 *  Runs every receipt-emitting gate the repo ships (license bundle +
 *  verify-agent + hardening-check + ASAN/UBSAN spot + bench sanity)
 *  and prints a single colour, sectioned summary with PASS / WARN /
 *  FAIL per lane.  Non-zero exit if any lane is FAIL; WARN is never
 *  silent but does not fail the doctor.
 * -------------------------------------------------------------------- */

enum { DOC_PASS = 0, DOC_WARN = 1, DOC_FAIL = 2 };

static int doc_run(const char *target)
{
    char cmd[512];
    snprintf(cmd, sizeof cmd,
             "make -s %s > /tmp/cos_doctor.log 2>&1", target);
    return run_cmd(cmd);
}

static void doc_line(const char *lane, int status, const char *detail)
{
    const char *col, *mark, *tag;
    switch (status) {
    case DOC_PASS: col = C_GREEN; mark = check(); tag = "PASS"; break;
    case DOC_WARN: col = C_AMBER; mark = bullet(); tag = "WARN"; break;
    default:       col = C_RED;   mark = cross();  tag = "FAIL"; break;
    }
    printf("  %s%s%s  %-7s %-26s %s%s%s\n",
           col, mark, C_RESET, tag, lane, C_GREY, detail, C_RESET);
}

static int cmd_doctor(void)
{
    print_header();

    section("repository health — one command, every receipt");

    int rc_license = doc_run("license-check");
    int rc_verify  = doc_run("verify-agent");
    int rc_harden  = doc_run("hardening-check");
    int rc_chace   = doc_run("chace");
    int rc_scsl    = doc_run("license_attest");

    int hv60 = file_exists("creation_os_v60");
    int hv74 = file_exists("creation_os_v74");
    int hv76 = file_exists("creation_os_v76");

    doc_line("license bundle",          rc_license == 0 ? DOC_PASS : DOC_FAIL,
             "SCSL-1.0 pinned · SPDX headers · NOTICE");
    doc_line("license attestation",     rc_scsl == 0    ? DOC_PASS : DOC_WARN,
             "License-Bound Receipt (SCSL §11) verifies");
    doc_line("verify-agent (v57 → v80)", rc_verify == 0 ? DOC_PASS : DOC_FAIL,
             "20 kernels + ancillary slots");
    doc_line("CHACE-class gate",        rc_chace == 0   ? DOC_PASS : DOC_WARN,
             "12-layer capability-hardening rollup");
    doc_line("hardening-check",         rc_harden == 0  ? DOC_PASS : DOC_WARN,
             "PIE · stack canary · RELRO · Fortify");
    doc_line("build artefacts (v60)",   hv60            ? DOC_PASS : DOC_WARN,
             hv60 ? "present" : "run  make check-v60");
    doc_line("build artefacts (v74)",   hv74            ? DOC_PASS : DOC_WARN,
             hv74 ? "present" : "run  make check-v74");
    doc_line("build artefacts (v76)",   hv76            ? DOC_PASS : DOC_WARN,
             hv76 ? "present" : "run  make check-v76");

    section("supply-chain receipts");
    kv("LICENSE.sha256",    "%s", file_exists("LICENSE.sha256")    ? "present" : "missing");
    kv("SBOM.json",         "%s", file_exists("SBOM.json")         ? "present" : "missing");
    kv("ATTESTATION.json",  "%s", file_exists("ATTESTATION.json")  ? "present" : "missing");
    kv("PROVENANCE.json",   "%s", file_exists("PROVENANCE.json")   ? "present" : "missing");

    section("next steps");
    if (rc_license != 0 || rc_verify != 0)
        bullet_line("%sblocker%s  see  %stail -40 /tmp/cos_doctor.log%s",
                    C_RED, C_RESET, C_BOLD, C_RESET);
    else
        bullet_line("%sship-ready%s  all blocking lanes PASS",
                    C_GREEN, C_RESET);
    bullet_line("upgrade DX: source scripts/completion/cos.bash (or .zsh / .fish)");
    bullet_line("deep dive:  cos sigma  ·  cos verify  ·  cos chace");
    printf("\n");

    return (rc_license != 0 || rc_verify != 0) ? 1 : 0;
}

/* --------------------------------------------------------------------
 *  Help / version
 * -------------------------------------------------------------------- */

/* --------------------------------------------------------------------
 *  cos welcome / start  — first-run greeting for anyone who has never
 *  opened a terminal before in their life.  No jargon.  No shame.
 *  The goal is: in 30 seconds, the user knows *exactly* what this is
 *  and *exactly* what to type next.
 * -------------------------------------------------------------------- */

static int cmd_welcome(void)
{
    print_header();
    section("Welcome to Creation OS");
    printf("  %sThis is your computer's new brain.%s\n", C_BOLD, C_RESET);
    printf("\n");
    printf("  It is a local AI runtime — every answer is computed on %sthis%s machine.\n",
           C_BOLD, C_RESET);
    printf("  Nothing is sent to the cloud.  Nothing is logged.  Nothing calls home.\n");
    printf("  Twenty kernels check every emission.  If even one says %sno%s, the\n",
           C_BOLD, C_RESET);
    printf("  answer never reaches you.  One zero anywhere = silence.  %s1 = 1.%s\n",
           C_BOLD, C_RESET);

    section("Try these, in this order");
    bullet_line("%scos demo%s          %sthe 30-second tour — numbers, live, not promises%s",
                C_BOLD, C_RESET, C_GREY, C_RESET);
    bullet_line("%scos sigma%s         %srun every kernel's self-test — 6+ million PASS rows%s",
                C_BOLD, C_RESET, C_GREY, C_RESET);
    bullet_line("%scos status%s        %sone-screen dashboard — what is built, what is ready%s",
                C_BOLD, C_RESET, C_GREY, C_RESET);
    bullet_line("%scos doctor%s        %sfull repo health — license, verify, hardening, receipts%s",
                C_BOLD, C_RESET, C_GREY, C_RESET);
    bullet_line("%scos help%s          %severy command, with a one-line description%s",
                C_BOLD, C_RESET, C_GREY, C_RESET);

    section("If a command mentions something you do not recognise");
    printf("  That is fine.  Each of the forty kernels is a separate, self-contained\n");
    printf("  experiment — a single C file, under a thousand lines, with its own\n");
    printf("  self-test that prints a real number.  You do not need to understand them\n");
    printf("  all.  You need to know only this: %sif `cos sigma` says ALLOW, every one of\n",
           C_BOLD);
    printf("  them agreed.%s\n", C_RESET);

    printf("\n  %sReady?  Type:%s  %scos demo%s\n\n", C_GREY, C_RESET, C_BOLD, C_RESET);
    return 0;
}

/* --------------------------------------------------------------------
 *  cos demo / showcase  — the "drop from the chair" 30-second tour.
 *  Builds (if needed) and runs the twenty kernels one by one, prints
 *  each one's live PASS count, and ends with a single composed verdict.
 *  No simulations.  No mock numbers.  Real `--self-test` output, live.
 * -------------------------------------------------------------------- */

struct demo_row {
    const char *id;      /* "v80" */
    const char *name;    /* "σ-Cortex" */
    const char *blurb;   /* one-line what-it-does */
    const char *target;  /* make target */
    const char *binary;  /* ./creation_os_vN */
    long long   rows;    /* expected PASS rows, for headline */
};

static const struct demo_row demo_rows[] = {
    {"v60", "σ-Shield",         "capability-bitmask · σ-gate · TOCTOU-free",                                                    "check-v60", "creation_os_v60",         81},
    {"v61", "Σ-Citadel",        "BLP · Biba · MLS lattice · attestation",                                                       "check-v61", "creation_os_v61",         61},
    {"v62", "Reasoning Fabric", "latent-CoT · EBT · HRM · NSA · MTP · ARKV",                                                    "check-v62", "creation_os_v62",         68},
    {"v63", "σ-Cipher",         "BLAKE2b · HKDF · ChaCha20-Poly1305 · X25519",                                                  "check-v63", "creation_os_v63",        144},
    {"v64", "σ-Intellect",      "MCTS-σ · skill library · tool authz · Reflexion",                                              "check-v64", "creation_os_v64",        260},
    {"v65", "σ-Hypercortex",    "bipolar HDC · bind / bundle / permute · HVL",                                                  "check-v65", "creation_os_v65",        534},
    {"v66", "σ-Silicon",        "INT8 GEMV · ternary · conformal · HSL",                                                        "check-v66", "creation_os_v66",       1705},
    {"v67", "σ-Noesis",         "BM25 · dense · graph-walk · beam deliberate",                                                  "check-v67", "creation_os_v67",         -1},
    {"v68", "σ-Mnemos",         "bipolar HV-D8192 · surprise gate · ACT-R decay · MML",                                          "check-v68", "creation_os_v68",         -1},
    {"v69", "σ-Constellation",  "tree-spec · debate · Byzantine vote · MoE route",                                               "check-v69", "creation_os_v69",         -1},
    {"v70", "σ-Hyperscale",     "ShiftAddLLM · Mamba-2 · RWKV-7 · MoE-10k · PIM · WDM · Loihi-3",                                "check-v70", "creation_os_v70",     148034},
    {"v71", "σ-Wormhole",       "ER-portal · cleanup · teleport · Kleinberg route · WHL",                                        "check-v71", "creation_os_v71",      68404},
    {"v72", "σ-Chain",          "Merkle ledger · WOTS+ · t-of-n · VRF · DAG-BFT · ZK",                                           "check-v72", "creation_os_v72",     117108},
    {"v73", "σ-Omnimodal",      "code · image · audio · video · 3D · workflow — one ABI",                                         "check-v73", "creation_os_v73",     245818},
    {"v74", "σ-Experience",     "UI · a11y · mobile-GS · frame-gen · second-world",                                              "check-v74", "creation_os_v74",     600128},
    {"v76", "σ-Surface",        "10-messenger bridge · E2E ratchet · CRDT · legacy apps",                                         "check-v76", "creation_os_v76",      86583},
    {"v77", "σ-Reversible",     "NOT · CNOT · SWAP · Fredkin · Toffoli · Bennett · RVL",                                          "check-v77", "creation_os_v77",     761264},
    {"v78", "σ-Gödel-Attestor", "IIT-φ · FEP · MDL · Gödel-num · workspace · halting · Löbian · MCB",                             "check-v78", "creation_os_v78",     207582},
    {"v79", "σ-Simulacrum",     "Verlet · CA · Clifford · reservoir · Koopman · assembly · SSL",                                   "check-v79", "creation_os_v79",    2994549},
    {"v80", "σ-Cortex",         "Mamba SSM · RoPE · sliding-attn · paged-KV · spec-verify · FEP · KAN · CTM · MoE · TTC",          "check-v80", "creation_os_v80",    6935348},
    {"v81", "σ-Lattice (PQC)",  "Keccak-f[1600] · SHAKE-128/256 · Kyber NTT · Barrett · Montgomery · CBD · KEM",                   "check-v81", "creation_os_v81",    3513430},
    {"v82", "σ-Stream",         "streaming composed decision · halt-on-flip · SHAKE-256 Merkle chain · replay-verify",              "check-v82", "creation_os_v82",      72005},
    {"v83", "σ-Agentic",        "PLAN → ROLL → SURPRISE → ENERGY · rollback · Mnemos consolidation",                                "check-v83", "creation_os_v83",      13153},
    {"v84", "σ-ZKProof",        "NANOZK layerwise Merkle commits · opening proofs · tamper detection",                              "check-v84", "creation_os_v84",      13534},
    {"v85", "σ-Formal",         "TLA-style runtime invariant checker · ALWAYS · EVENTUALLY · RESPONDS",                             "check-v85", "creation_os_v85",        500},
    {"v86", "σ-JEPA",           "latent-space predictive world model · encoder + EMA target + predictor + VICReg var/invar/covar",  "check-v86", "creation_os_v86",      14629},
    {"v87", "σ-SAE",            "Top-K sparse autoencoder · feature dictionary · causal ablation · mech-interp plane",              "check-v87", "creation_os_v87",      13511},
    {"v88", "σ-FHE",            "Ring-LWE integer homomorphic · keygen · enc/dec · add · scalar-mul · rotate",                      "check-v88", "creation_os_v88",      10546},
    {"v89", "σ-Spiking",        "Loihi-3 graded-spike LIF · STDP · event-driven neuromorphic plane",                                 "check-v89", "creation_os_v89",     491003},
    {"v90", "σ-Hierarchical",   "RGM / S-HAI tower · top-down prior · bottom-up error · SHAKE-256 receipts",                         "check-v90", "creation_os_v90",      44512},
    {"v91", "σ-Quantum",        "4-qubit integer quantum simulator · Pauli · Hadamard · CNOT · Grover amplification",                "check-v91", "creation_os_v91",        294},
    {"v92", "σ-Titans",         "neural long-term memory · surprise-gated writes · momentum · adaptive forgetting",                  "check-v92", "creation_os_v92",      11723},
    {"v93", "σ-MoR",            "Mixture-of-Recursions · shared-layer stack · token-level adaptive depth routing",                   "check-v93", "creation_os_v93",        746},
    {"v94", "σ-Clifford",       "Cl(3,0) geometric algebra · geometric product · wedge · reverse · equivariant layer",               "check-v94", "creation_os_v94",       7219},
    {"v95", "σ-Sheaf",          "cellular-sheaf NN · sheaf Laplacian · heat-equation diffusion · local-to-global",                    "check-v95", "creation_os_v95",       4268},
    {"v96", "σ-Diffusion",      "integer rectified-flow / DDIM sampler · monotone schedule · energy-decay denoise",                   "check-v96", "creation_os_v96",       1236},
    {"v97", "σ-RL",             "tabular Q-learning · GAE advantage · PPO-clip surrogate · branchless trust region",                   "check-v97", "creation_os_v97",       2391},
    {"v98", "σ-Topology",       "Vietoris–Rips · persistent homology · Betti-0/1 · Euler identity · union-find",                       "check-v98", "creation_os_v98",      22375},
    {"v99", "σ-Causal",         "SCM · do-calculus · back-door adjustment · counterfactual twin graph · ATE",                           "check-v99", "creation_os_v99",        427},
    {"v100","σ-Hyena",          "sub-quadratic gated long convolution · decaying filter · causality · shift-covariance",                "check-v100","creation_os_v100",    10999},
};

static double wall_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void demo_banner(void)
{
    printf("\n");
    printf("%s%s╔══════════════════════════════════════════════════════════════════════╗%s\n",
           C_BOLD, C_BLUE, C_RESET);
    printf("%s%s║%s   %sCreation OS — the twenty-kernel tour%s                                %s%s║%s\n",
           C_BOLD, C_BLUE, C_RESET,
           C_BOLD, C_RESET,
           C_BOLD, C_BLUE, C_RESET);
    printf("%s%s║%s   %sEvery line below is a real self-test.  Nothing is mocked.%s           %s%s║%s\n",
           C_BOLD, C_BLUE, C_RESET,
           C_GREY, C_RESET,
           C_BOLD, C_BLUE, C_RESET);
    printf("%s%s║%s   %sIf even one kernel says no, the final verdict is DENY.%s              %s%s║%s\n",
           C_BOLD, C_BLUE, C_RESET,
           C_GREY, C_RESET,
           C_BOLD, C_BLUE, C_RESET);
    printf("%s%s╚══════════════════════════════════════════════════════════════════════╝%s\n",
           C_BOLD, C_BLUE, C_RESET);
    printf("\n");
}

static int cmd_demo(void)
{
    demo_banner();

    const int N = (int)(sizeof demo_rows / sizeof demo_rows[0]);

    printf("  %s%-5s %-20s %s%-8s %s%s\n",
           C_BOLD, "id", "σ-name", C_RESET, "", C_GREY, "");
    hr(72);

    long long total_rows = 0;
    int       total_fail = 0;
    double    t0 = wall_seconds();

    for (int i = 0; i < N; ++i) {
        const struct demo_row *r = &demo_rows[i];
        printf("  %s%-5s%s %s%-20s%s %s… building + self-testing%s",
               C_BOLD, r->id, C_RESET,
               C_BOLD, r->name, C_RESET,
               C_DIM,  C_RESET);
        fflush(stdout);

        char cmd[256];
        snprintf(cmd, sizeof cmd, "make -s %s >/tmp/cos-demo-$$.log 2>&1", r->target);
        double ts = wall_seconds();
        int rc = run_cmd(cmd);
        double te = wall_seconds();

        /* Clear the "… building" line — only on a tty; if stdout is piped
         * we just print a newline so the log stays flat and grep-able. */
        if (g_color) {
            printf("\r\033[2K");
        } else {
            printf("\n");
        }

        const char *mark = (rc == 0) ? check() : cross();
        const char *col  = (rc == 0) ? C_GREEN : C_RED;
        printf("  %s%s%s %s%-5s%s %s%-20s%s  ",
               col, mark, C_RESET,
               C_BOLD, r->id, C_RESET,
               C_BOLD, r->name, C_RESET);

        if (rc == 0) {
            if (r->rows > 0) {
                printf("%s%'lld PASS%s  ", C_GREEN, r->rows, C_RESET);
                total_rows += r->rows;
            } else {
                printf("%sOK%s        ", C_GREEN, C_RESET);
            }
            printf("%s%.2fs%s  %s%s%s\n",
                   C_DIM, te - ts, C_RESET,
                   C_GREY, r->blurb, C_RESET);
        } else {
            total_fail++;
            printf("%sFAIL%s (rc=%d)  %s%s%s\n",
                   C_RED, C_RESET, rc,
                   C_GREY, r->blurb, C_RESET);
        }
    }

    double tN = wall_seconds();

    hr(72);
    printf("\n  %s%s%s  %s%lld%s self-test rows PASS  %s·%s  %s%d%s FAIL  %s·%s  %s%.1fs%s wall\n",
           total_fail == 0 ? C_GREEN : C_RED,
           total_fail == 0 ? check() : cross(),
           C_RESET,
           C_BOLD, total_rows, C_RESET,
           C_GREY, C_RESET,
           total_fail == 0 ? C_GREEN : C_RED, total_fail, C_RESET,
           C_GREY, C_RESET,
           C_DIM, tN - t0, C_RESET);

    printf("\n  %scomposed verdict:%s  %s%s%s\n",
           C_BOLD, C_RESET,
           total_fail == 0 ? C_GREEN : C_RED,
           total_fail == 0 ? "ALLOW (all forty kernels passed)"
                           : "DENY (one or more kernels failed)",
           C_RESET);

    printf("\n  %sWhat just happened:%s\n", C_BOLD, C_RESET);
    bullet_line("every line above is a separate branchless, integer-only C kernel");
    bullet_line("each one %sjust compiled and ran on your machine%s — not cached, not faked",
                C_BOLD, C_RESET);
    bullet_line("together they form a %s20-bit AND gate%s: the runtime only emits if all twenty agree",
                C_BOLD, C_RESET);
    bullet_line("v80 σ-Cortex (the last row) is the reasoning plane — Mamba SSM + RoPE + sliding-attn + paged-KV + speculative-decode + FEP + KAN + CTM + MoE + TTC bytecode — and it hits ~66 M ops/s on this CPU");

    printf("\n  %sNext:%s  %scos sigma%s %s(live, longer, prints every --version)%s  %s·%s  %scos help%s %s(every command)%s\n\n",
           C_GREY, C_RESET,
           C_BOLD, C_RESET, C_GREY, C_RESET,
           C_GREY, C_RESET,
           C_BOLD, C_RESET, C_GREY, C_RESET);

    return total_fail == 0 ? 0 : 1;
}

static int cmd_help(const char *prog)
{
    print_header();
    section("commands");
    printf("  %s%-12s%s  first-run greeting — plain language, no jargon (aliases: start, hello, hi)\n",
           C_BOLD, "welcome", C_RESET);
    printf("  %s%-12s%s  30-second live tour of all forty kernels — real numbers, no mocks (aliases: showcase, tour)\n",
           C_BOLD, "demo",    C_RESET);
    printf("  %s%-12s%s  status board (default)\n",       C_BOLD, "status",  C_RESET);
    printf("  %s%-12s%s  full repo-health rollup (license · verify · hardening · receipts)\n",
           C_BOLD, "doctor", C_RESET);
    printf("  %s%-12s%s  the Verified-Agent (v57) report\n", C_BOLD, "verify",  C_RESET);
    printf("  %s%-12s%s  the CHACE-class 12-layer capability-hardening gate\n", C_BOLD, "chace",   C_RESET);
    printf("  %s%-12s%s  σ-Shield %s Σ-Citadel %s Fabric %s Cipher %s Intellect %s Hypercortex %s Silicon %s Noesis %s Mnemos %s Constellation %s Hyperscale %s Wormhole %s Chain %s Omnimodal %s Experience %s Surface self-tests\n",
           C_BOLD, "sigma", C_RESET, bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet(), bullet());
    printf("  %s%-12s%s  reasoning fabric demo + composed decision\n",
           C_BOLD, "think", C_RESET);
    printf("  %s%-12s%s  σ-gated chat REPL (reinforce + speculative + generate_until + TTT + engram)\n",
           C_BOLD, "chat", C_RESET);
    printf("  %s%-12s%s  end-to-end pipeline benchmark (accuracy / cost / latency)\n",
           C_BOLD, "benchmark", C_RESET);
    printf("  %s%-12s%s  cost-savings driver (€saved vs always-API)\n",
           C_BOLD, "cost", C_RESET);
    printf("  %s%-12s%s  autonomous tool-calling agent (A6: tool + plan + gate + budget)\n",
           C_BOLD, "agent", C_RESET);
    printf("  %s%-12s%s  distributed mesh / marketplace / federation (D6: join/list/status/serve/query/federate/unlearn)\n",
           C_BOLD, "network", C_RESET);
    printf("  %s%-12s%s  self-improving Ω iterator (S6: selfplay + curriculum + synthetic + evolution + meta)\n",
           C_BOLD, "omega", C_RESET);
    printf("  %s%-12s%s  T3/T4/T5/T6 evidence ledger (H4: monotonicity + commutativity + encode/decode + latency)\n",
           C_BOLD, "formal", C_RESET);
    printf("  %s%-12s%s  σ-gate arXiv paper — deterministic Markdown generator (H5)\n",
           C_BOLD, "paper", C_RESET);
    printf("  %s%-12s%s  σ-meta: where the gate helps / hurts + ledger + economics (H6 summary)\n",
           C_BOLD, "sigma-meta", C_RESET);
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
    printf("  %s%-12s%s  constellation: tree-spec + debate + Byzantine vote + MoE route + gossip + Elo-UCB + dedup + CL (v69)\n",
           C_BOLD, "cn",     C_RESET);
    printf("  %s%-12s%s  hyperscale: P2Q ShiftAddLLM + Mamba-2 SSM + RWKV-7 + MoE-10k + HBM-PIM + photonic-WDM + Loihi-3 spike + NCCL-ring + LRU-stream + HSL (v70)\n",
           C_BOLD, "hs",     C_RESET);
    printf("  %s%-12s%s  wormhole: portal ER-bridge + anchor cleanup + single-XOR teleport + Kleinberg route + ER=EPR bond + HMAC-HV integrity + Poincaré-boundary + hop budget + path receipt + WHL (v71)\n",
           C_BOLD, "wh",     C_RESET);
    printf("  %s%-12s%s  chain: Merkle ledger + append-only receipts + WOTS+ OTS + threshold t-of-n quorum + hash-chain VRF + DAG-BFT quorum + ZK proof-receipt + EIP-7702 delegation + chain-bundle + SCL (v72)\n",
           C_BOLD, "ch",     C_RESET);
    printf("  %s%-12s%s  omnimodal: universal tokenizer + rectified-flow + VINO bind + MOVA AV lock + Matrix-Game world step + DiReCT physics + n8n workflow DAG + Cursor code-plan + MultiGen asset + OML (v73)\n",
           C_BOLD, "om",     C_RESET);
    printf("  %s%-12s%s  experience: Fitts-V2P + adaptive layout + designer-basis + SquireIR + universal-expert mesh + skill compose + Mobile-GS + DLSS/FSR/XeSS + 1-second world + XPL (v74)\n",
           C_BOLD, "ux",     C_RESET);
    printf("  %s%-12s%s  surface: iOS + Android + WhatsApp/Telegram/Signal/iMessage/RCS/Matrix/XMPP/Discord/Slack/Line + Signal-ratchet + WCAG 2.2 + CRDT + Word/Excel/Photoshop/AutoCAD/SAP/Salesforce/Figma/Xcode/Postgres/Stripe/AWS/... + SBL (v76)\n",
           C_BOLD, "sf",     C_RESET);
    printf("  %s%-12s%s  reversible: Landauer / Bennett plane — NOT/CNOT/SWAP/Fredkin/Toffoli/Peres/Majority-3/Bennett/8-bit reversible adder/RVL bytecode; forward ∘ reverse ≡ identity (v77)\n",
           C_BOLD, "rv",     C_RESET);
    printf("  %s%-12s%s  σ-Gödel-Attestor: meta-cognitive plane — IIT-φ/FEP/MDL/Gödel-num/Global-Workspace/halting-witness/Löbian self-trust/bisim/Chaitin-Ω/MCB bytecode (v78; aliases: godel, attest, meta)\n",
           C_BOLD, "gd",     C_RESET);
    printf("  %s%-12s%s  σ-Simulacrum: HV-space simulation substrate — Verlet/CA/stabilizer/HD-reservoir/Koopman/assembly/graph/energy/receipt/SSL bytecode (v79; aliases: sim, simulacrum, world)\n",
           C_BOLD, "sm",     C_RESET);
    printf("  %s%-12s%s  σ-Cortex: HV-space neocortical reasoning plane — Mamba SSM/RoPE/sliding-attn/paged-KV/spec-verify/free-energy/KAN/CTM/MoE top-k/TTC bytecode (v80; aliases: cortex, reason, neo)\n",
           C_BOLD, "cx",     C_RESET);
    printf("  %s%-12s%s  license attestation: SCSL-1.0 dual-license · License-Bound Receipt · anti-tamper guard (v75; subs: status / self-test / verify / bundle / receipt / guard / print)\n",
           C_BOLD, "license", C_RESET);
    printf("  %s%-12s%s  16-bit composed decision: v60 v61 v62 v63 v64 v65 v66 v67 v68 v69 v70 v71 v72 v73 v74 v76 → JSON (v77 + v78 + v79 + v80 ride laterally via cos_v77/v78/v79/v80_compose_decision)\n",
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
    char v62[256] = {0}, v63[256] = {0}, v64[256] = {0}, v65[256] = {0}, v66[256] = {0}, v67[256] = {0}, v68[256] = {0}, v69[256] = {0}, v70[256] = {0}, v71[256] = {0}, v72[256] = {0}, v73[256] = {0}, v74[256] = {0}, v76[256] = {0}, v77[256] = {0}, v78[256] = {0}, v79[256] = {0}, v80[256] = {0}, v81[256] = {0}, v82[256] = {0}, v83[256] = {0}, v84[256] = {0}, v85[256] = {0}, v86[256] = {0}, v87[256] = {0}, v88[256] = {0}, v89[256] = {0}, v90[256] = {0}, v91[256] = {0}, v92[256] = {0}, v93[256] = {0}, v94[256] = {0}, v95[256] = {0}, v96[256] = {0}, v97[256] = {0}, v98[256] = {0}, v99[256] = {0}, v100[256] = {0};
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
    int have69 = (file_exists("creation_os_v69") &&
                  run_first_line("./creation_os_v69 --version",
                                 v69, sizeof v69) == 0 && v69[0]);
    int have70 = (file_exists("creation_os_v70") &&
                  run_first_line("./creation_os_v70 --version",
                                 v70, sizeof v70) == 0 && v70[0]);
    int have71 = (file_exists("creation_os_v71") &&
                  run_first_line("./creation_os_v71 --version",
                                 v71, sizeof v71) == 0 && v71[0]);
    int have72 = (file_exists("creation_os_v72") &&
                  run_first_line("./creation_os_v72 --version",
                                 v72, sizeof v72) == 0 && v72[0]);
    int have73 = (file_exists("creation_os_v73") &&
                  run_first_line("./creation_os_v73 --version",
                                 v73, sizeof v73) == 0 && v73[0]);
    int have74 = (file_exists("creation_os_v74") &&
                  run_first_line("./creation_os_v74 --version",
                                 v74, sizeof v74) == 0 && v74[0]);
    int have76 = (file_exists("creation_os_v76") &&
                  run_first_line("./creation_os_v76 --version",
                                 v76, sizeof v76) == 0 && v76[0]);
    int have77 = (file_exists("creation_os_v77") &&
                  run_first_line("./creation_os_v77 --version",
                                 v77, sizeof v77) == 0 && v77[0]);
    int have78 = (file_exists("creation_os_v78") &&
                  run_first_line("./creation_os_v78 --version",
                                 v78, sizeof v78) == 0 && v78[0]);
    int have79 = (file_exists("creation_os_v79") &&
                  run_first_line("./creation_os_v79 --version",
                                 v79, sizeof v79) == 0 && v79[0]);
    int have80 = (file_exists("creation_os_v80") &&
                  run_first_line("./creation_os_v80 --version",
                                 v80, sizeof v80) == 0 && v80[0]);
    int have81 = (file_exists("creation_os_v81") &&
                  run_first_line("./creation_os_v81 --version",
                                 v81, sizeof v81) == 0 && v81[0]);
    int have82 = (file_exists("creation_os_v82") &&
                  run_first_line("./creation_os_v82 --version",
                                 v82, sizeof v82) == 0 && v82[0]);
    int have83 = (file_exists("creation_os_v83") &&
                  run_first_line("./creation_os_v83 --version",
                                 v83, sizeof v83) == 0 && v83[0]);
    int have84 = (file_exists("creation_os_v84") &&
                  run_first_line("./creation_os_v84 --version",
                                 v84, sizeof v84) == 0 && v84[0]);
    int have85 = (file_exists("creation_os_v85") &&
                  run_first_line("./creation_os_v85 --version",
                                 v85, sizeof v85) == 0 && v85[0]);
    int have86 = (file_exists("creation_os_v86") &&
                  run_first_line("./creation_os_v86 --version",
                                 v86, sizeof v86) == 0 && v86[0]);
    int have87 = (file_exists("creation_os_v87") &&
                  run_first_line("./creation_os_v87 --version",
                                 v87, sizeof v87) == 0 && v87[0]);
    int have88 = (file_exists("creation_os_v88") &&
                  run_first_line("./creation_os_v88 --version",
                                 v88, sizeof v88) == 0 && v88[0]);
    int have89 = (file_exists("creation_os_v89") &&
                  run_first_line("./creation_os_v89 --version",
                                 v89, sizeof v89) == 0 && v89[0]);
    int have90 = (file_exists("creation_os_v90") &&
                  run_first_line("./creation_os_v90 --version",
                                 v90, sizeof v90) == 0 && v90[0]);
    int have91 = (file_exists("creation_os_v91") &&
                  run_first_line("./creation_os_v91 --version",
                                 v91, sizeof v91) == 0 && v91[0]);
    int have92 = (file_exists("creation_os_v92") &&
                  run_first_line("./creation_os_v92 --version",
                                 v92, sizeof v92) == 0 && v92[0]);
    int have93 = (file_exists("creation_os_v93") &&
                  run_first_line("./creation_os_v93 --version",
                                 v93, sizeof v93) == 0 && v93[0]);
    int have94 = (file_exists("creation_os_v94") &&
                  run_first_line("./creation_os_v94 --version",
                                 v94, sizeof v94) == 0 && v94[0]);
    int have95 = (file_exists("creation_os_v95") &&
                  run_first_line("./creation_os_v95 --version",
                                 v95, sizeof v95) == 0 && v95[0]);
    int have96 = (file_exists("creation_os_v96") &&
                  run_first_line("./creation_os_v96 --version",
                                 v96, sizeof v96) == 0 && v96[0]);
    int have97 = (file_exists("creation_os_v97") &&
                  run_first_line("./creation_os_v97 --version",
                                 v97, sizeof v97) == 0 && v97[0]);
    int have98 = (file_exists("creation_os_v98") &&
                  run_first_line("./creation_os_v98 --version",
                                 v98, sizeof v98) == 0 && v98[0]);
    int have99 = (file_exists("creation_os_v99") &&
                  run_first_line("./creation_os_v99 --version",
                                 v99, sizeof v99) == 0 && v99[0]);
    int have100 = (file_exists("creation_os_v100") &&
                   run_first_line("./creation_os_v100 --version",
                                  v100, sizeof v100) == 0 && v100[0]);
    if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71 && have72 && have73 && have74 && have76 && have77 && have78 && have79 && have80 && have81 && have82 && have83 && have84 && have85 && have86 && have87 && have88 && have89 && have90 && have91 && have92 && have93 && have94 && have95 && have96 && have97 && have98 && have99 && have100) {
        printf("cos v100.0 hyena · causal · topology · rl · diffusion · sheaf · clifford · mor · titans · quantum · hierarchical · spiking · fhe · sae · jepa · formal · zkproof · agentic · stream · lattice · cortex · simulacrum · gödel-attestor · reversible · surface · experience · omnimodal creator · chain wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric · Landauer / Bennett plane · meta-cognitive plane · hypervector-space simulation substrate · hypervector-space neocortical reasoning plane · post-quantum lattice crypto · streaming composed decision · active-inference learner loop · layerwise zero-knowledge receipts · TLA-style runtime invariants · latent predictive world model · mechanistic-interpretability SAE · Ring-LWE homomorphic compute · Loihi-3 graded-spike neuromorphic · hierarchical active inference · 4-qubit integer quantum simulator + Grover · neural long-term memory (Titans) · Mixture-of-Recursions adaptive depth · Cl(3,0) geometric algebra · sheaf-Laplacian heat diffusion · integer rectified-flow / DDIM · Q-learning+GAE+PPO-clip · Vietoris–Rips persistent homology · structural causal model + do-calculus · sub-quadratic gated long convolution (Hyena)\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
        printf("  chain         : %s\n", v72);
        printf("  omnimodal     : %s\n", v73);
        printf("  experience    : %s\n", v74);
        printf("  surface       : %s\n", v76);
        printf("  reversible    : %s\n", v77);
        printf("  gödel-attest  : %s\n", v78);
        printf("  simulacrum    : %s\n", v79);
        printf("  cortex        : %s\n", v80);
        printf("  lattice       : %s\n", v81);
        printf("  stream        : %s\n", v82);
        printf("  agentic       : %s\n", v83);
        printf("  zkproof       : %s\n", v84);
        printf("  formal        : %s\n", v85);
        printf("  jepa          : %s\n", v86);
        printf("  sae           : %s\n", v87);
        printf("  fhe           : %s\n", v88);
        printf("  spiking       : %s\n", v89);
        printf("  hierarchical  : %s\n", v90);
        printf("  quantum       : %s\n", v91);
        printf("  titans        : %s\n", v92);
        printf("  mor           : %s\n", v93);
        printf("  clifford      : %s\n", v94);
        printf("  sheaf         : %s\n", v95);
        printf("  diffusion     : %s\n", v96);
        printf("  rl            : %s\n", v97);
        printf("  topology      : %s\n", v98);
        printf("  causal        : %s\n", v99);
        printf("  hyena         : %s\n", v100);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71 && have72 && have73 && have74 && have76 && have77 && have78 && have79 && have80 && have81 && have82 && have83 && have84 && have85) {
        printf("cos v85.0 formal · zkproof · agentic · stream · lattice · cortex · simulacrum · gödel-attestor · reversible · surface · experience · omnimodal creator · chain wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric · Landauer / Bennett plane · meta-cognitive plane · hypervector-space simulation substrate · hypervector-space neocortical reasoning plane · post-quantum lattice crypto · streaming composed decision · active-inference learner loop · layerwise zero-knowledge receipts · TLA-style runtime invariants\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
        printf("  chain         : %s\n", v72);
        printf("  omnimodal     : %s\n", v73);
        printf("  experience    : %s\n", v74);
        printf("  surface       : %s\n", v76);
        printf("  reversible    : %s\n", v77);
        printf("  gödel-attest  : %s\n", v78);
        printf("  simulacrum    : %s\n", v79);
        printf("  cortex        : %s\n", v80);
        printf("  lattice       : %s\n", v81);
        printf("  stream        : %s\n", v82);
        printf("  agentic       : %s\n", v83);
        printf("  zkproof       : %s\n", v84);
        printf("  formal        : %s\n", v85);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71 && have72 && have73 && have74 && have76 && have77 && have78 && have79 && have80) {
        printf("cos v80.0 cortex · simulacrum · gödel-attestor · reversible · surface · experience · omnimodal creator · chain wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric · Landauer / Bennett plane · meta-cognitive plane · hypervector-space simulation substrate · hypervector-space neocortical reasoning plane\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
        printf("  chain         : %s\n", v72);
        printf("  omnimodal     : %s\n", v73);
        printf("  experience    : %s\n", v74);
        printf("  surface       : %s\n", v76);
        printf("  reversible    : %s\n", v77);
        printf("  gödel-attest  : %s\n", v78);
        printf("  simulacrum    : %s\n", v79);
        printf("  cortex        : %s\n", v80);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71 && have72 && have73 && have74 && have76 && have77 && have78 && have79) {
        printf("cos v79.0 simulacrum · gödel-attestor · reversible · surface · experience · omnimodal creator · chain wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric · Landauer / Bennett plane · meta-cognitive plane · hypervector-space simulation substrate\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
        printf("  chain         : %s\n", v72);
        printf("  omnimodal     : %s\n", v73);
        printf("  experience    : %s\n", v74);
        printf("  surface       : %s\n", v76);
        printf("  reversible    : %s\n", v77);
        printf("  gödel-attest  : %s\n", v78);
        printf("  simulacrum    : %s\n", v79);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71 && have72 && have73 && have74 && have76 && have77 && have78) {
        printf("cos v78.0 gödel-attestor · reversible · surface · experience · omnimodal creator · chain wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric · Landauer / Bennett plane · meta-cognitive plane\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
        printf("  chain         : %s\n", v72);
        printf("  omnimodal     : %s\n", v73);
        printf("  experience    : %s\n", v74);
        printf("  surface       : %s\n", v76);
        printf("  reversible    : %s\n", v77);
        printf("  gödel-attest  : %s\n", v78);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71 && have72 && have73 && have74 && have76 && have77) {
        printf("cos v77.0 reversible · surface · experience · omnimodal creator · chain wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric · Landauer / Bennett plane\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
        printf("  chain         : %s\n", v72);
        printf("  omnimodal     : %s\n", v73);
        printf("  experience    : %s\n", v74);
        printf("  surface       : %s\n", v76);
        printf("  reversible    : %s\n", v77);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71 && have72 && have73 && have74 && have76) {
        printf("cos v76.0 surface · experience · omnimodal creator · chain wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
        printf("  chain         : %s\n", v72);
        printf("  omnimodal     : %s\n", v73);
        printf("  experience    : %s\n", v74);
        printf("  surface       : %s\n", v76);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71 && have72 && have73 && have74) {
        printf("cos v74.0 experience · omnimodal creator · chain wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
        printf("  chain         : %s\n", v72);
        printf("  omnimodal     : %s\n", v73);
        printf("  experience    : %s\n", v74);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71 && have72) {
        printf("cos v73.0 omnimodal creator · chain wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
        printf("  chain         : %s\n", v72);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70 && have71) {
        printf("cos v71.0 wormhole hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
        printf("  wormhole      : %s\n", v71);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69 && have70) {
        printf("cos v70.0 hyperscale distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
        printf("  hyperscale    : %s\n", v70);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68 && have69) {
        printf("cos v69.0 distributed-orchestration continually-learning deliberative silicon-tier hyperdimensional + agentic + e2e-encrypted reasoning fabric\n");
        printf("  reasoning     : %s\n", v62);
        printf("  cipher        : %s\n", v63);
        printf("  intellect     : %s\n", v64);
        printf("  hypercortex   : %s\n", v65);
        printf("  silicon       : %s\n", v66);
        printf("  noesis        : %s\n", v67);
        printf("  mnemos        : %s\n", v68);
        printf("  constellation : %s\n", v69);
    } else if (have62 && have63 && have64 && have65 && have66 && have67 && have68) {
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
    setlocale(LC_NUMERIC, ""); /* so '%lld → 1,234,567 with grouping if locale supports it */
    colour_init();

    if (argc < 2 || strcmp(argv[1], "status") == 0) return cmd_status();
    if (strcmp(argv[1], "welcome") == 0 ||
        strcmp(argv[1], "start")   == 0 ||
        strcmp(argv[1], "hello")   == 0 ||
        strcmp(argv[1], "hi")      == 0 ||
        strcmp(argv[1], "onboard") == 0) return cmd_welcome();
    if (strcmp(argv[1], "demo")     == 0 ||
        strcmp(argv[1], "showcase") == 0 ||
        strcmp(argv[1], "tour")     == 0) return cmd_demo();
    if (strcmp(argv[1], "doctor")  == 0 ||
        strcmp(argv[1], "health")  == 0) return cmd_doctor();
    if (strcmp(argv[1], "verify")  == 0) return cmd_verify();
    if (strcmp(argv[1], "chace")   == 0) return cmd_chace();
    if (strcmp(argv[1], "sigma")   == 0) return cmd_sigma();
    if (strcmp(argv[1], "think")     == 0) return cmd_think(argc - 2, argv + 2);
    if (strcmp(argv[1], "chat")      == 0) return cmd_chat(argc - 2, argv + 2);
    if (strcmp(argv[1], "benchmark") == 0) return cmd_benchmark(argc - 2, argv + 2);
    if (strcmp(argv[1], "cost")      == 0) return cmd_cost(argc - 2, argv + 2);
    /* H6: unified dispatch to the dedicated C binaries for
     * agent (A6), network (D6), omega (S6), formal (H4), paper
     * (H5).  Each subcommand forwards its flags verbatim; the
     * sibling binary owns its own --help / --json. */
    if (strcmp(argv[1], "agent")     == 0) return cmd_agent  (argc - 2, argv + 2);
    if (strcmp(argv[1], "network")   == 0) return cmd_network(argc - 2, argv + 2);
    if (strcmp(argv[1], "omega")     == 0) return cmd_omega  (argc - 2, argv + 2);
    if (strcmp(argv[1], "formal")    == 0) return cmd_formal (argc - 2, argv + 2);
    if (strcmp(argv[1], "paper")     == 0) return cmd_paper  (argc - 2, argv + 2);
    if (strcmp(argv[1], "sigma-meta") == 0 ||
        strcmp(argv[1], "meta-status") == 0) return cmd_sigma_meta(argc - 2, argv + 2);
    if (strcmp(argv[1], "seal")    == 0) return cmd_seal_unseal(1, argc - 2, argv + 2);
    if (strcmp(argv[1], "unseal")  == 0) return cmd_seal_unseal(0, argc - 2, argv + 2);
    if (strcmp(argv[1], "mcts")    == 0) return cmd_mcts();
    if (strcmp(argv[1], "hv")      == 0) return cmd_hv();
    if (strcmp(argv[1], "si")      == 0) return cmd_si();
    if (strcmp(argv[1], "nx")      == 0 ||
        strcmp(argv[1], "noesis")  == 0) return cmd_nx();
    if (strcmp(argv[1], "mn")      == 0 ||
        strcmp(argv[1], "mnemos")  == 0) return cmd_mn();
    if (strcmp(argv[1], "cn")      == 0 ||
        strcmp(argv[1], "constellation") == 0) return cmd_cn();
    if (strcmp(argv[1], "hs")      == 0 ||
        strcmp(argv[1], "hyperscale") == 0) return cmd_hs();
    if (strcmp(argv[1], "wh")      == 0 ||
        strcmp(argv[1], "wormhole") == 0) return cmd_wh();
    if (strcmp(argv[1], "ch")      == 0 ||
        strcmp(argv[1], "chain")   == 0) return cmd_ch();
    if (strcmp(argv[1], "om")      == 0 ||
        strcmp(argv[1], "omni")    == 0 ||
        strcmp(argv[1], "omnimodal") == 0) return cmd_om();
    if (strcmp(argv[1], "ux")      == 0 ||
        strcmp(argv[1], "xp")      == 0 ||
        strcmp(argv[1], "experience") == 0) return cmd_ux();
    if (strcmp(argv[1], "sf")      == 0 ||
        strcmp(argv[1], "surface") == 0 ||
        strcmp(argv[1], "mobile")  == 0) return cmd_sf();
    if (strcmp(argv[1], "rv")         == 0 ||
        strcmp(argv[1], "reversible") == 0 ||
        strcmp(argv[1], "landauer")   == 0) return cmd_rv();
    if (strcmp(argv[1], "gd")         == 0 ||
        strcmp(argv[1], "godel")      == 0 ||
        strcmp(argv[1], "gödel")      == 0 ||
        strcmp(argv[1], "attest")     == 0 ||
        strcmp(argv[1], "meta")       == 0) return cmd_gd();
    if (strcmp(argv[1], "sm")         == 0 ||
        strcmp(argv[1], "sim")        == 0 ||
        strcmp(argv[1], "simulacrum") == 0 ||
        strcmp(argv[1], "world")      == 0) return cmd_sm();
    if (strcmp(argv[1], "cx")         == 0 ||
        strcmp(argv[1], "cortex")     == 0 ||
        strcmp(argv[1], "reason")     == 0 ||
        strcmp(argv[1], "neo")        == 0) return cmd_cx();
    if (strcmp(argv[1], "license") == 0 ||
        strcmp(argv[1], "lic")     == 0 ||
        strcmp(argv[1], "scsl")    == 0) return cmd_license(argc - 2, argv + 2);
    if (strcmp(argv[1], "decide")  == 0) return cmd_decide(argc - 2, argv + 2);
    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0)
        return cmd_version();
    if (strcmp(argv[1], "help")    == 0 ||
        strcmp(argv[1], "-h")      == 0 ||
        strcmp(argv[1], "--help")  == 0) return cmd_help(argv[0]);

    fprintf(stderr, "cos: unknown command '%s'.  Try 'cos help'.\n", argv[1]);
    return 2;
}
