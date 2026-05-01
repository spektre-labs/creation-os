/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Tool self-test binary + canonical selector/gate demo. */

#include "tool.h"
#include "agent.h"

#include <stdio.h>
#include <string.h>

static int exec_calc(const cos_tool_call_t *call, void *ctx,
                     const char **out_text, float *out_sigma_result) {
    (void)call; (void)ctx;
    *out_text = "4";
    *out_sigma_result = 0.02f;
    return 0;
}

static const char *decision_name(cos_agent_decision_t d) {
    switch (d) {
        case COS_AGENT_ALLOW:   return "ALLOW";
        case COS_AGENT_CONFIRM: return "CONFIRM";
        case COS_AGENT_BLOCK:   return "BLOCK";
        default:                return "?";
    }
}

static const char *risk_name(cos_tool_risk_t r) {
    switch (r) {
        case COS_TOOL_SAFE:         return "SAFE";
        case COS_TOOL_READ:         return "READ";
        case COS_TOOL_WRITE:        return "WRITE";
        case COS_TOOL_NETWORK:      return "NETWORK";
        case COS_TOOL_IRREVERSIBLE: return "IRREVERSIBLE";
        default:                    return "?";
    }
}

int main(int argc, char **argv) {
    int rc = cos_sigma_tool_self_test();

    /* Canonical demo: four built-in tools + σ-Agent base 0.80 / margin 0.10 */
    cos_tool_registry_t reg;
    cos_tool_t slots[8];
    cos_sigma_tool_registry_init(&reg, slots, 8);

    cos_tool_t calc       = {.name = "calculator", .description = "math",
                             .risk = COS_TOOL_SAFE,    .cost_eur = 0.00001};
    cos_tool_t file_read  = {.name = "file_read",  .description = "read a file",
                             .risk = COS_TOOL_READ,    .cost_eur = 0.00005};
    cos_tool_t file_write = {.name = "file_write", .description = "write a file",
                             .risk = COS_TOOL_WRITE,   .cost_eur = 0.00005};
    cos_tool_t web_fetch  = {.name = "web_fetch",  .description = "fetch a URL",
                             .risk = COS_TOOL_NETWORK, .cost_eur = 0.00100};
    cos_tool_t shell_rm   = {.name = "shell_rm",   .description = "delete files",
                             .risk = COS_TOOL_IRREVERSIBLE, .cost_eur = 0.0};
    cos_sigma_tool_register(&reg, &calc);
    cos_sigma_tool_register(&reg, &file_read);
    cos_sigma_tool_register(&reg, &file_write);
    cos_sigma_tool_register(&reg, &web_fetch);
    cos_sigma_tool_register(&reg, &shell_rm);

    cos_sigma_agent_t ag;
    cos_sigma_agent_init(&ag, 0.80f, 0.10f);

    float s;
    cos_tool_call_t c_calc  = cos_sigma_tool_select(&reg, "use calculator on 2+2", "2+2", &s);
    cos_tool_call_t c_fread = cos_sigma_tool_select(&reg, "please file_read README.md", "README.md", &s);
    cos_tool_call_t c_rm    = cos_sigma_tool_select(&reg, "shell_rm temp.txt", "temp.txt", &s);
    cos_tool_call_t c_none  = cos_sigma_tool_select(&reg, "teleport me home", NULL, &s);

    /* Drive one successful calc execution. */
    const char *text = NULL;
    int rc_exec = cos_sigma_tool_call(&c_calc, &ag, exec_calc, NULL, 0, &text);

    cos_agent_decision_t d_calc  = cos_sigma_pipeline_tool_gate(&ag, &c_calc);
    cos_agent_decision_t d_fread = cos_sigma_pipeline_tool_gate(&ag, &c_fread);
    cos_agent_decision_t d_rm    = cos_sigma_pipeline_tool_gate(&ag, &c_rm);

    printf("{\"kernel\":\"sigma_tool\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"registry\":{\"cap\":%d,\"count\":%d},"
             "\"tools\":["
               "{\"name\":\"calculator\",\"risk\":\"%s\"},"
               "{\"name\":\"file_read\",\"risk\":\"%s\"},"
               "{\"name\":\"file_write\",\"risk\":\"%s\"},"
               "{\"name\":\"web_fetch\",\"risk\":\"%s\"},"
               "{\"name\":\"shell_rm\",\"risk\":\"%s\"}],"
             "\"select\":{"
               "\"calc\":{\"tool\":\"%s\",\"sigma_sel\":%.4f,\"gate\":\"%s\"},"
               "\"fread\":{\"tool\":\"%s\",\"sigma_sel\":%.4f,\"gate\":\"%s\"},"
               "\"rm\":{\"tool\":\"%s\",\"sigma_sel\":%.4f,\"gate\":\"%s\"},"
               "\"none\":{\"tool\":%s,\"sigma_sel\":%.4f}},"
             "\"exec_calc\":{\"rc\":%d,\"text\":\"%s\","
                             "\"sigma_res\":%.4f,\"decision\":\"%s\","
                             "\"cost_eur\":%.6f}"
             "},"
           "\"pass\":%s}\n",
           rc, reg.cap, reg.count,
           risk_name(calc.risk), risk_name(file_read.risk),
           risk_name(file_write.risk), risk_name(web_fetch.risk),
           risk_name(shell_rm.risk),
           c_calc.tool  ? c_calc.tool->name  : "null",
           (double)c_calc.sigma_select,  decision_name(d_calc),
           c_fread.tool ? c_fread.tool->name : "null",
           (double)c_fread.sigma_select, decision_name(d_fread),
           c_rm.tool    ? c_rm.tool->name    : "null",
           (double)c_rm.sigma_select,    decision_name(d_rm),
           c_none.tool ? "\"?\"" : "null",
           (double)c_none.sigma_select,
           rc_exec, text ? text : "",
           (double)c_calc.sigma_result, decision_name(c_calc.decision),
           c_calc.cost_eur,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Tool demo:\n");
        fprintf(stderr, "  calc      σ_sel=%.2f gate=%s\n",
            (double)c_calc.sigma_select,  decision_name(d_calc));
        fprintf(stderr, "  file_read σ_sel=%.2f gate=%s\n",
            (double)c_fread.sigma_select, decision_name(d_fread));
        fprintf(stderr, "  shell_rm  σ_sel=%.2f gate=%s (BLOCK expected)\n",
            (double)c_rm.sigma_select,    decision_name(d_rm));
    }
    return rc;
}
