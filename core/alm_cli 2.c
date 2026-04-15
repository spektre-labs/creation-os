#include "alm.h"

#include "cognitive_hook.h"
#include "epistemic_drive.h"
#include "facts_store.h"
#include "hdc.h"
#include "metacognition.h"
#include "world_model.h"
#include "causal_model.h"
#include "reasoner.h"
#include "mcts_reasoner.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *attest_name(alm_attest_t a) {
    switch (a) {
    case ALM_ATTESTED:
        return "ATTESTED";
    case ALM_PARTIAL:
        return "PARTIAL";
    case ALM_UNKNOWN:
        return "UNKNOWN";
    case ALM_HALLUCINATION:
        return "HALLUCINATION";
    default:
        return "?";
    }
}

static void usage(const char *prog) {
    fprintf(stderr,
            "usage: %s [facts.json] \"question\"\n"
            "       %s facts.json --batch   # one question per line stdin, JSON per line\n"
            "       %s --self-test         # no LLM; cognitive_smoke_facts.json vieressä binääriin\n"
            "       %s --wm-eval           # maailmanmalli: graafi + HDC-yleistys (JSON)\n"
            "       %s --causal-eval      # 20 kausaalikysymystä: outcome/because/sigma (JSON)\n"
            "       %s --reasoner-eval    # 100 tripleä + forward chaining + 20 kyselyä (JSON)\n"
            "       %s --mcts-eval        # σ-MCTS rollout-demo (JSON; causal_smoke_facts.json)\n"
            "       %s --hdc-analogy-eval # 10 analogiaa, JSON (stdout)\n"
            "       %s --epistemic-drive-test [facts.json] [steps] [realtime 0|1] [max_facts]\n"
            "       CREATION_OS_FACTS=facts.json %s \"question\"\n"
            "env: ALM_LLM_SCRIPT  ALM_PYTHON  ALM_DISABLE_LLM=1  CREATION_OS_MCTS_BUDGET\n",
            prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

static void trim_line(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = 0;
        n--;
    }
    char *p = s;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
}

static void run_batch(const char *facts, const char *argv0) {
    alm_set_llm_script_from_argv0(argv0);
    if (alm_init(facts) != 0) {
        fprintf(stderr, "alm: facts_load failed: %s\n", facts);
        exit(1);
    }
    char line[4096];
    while (fgets(line, sizeof line, stdin)) {
        trim_line(line);
        if (!line[0] || line[0] == '#')
            continue;
        alm_response_t r = alm_query(line);
        int llm = (r.tier == 2) ? 1 : 0; /* tier 3 = hylkäys ilman LLM-vastausta */
        printf("{\"tier\":%d,\"sigma\":%.8f,\"attest\":%d,\"lat_ns\":%.0f,\"llm\":%d,"
               "\"attest_s\":\"%s\"}\n",
               r.tier, (double)r.sigma, (int)r.attest, r.latency_ns, llm,
               attest_name(r.attest));
        fflush(stdout);
    }
    if (getenv("CREATION_OS_BATCH_META") && getenv("CREATION_OS_BATCH_META")[0] &&
        strcmp(getenv("CREATION_OS_BATCH_META"), "0") != 0) {
        int tc[8];
        int llm_steps = 0;
        float llm_pct = 0.f;
        meta_llm_and_tier_stats(&llm_steps, &llm_pct, tc);
        printf(
            "{\"meta\":true,\"total\":%d,\"llm_pct\":%.4f,\"confidence\":%.4f,\"sigma_trend\":%.6f,"
            "\"tier0\":%d,\"tier1\":%d,\"tier2\":%d,\"tier3p\":%d}\n",
            meta_total_queries(), (double)llm_pct, (double)meta_confidence(),
            (double)meta_sigma_trend(), tc[0], tc[1], tc[2], tc[3]);
        fflush(stdout);
    }
}

int main(int argc, char **argv) {
    const char *facts = getenv("CREATION_OS_FACTS");
    int         batch = 0;

    if (argc >= 2 && strcmp(argv[1], "--hdc-analogy-eval") == 0) {
        return hdc_analogy_run_eval_json();
    }

    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        alm_set_llm_script_from_argv0(argv[0]);
        return alm_run_self_test(argv[0]) ? 0 : 1;
    }

    if (argc >= 2 && strcmp(argv[1], "--wm-eval") == 0) {
        return world_model_run_eval_json();
    }

    if (argc >= 2 && strcmp(argv[1], "--causal-eval") == 0) {
        return causal_run_eval(argv[0]);
    }

    if (argc >= 2 && strcmp(argv[1], "--reasoner-eval") == 0) {
        return reasoner_run_eval_json();
    }

    if (argc >= 2 && strcmp(argv[1], "--mcts-eval") == 0) {
        return mcts_run_eval_json(argv[0]);
    }

    if (argc >= 2 && strcmp(argv[1], "--epistemic-drive-test") == 0) {
        const char *fj = facts ? facts : "facts.json";
        int         steps = 30;
        int         realtime = 0;
        int         cap = 100;
        if (argc >= 3 && argv[2][0])
            fj = argv[2];
        if (argc >= 4)
            steps = atoi(argv[3]);
        if (argc >= 5)
            realtime = atoi(argv[4]);
        if (argc >= 6)
            cap = atoi(argv[5]);
        if (steps < 1)
            steps = 1;
        alm_set_llm_script_from_argv0(argv[0]);
        alm_cognitive_reset_session();
        int load_rc = (cap > 0) ? alm_init_max_facts(fj, cap) : alm_init(fj);
        if (load_rc != 0) {
            fprintf(stderr, "alm: epistemic facts load failed: %s\n", fj);
            return 1;
        }
        int n0 = g_nfacts;
        fprintf(stderr, "alm: epistemic drive: facts=%s cap=%d steps=%d realtime=%d n_facts=%d\n", fj, cap,
                steps, realtime, g_nfacts);
        epistemic_drive_run_batch(steps, realtime);
        epistemic_print_log_json();
        printf("{\"epistemic_integrated_facts\":%d,\"new_facts_from_epistemic\":%d}\n", g_nfacts,
               g_nfacts - n0);
        fflush(stdout);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "--batch") == 0) {
        batch = 1;
        if (!facts)
            facts = "facts.json";
    } else if (argc >= 3 && strcmp(argv[2], "--batch") == 0) {
        batch = 1;
        facts = argv[1];
    }

    if (batch) {
        run_batch(facts ? facts : "facts.json", argv[0]);
        return 0;
    }

    const char *question = NULL;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (argc >= 3) {
        facts = argv[1];
        question = argv[2];
    } else {
        if (!facts)
            facts = "facts.json";
        question = argv[1];
    }

    if (!question || !question[0]) {
        usage(argv[0]);
        return 1;
    }

    alm_set_llm_script_from_argv0(argv[0]);

    if (alm_init(facts) != 0) {
        fprintf(stderr, "alm: facts_load failed: %s\n", facts);
        return 1;
    }

    alm_response_t r = alm_query(question);

    printf("Answer:  %s\n", r.answer);
    printf("Tier:    %d\n", r.tier);
    printf("Sigma:   %.6f\n", (double)r.sigma);
    printf("Attest:  %s\n", attest_name(r.attest));
    printf("Latency: %.0f ns\n", r.latency_ns);

    return 0;
}
