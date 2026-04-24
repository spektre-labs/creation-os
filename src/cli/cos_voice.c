/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * cos voice — thin wrapper: ffmpeg mic → whisper.cpp HTTP
 * → ./cos-chat --once → say/espeak.  libc + libcurl only.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_voice.h"
#include "cos_tui.h"

#include <curl/curl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    char  *data;
    size_t len, cap;
} membuf_t;

static volatile sig_atomic_t g_voice_stop;

static void voice_sigint(int sig)
{
    (void)sig;
    g_voice_stop = 1;
}

static size_t curl_mem_cb(void *ptr, size_t size, size_t nmemb, void *ud) {
    size_t n = size * nmemb;
    membuf_t *m = (membuf_t *)ud;
    if (m->len + n + 1 > m->cap) {
        size_t nc = m->cap ? m->cap * 2u : 8192u;
        while (nc < m->len + n + 1) nc *= 2u;
        char *nd = realloc(m->data, nc);
        if (nd == NULL) return 0;
        m->data = nd;
        m->cap  = nc;
    }
    memcpy(m->data + m->len, ptr, n);
    m->len += n;
    m->data[m->len] = '\0';
    return n;
}

static int json_extract_text(const char *js, char *out, size_t cap) {
    const char *p = strstr(js, "\"text\"");
    if (p == NULL) return -1;
    p = strchr(p, ':');
    if (p == NULL) return -1;
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != '"') return -1;
    ++p;
    size_t w = 0;
    while (*p != '\0' && *p != '"' && w + 1 < cap) {
        if (*p == '\\' && p[1] != '\0') ++p;
        out[w++] = *p++;
    }
    out[w] = '\0';
    return (w > 0) ? 0 : -1;
}

static int whisper_transcribe_file(const char *wav_path, const char *url,
                                     const char *lang_opt, char *out, size_t out_cap) {
    CURL *c = curl_easy_init();
    if (c == NULL) return -1;
    curl_mime *mime = curl_mime_init(c);
    if (mime == NULL) {
        curl_easy_cleanup(c);
        return -1;
    }
    curl_mimepart *pt = curl_mime_addpart(mime);
    curl_mime_name(pt, "file");
    if (curl_mime_filedata(pt, wav_path) != CURLE_OK) {
        curl_mime_free(mime);
        curl_easy_cleanup(c);
        return -1;
    }
    const char *wm = getenv("COS_WHISPER_MODEL");
    if (wm != NULL && wm[0] != '\0') {
        curl_mimepart *pm = curl_mime_addpart(mime);
        curl_mime_name(pm, "model");
        curl_mime_data(pm, wm, CURL_ZERO_TERMINATED);
    }
    if (lang_opt != NULL && lang_opt[0] != '\0') {
        curl_mimepart *pl = curl_mime_addpart(mime);
        curl_mime_name(pl, "language");
        curl_mime_data(pl, lang_opt, CURL_ZERO_TERMINATED);
    }
    membuf_t resp = {0};
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_mem_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 120L);
    CURLcode rc = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_mime_free(mime);
    curl_easy_cleanup(c);
    int ok = (rc == CURLE_OK && http >= 200 && http < 300 && resp.data != NULL
              && json_extract_text(resp.data, out, out_cap) == 0);
    free(resp.data);
    return ok ? 0 : -1;
}

static int record_wav_default(const char *wav_path) {
    const char *cmd0 = getenv("COS_VOICE_RECORD_CMD");
    char cmd[2048];
    if (cmd0 != NULL && cmd0[0] != '\0') {
        if (snprintf(cmd, sizeof cmd, cmd0, wav_path) <= 0)
            return -1;
    } else {
#if defined(__APPLE__)
        if (snprintf(cmd, sizeof cmd,
                     "ffmpeg -nostdin -loglevel error -y -f avfoundation "
                     "-i \":0\" -t 4 -ar 16000 -ac 1 -sample_fmt s16 \"%s\"",
                     wav_path) <= 0)
            return -1;
#else
        if (snprintf(cmd, sizeof cmd,
                     "ffmpeg -nostdin -loglevel error -y -f alsa -i default "
                     "-t 4 -ar 16000 -ac 1 -sample_fmt s16 \"%s\"",
                     wav_path) <= 0)
            return -1;
#endif
    }
    if (system(cmd) != 0)
        return -1;
    struct stat sb;
    return (stat(wav_path, &sb) == 0 && sb.st_size > 44) ? 0 : -1;
}

static int resolve_cos_chat(char *out_path, size_t cap, const char *exe0) {
    struct stat st;
    if (stat("./cos-chat", &st) == 0 && S_ISREG(st.st_mode)
        && access("./cos-chat", X_OK) == 0) {
        snprintf(out_path, cap, "%s", "./cos-chat");
        return 0;
    }
    if (exe0 == NULL || exe0[0] == '\0')
        return -1;
    const char *slash = strrchr(exe0, '/');
    if (slash == NULL)
        return -1;
    size_t dirlen = (size_t)(slash - exe0 + 1u);
    if (dirlen + strlen("cos-chat") + 2u > cap)
        return -1;
    memcpy(out_path, exe0, dirlen);
    memcpy(out_path + dirlen, "cos-chat", sizeof "cos-chat");
    out_path[dirlen + sizeof "cos-chat" - 1u] = '\0';
    if (stat(out_path, &st) == 0 && S_ISREG(st.st_mode)
        && access(out_path, X_OK) == 0)
        return 0;
    return -1;
}

static int run_cos_chat_once(const char *chat_bin, const char *prompt,
                             char *resp, size_t rcap, float *sigma_out,
                             double *cost_out) {
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return -1;
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    if (pid == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0)
            _exit(126);
        close(pipefd[1]);
        execl(chat_bin, chat_bin, "--once", "--no-tui", "--no-stream",
              "--no-coherence", "--prompt", prompt, (char *)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    size_t total = 0;
    resp[0] = '\0';
    for (;;) {
        char tmp[4096];
        ssize_t n = read(pipefd[0], tmp, sizeof tmp);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        if (total + (size_t)n + 1u < rcap) {
            memcpy(resp + total, tmp, (size_t)n);
            total += (size_t)n;
            resp[total] = '\0';
        }
    }
    close(pipefd[0]);
    int st = 0;
    if (waitpid(pid, &st, 0) < 0)
        return -1;
    if (!(WIFEXITED(st) && WEXITSTATUS(st) == 0))
        return -1;

    *sigma_out = 0.5f;
    *cost_out  = 0.0;
    const char *hdr = strstr(resp, "round 0");
    if (hdr != NULL) {
        const char *pk = strstr(hdr, "[σ_peak=");
        if (pk != NULL) {
            float sp = 0.0f;
            if (sscanf(pk, "[σ_peak=%f", &sp) == 1)
                *sigma_out = sp;
        }
    }
    const char *rc = strstr(resp, "[σ=");
    if (rc != NULL && strstr(rc, "€") != NULL)
        (void)sscanf(strstr(rc, "€"), "€%lf", cost_out);
    return 0;
}

static void cos_voice_speak(const char *text, int no_tts) {
    if (no_tts || text == NULL || text[0] == '\0') return;
    char tmpl[] = "/tmp/cos_voice_sayXXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return;
    size_t n = strlen(text);
    const char *p = text;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) break;
        p += (size_t)w;
        n -= (size_t)w;
    }
    close(fd);
    char cmd[512];
#if defined(__APPLE__)
    snprintf(cmd, sizeof cmd, "say -f \"%s\" && rm -f \"%s\"", tmpl, tmpl);
#else
    snprintf(cmd, sizeof cmd, "espeak -f \"%s\" 2>/dev/null; rm -f \"%s\"",
             tmpl, tmpl);
#endif
    (void)system(cmd);
}

int cos_voice_main(int argc, char **argv, const char *exe0) {
    const char *lang = NULL;
    int           no_tts = 0;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--no-tts") == 0)
            no_tts = 1;
        else if (strcmp(argv[i], "--lang") == 0 && i + 1 < argc) {
            lang = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fputs(
                "cos voice — local speech → whisper.cpp → cos-chat → TTS\n"
                "  Requires: ./cos-chat, ffmpeg, whisper.cpp server\n"
                "  Setup: bash scripts/real/setup_whisper.sh\n"
                "  Env: COS_WHISPER_URL (default http://127.0.0.1:2022/v1/audio/transcriptions)\n"
                "       COS_VOICE_WAV — skip mic; transcribe this WAV once then exit\n"
                "       COS_VOICE_RECORD_CMD — snprintf pattern with %%s for wav path\n"
                "  Flags: --lang <code>   whisper language (e.g. fi)\n"
                "         --no-tts        no say/espeak\n",
                stdout);
            return 0;
        }
    }

    char chat_path[1024];
    if (resolve_cos_chat(chat_path, sizeof chat_path, exe0) != 0) {
        fprintf(stderr,
                "cos voice: ./cos-chat not found — run `make cos-chat` from repo root.\n");
        return 127;
    }

    const char *wurl = getenv("COS_WHISPER_URL");
    if (wurl == NULL || wurl[0] == '\0')
        wurl = "http://127.0.0.1:2022/v1/audio/transcriptions";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_voice_stop = 0;
    signal(SIGINT, voice_sigint);

    const char *env_wav = getenv("COS_VOICE_WAV");
    int         one_shot = (env_wav != NULL && env_wav[0] != '\0');
    static const char k_cap[] = "/tmp/cos_voice_capture.wav";

    fputs("cos voice: STT → cos-chat → TTS (Ctrl-C to stop).\n", stderr);

    for (;;) {
        const char *wav_use = k_cap;
        if (!one_shot) {
            (void)unlink(k_cap);
            if (record_wav_default(k_cap) != 0) {
                fprintf(stderr,
                        "cos voice: ffmpeg capture failed (install ffmpeg or set "
                        "COS_VOICE_RECORD_CMD).\n");
                curl_global_cleanup();
                return 2;
            }
        } else {
            wav_use = env_wav;
        }

        char transcript[4096];
        if (whisper_transcribe_file(wav_use, wurl, lang, transcript,
                                    sizeof transcript)
            != 0) {
            fprintf(stderr,
                    "cos voice: whisper HTTP failed (%s). Is the server running?\n",
                    wurl);
            curl_global_cleanup();
            return 3;
        }
        if (transcript[0] == '\0') {
            if (one_shot) break;
            continue;
        }

        printf("You said: %s\n", transcript);

        static char chat_out[256 * 1024];
        float         sigma = 0.5f;
        double        cost  = 0.0;
        if (run_cos_chat_once(chat_path, transcript, chat_out,
                              sizeof chat_out, &sigma, &cost)
            != 0) {
            fprintf(stderr, "cos voice: cos-chat subprocess failed\n");
            curl_global_cleanup();
            return 4;
        }
        fputs(chat_out, stdout);
        if (strchr(chat_out, '\n') == NULL)
            fputc('\n', stdout);

        const char *act = "ACCEPT";
        if (strstr(chat_out, "ABSTAIN") != NULL) act = "ABSTAIN";
        cos_tui_print_receipt(sigma, act, 0.0f, 0.0f, cost);

        static char ansbuf[8192];
        const char *answer = "";
        const char *hdr    = strstr(chat_out, "round 0");
        if (hdr != NULL) {
            hdr += strlen("round 0");
            while (*hdr == ' ') ++hdr;
            const char *br = strstr(hdr, "  [σ_peak=");
            if (br != NULL && br > hdr) {
                size_t alen = (size_t)(br - hdr);
                if (alen >= sizeof ansbuf) alen = sizeof ansbuf - 1u;
                memcpy(ansbuf, hdr, alen);
                ansbuf[alen] = '\0';
                answer = ansbuf;
            }
        }

        if (answer[0] == '\0')
            fputs("AI: (no assistant text parsed from cos-chat output)\n",
                  stdout);
        else
            printf("AI: %s\n", answer);
        cos_voice_speak(answer, no_tts);

        if (one_shot || g_voice_stop) break;
    }

    curl_global_cleanup();
    fputs("cos voice: done.\n", stderr);
    return 0;
}
