/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * cos marketplace — σ services + proof receipts (CLI).
 */

#include "cos_marketplace_cli.h"

#include "../sigma/marketplace.h"
#include "../sigma/reputation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(FILE *o)
{
    fputs(
        "usage:\n"
        "  cos marketplace\n"
        "  cos marketplace --register [--endpoint URL]\n"
        "  cos marketplace --my-services\n"
        "  cos marketplace --request \"prompt\" [--service-id ID]\n"
        "  cos marketplace --stats\n"
        "  cos marketplace --self-test\n",
        o);
}

static const char *node_id_default(void)
{
    const char *p = getenv("COS_NODE_ID");
    return (p && p[0]) ? p : "local-node";
}

static int cmd_browse(void)
{
    struct cos_service sv[64];
    int                 n = 0;
    if (cos_marketplace_browse(sv, 64, &n) != 0) {
        fputs("cos marketplace: browse failed\n", stderr);
        return 2;
    }
    printf("%d service(s)\n", n);
    for (int i = 0; i < n; i++) {
        printf(
            "  %-20s  %-40s  σ=%.4f  rel=%.4f  €%.4f  endpoint=%s\n",
            sv[i].service_id, sv[i].name, (double)sv[i].sigma_mean,
            (double)sv[i].reliability, (double)sv[i].cost_eur,
            sv[i].endpoint[0] ? sv[i].endpoint : "(none)");
    }
    return 0;
}

static int cmd_register(const char *endpoint_override)
{
    const char *nid = node_id_default();
    struct cos_service s;
    memset(&s, 0, sizeof s);
    snprintf(s.service_id, sizeof s.service_id, "%s", "general");
    snprintf(s.name, sizeof s.name, "%s",
             "General σ-verified assistance");
    snprintf(s.description, sizeof s.description, "%s",
             "Registered local offering (tune catalog via SQLite or API).");
    snprintf(s.provider_id, sizeof s.provider_id, "%s", nid);
    s.sigma_mean   = 0.18f;
    s.reliability  = 0.85f;
    s.times_served = 0;
    s.cost_eur     = 0.f;
    s.registered   = (int64_t)time(NULL) * 1000LL;
    const char *ep = endpoint_override;
    if (!ep || !ep[0]) ep = getenv("COS_PUBLIC_ENDPOINT");
    if (ep && ep[0])
        snprintf(s.endpoint, sizeof s.endpoint, "%s", ep);
    else
        snprintf(s.endpoint, sizeof s.endpoint, "%s", "http://127.0.0.1:3030");

    if (cos_marketplace_register_service(&s) != 0) {
        fputs("cos marketplace: register failed\n", stderr);
        return 2;
    }
    printf("registered service_id=%s endpoint=%s\n", s.service_id, s.endpoint);
    return 0;
}

static int cmd_my_services(void)
{
    struct cos_service sv[64];
    int                 n = 0;
    if (cos_marketplace_browse(sv, 64, &n) != 0) return 2;
    const char *nid = node_id_default();
    printf("services for provider_id=%s\n", nid);
    for (int i = 0; i < n; i++) {
        if (strcmp(sv[i].provider_id, nid) != 0) continue;
        printf("  %s  σ=%.4f  served=%d  €%.4f\n", sv[i].service_id,
               (double)sv[i].sigma_mean, sv[i].times_served,
               (double)sv[i].cost_eur);
    }
    return 0;
}

static int cmd_request(const char *prompt, const char *svc_id)
{
    struct cos_service_request rq;
    memset(&rq, 0, sizeof rq);
    snprintf(rq.service_id, sizeof rq.service_id, "%s",
             svc_id && svc_id[0] ? svc_id : "general");
    snprintf(rq.prompt, sizeof rq.prompt, "%s", prompt);
    snprintf(rq.requester_id, sizeof rq.requester_id, "%s", node_id_default());
    rq.max_sigma    = 1.0f;
    rq.max_cost_eur = 1e9f;
    rq.timeout_ms   = 120000LL;

    struct cos_service_response resp;
    memset(&resp, 0, sizeof resp);
    int rc = cos_marketplace_request(&rq, &resp);
    if (rc != 0) {
        fprintf(stderr, "cos marketplace: request failed (%d)\n", rc);
        return 2;
    }
    printf("σ=%.4f  cost€=%.4f  verify=%d\n", (double)resp.sigma,
           (double)resp.cost_eur, cos_marketplace_verify_receipt(&resp));
    printf("%s\n", resp.response);
    return 0;
}

static int cmd_stats(void)
{
    cos_marketplace_stats_fprint(stdout);
    struct cos_reputation nr[32];
    int nn = 0;
    if (cos_reputation_ranking(nr, 32, &nn) == 0 && nn > 0) {
        fputs("reputation ranking (trust composite)\n", stdout);
        for (int i = 0; i < nn; i++)
            printf("  %s  trust=%.4f  σ_served=%.4f  rel=%.4f  served=%d\n",
                   nr[i].node_id, (double)nr[i].trust_composite,
                   (double)nr[i].sigma_mean_served, (double)nr[i].reliability,
                   nr[i].total_served);
    }
    return 0;
}

int cos_marketplace_main(int argc, char **argv)
{
    int do_reg = 0, do_my = 0, do_req = 0, do_stats = 0, do_st = 0;
    const char *prompt = NULL, *svc = NULL, *endo = NULL;

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--register")) do_reg = 1;
        else if (!strcmp(argv[i], "--my-services")) do_my = 1;
        else if (!strcmp(argv[i], "--stats")) do_stats = 1;
        else if (!strcmp(argv[i], "--self-test")) do_st = 1;
        else if (!strcmp(argv[i], "--request") && i + 1 < argc)
            prompt = argv[++i], do_req = 1;
        else if (!strcmp(argv[i], "--service-id") && i + 1 < argc)
            svc = argv[++i];
        else if (!strcmp(argv[i], "--endpoint") && i + 1 < argc)
            endo = argv[++i];
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(stdout);
            return 0;
        }
    }

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_MARKETPLACE_TEST) \
    || defined(COS_REPUTATION_TEST)
    if (do_st) {
        if (cos_reputation_self_test() != 0
            || cos_marketplace_self_test() != 0) {
            fputs("cos marketplace: --self-test failed\n", stderr);
            return 2;
        }
        fputs("cos marketplace: --self-test OK\n", stdout);
        return 0;
    }
#endif

    if (cos_marketplace_init() != 0 || cos_reputation_init() != 0) {
        fputs("cos marketplace: database init failed\n", stderr);
        return 2;
    }

    int ret = 0;
    if (do_reg)
        ret = cmd_register(endo);
    else if (do_my)
        ret = cmd_my_services();
    else if (do_req) {
        if (!prompt) {
            usage(stderr);
            ret = 2;
        } else
            ret = cmd_request(prompt, svc);
    } else if (do_stats)
        ret = cmd_stats();
    else
        ret = cmd_browse();

    cos_marketplace_shutdown();
    cos_reputation_shutdown();
    return ret;
}
