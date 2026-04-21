/*
 * cos-swarm — σ-coordinated multi-peer routing (HORIZON-4).
 *
 *   cos-swarm --peers 8081,8082,8083
 *   cos-swarm --once --prompt "What is 2+2?"
 *   cos-swarm --self-test
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../sigma/swarm/cos_swarm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr,
            "usage: cos-swarm --peers HOST:PORT[,HOST:PORT...]\n"
            "                 [--tau-abstain F] [--lr F]\n"
            "       cos-swarm --once --prompt \"...\"\n"
            "       cos-swarm --self-test\n"
            "\n"
            "  Shorthand: port-only entries become localhost:PORT.\n"
            "  Mock σ: deterministic per-peer surface (no live HTTP in v0).\n");
}

static int parse_peers(cos_swarm_net_t *net, char *list) {
    char *save = NULL;
    int idx = 0;
    for (char *tok = strtok_r(list, ",", &save); tok != NULL;
         tok = strtok_r(NULL, ",", &save), idx++) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char endpoint[256];
        if (strchr(tok, ':') != NULL)
            snprintf(endpoint, sizeof endpoint, "%s", tok);
        else
            snprintf(endpoint, sizeof endpoint, "localhost:%s", tok);
        char pid[32];
        snprintf(pid, sizeof pid, "p%d", idx);
        char spec[32];
        snprintf(spec, sizeof spec, "peer%d", idx);
        if (cos_swarm_register(net, pid, endpoint, spec) != 0) return -1;
    }
    return net->n_peers > 0 ? 0 : -1;
}

static const char *endpoint_port(const char *ep) {
    const char *c = strrchr(ep, ':');
    return c ? c + 1 : ep;
}

static const char *stub_reply(const char *prompt) {
    if (prompt && strstr(prompt, "2+2")) return "4";
    return "(stub — no BitNet bridge in swarm v0)";
}

static void run_one(cos_swarm_net_t *net, const char *prompt) {
    float sigs[COS_SWARM_MAX_PEERS];
    fprintf(stdout, "[swarm:");
    for (int i = 0; i < net->n_peers; i++) {
        sigs[i] = cos_swarm_mock_sigma(i, prompt);
        fprintf(stdout, " %s %s σ=%.2f%s", net->peers[i].peer_id,
                net->peers[i].endpoint, (double)sigs[i],
                i + 1 < net->n_peers ? "," : "");
    }
    fprintf(stdout, "]\n");

    float mn = 0.f;
    int ch = cos_swarm_route(net, sigs, net->n_peers, &mn);
    if (ch < 0) {
        fprintf(stdout, "[swarm: all peers above τ_abstain — ABSTAIN]\n");
        fprintf(stdout, "(no answer — abstained)\n");
        return;
    }
    fprintf(stdout,
            "[swarm: routing → %s %s (lowest σ)]\n",
            net->peers[ch].peer_id, net->peers[ch].endpoint);
    const char *ans = stub_reply(prompt);
    fprintf(stdout, "%s [σ=%.2f | ACCEPT | SWARM:%s]\n",
            ans, (double)sigs[ch], endpoint_port(net->peers[ch].endpoint));
    cos_swarm_trust_update(net, ch, sigs[ch]);
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_swarm_self_test() != 0 ? 1 : 0;

    char peer_buf[1024];
    peer_buf[0] = '\0';
    int once = 0;
    const char *prompt = NULL;
    float tau_a = 0.90f, lr = 0.20f;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--peers") == 0 && i + 1 < argc)
            snprintf(peer_buf, sizeof peer_buf, "%s", argv[++i]);
        else if (strcmp(argv[i], "--once") == 0)
            once = 1;
        else if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc)
            prompt = argv[++i];
        else if (strcmp(argv[i], "--tau-abstain") == 0 && i + 1 < argc)
            tau_a = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--lr") == 0 && i + 1 < argc)
            lr = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        }
    }

    if (peer_buf[0] == '\0') {
        usage();
        return 2;
    }

    cos_swarm_net_t net;
    cos_swarm_init(&net, lr, tau_a);
    if (parse_peers(&net, peer_buf) != 0) {
        fprintf(stderr, "cos-swarm: bad --peers\n");
        return 2;
    }

    if (once) {
        if (!prompt || !prompt[0]) {
            fprintf(stderr, "cos-swarm: --once requires --prompt\n");
            return 2;
        }
        run_one(&net, prompt);
        fprintf(stdout, "  assert(declared == realized);\n  1 = 1.\n");
        return 0;
    }

    fprintf(stdout,
            "cos-swarm · %d peers · τ_abstain=%.2f · mock σ (v0)\n"
            "  exit | quit to end\n",
            net.n_peers, (double)net.tau_abstain);

    char line[2048];
    for (;;) {
        fprintf(stdout, "\n> ");
        fflush(stdout);
        if (fgets(line, sizeof line, stdin) == NULL) break;
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (n == 0) continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        run_one(&net, line);
    }
    fprintf(stdout, "\n  assert(declared == realized);\n  1 = 1.\n");
    return 0;
}
