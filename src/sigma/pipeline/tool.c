/* σ-Tool primitive (registry + selector + gate + executor). */

#include "tool.h"

#include <string.h>

static float clamp01f(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

cos_action_class_t cos_sigma_tool_risk_to_action(cos_tool_risk_t r) {
    switch (r) {
        case COS_TOOL_SAFE:
        case COS_TOOL_READ:         return COS_ACT_READ;
        case COS_TOOL_WRITE:        return COS_ACT_WRITE;
        case COS_TOOL_NETWORK:      return COS_ACT_NETWORK;
        case COS_TOOL_IRREVERSIBLE: return COS_ACT_IRREVERSIBLE;
        default:                    return COS_ACT_IRREVERSIBLE;
    }
}

int cos_sigma_tool_registry_init(cos_tool_registry_t *reg,
                                 cos_tool_t *storage, int cap)
{
    if (!reg || !storage || cap <= 0) return -1;
    reg->slots = storage;
    reg->cap   = cap;
    reg->count = 0;
    memset(storage, 0, sizeof(cos_tool_t) * (size_t)cap);
    return 0;
}

int cos_sigma_tool_register(cos_tool_registry_t *reg,
                            const cos_tool_t *tool)
{
    if (!reg || !tool || !tool->name) return -1;
    if (reg->count >= reg->cap) return -2;
    /* reject duplicates by name */
    for (int i = 0; i < reg->count; ++i) {
        if (strcmp(reg->slots[i].name, tool->name) == 0) return -3;
    }
    reg->slots[reg->count++] = *tool;
    return 0;
}

static int substr_ci(const char *hay, const char *needle) {
    if (!hay || !needle || !*needle) return 0;
    size_t hn = strlen(hay), nn = strlen(needle);
    if (nn > hn) return 0;
    for (size_t i = 0; i + nn <= hn; ++i) {
        int ok = 1;
        for (size_t j = 0; j < nn; ++j) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

cos_tool_call_t
cos_sigma_tool_select(const cos_tool_registry_t *reg,
                      const char *intent,
                      const char *arguments,
                      float      *out_sigma_select)
{
    cos_tool_call_t c;
    memset(&c, 0, sizeof c);
    c.sigma_select = 1.0f;   /* default: no confidence */
    c.sigma_result = 1.0f;
    c.rc           = 0;
    c.decision     = COS_AGENT_BLOCK;
    c.arguments    = arguments;
    if (!reg || !intent) {
        if (out_sigma_select) *out_sigma_select = c.sigma_select;
        return c;
    }
    /* Try name match first (strongest signal), fall back to
     * description substring. Two hits with the same name count
     * as ambiguous; raise σ. */
    int best = -1;
    int n_name_hits = 0, n_desc_hits = 0;
    for (int i = 0; i < reg->count; ++i) {
        if (substr_ci(intent, reg->slots[i].name)) { best = i; n_name_hits++; }
    }
    if (n_name_hits == 0) {
        for (int i = 0; i < reg->count; ++i) {
            if (substr_ci(intent, reg->slots[i].description)) {
                best = i; n_desc_hits++;
            }
        }
    }
    if (best < 0) {
        if (out_sigma_select) *out_sigma_select = c.sigma_select;
        return c;
    }
    c.tool = &reg->slots[best];
    c.cost_eur = c.tool->cost_eur;
    /* σ_select = 0.05 on a unique name hit, 0.35 on description hit,
     * +0.20 per ambiguous tie (capped at 0.95).                    */
    float sigma = 0.05f;
    if (n_name_hits == 0) sigma = 0.35f;
    if (n_name_hits > 1)  sigma = 0.05f + 0.20f * (float)(n_name_hits - 1);
    if (n_desc_hits > 1)  sigma = 0.35f + 0.20f * (float)(n_desc_hits - 1);
    if (sigma > 0.95f) sigma = 0.95f;
    c.sigma_select = sigma;
    if (out_sigma_select) *out_sigma_select = sigma;
    return c;
}

cos_agent_decision_t
cos_sigma_tool_gate(const cos_sigma_agent_t *agent,
                    const cos_tool_call_t   *call)
{
    if (!agent || !call || !call->tool) return COS_AGENT_BLOCK;
    cos_action_class_t cls =
        cos_sigma_tool_risk_to_action(call->tool->risk);
    cos_agent_decision_t d =
        cos_sigma_agent_gate(agent, cls, call->sigma_select);
    /* Irreversible actions always require CONFIRM minimum. */
    if (call->tool->risk == COS_TOOL_IRREVERSIBLE && d == COS_AGENT_ALLOW) {
        d = COS_AGENT_CONFIRM;
    }
    return d;
}

int cos_sigma_tool_call(cos_tool_call_t *call,
                        const cos_sigma_agent_t *agent,
                        cos_tool_executor_fn executor,
                        void *executor_ctx,
                        int confirmed,
                        const char **out_text)
{
    if (!call || !call->tool || !executor) return -1;
    cos_agent_decision_t d = cos_sigma_tool_gate(agent, call);
    call->decision = d;
    if (d == COS_AGENT_BLOCK)   return -1;
    if (d == COS_AGENT_CONFIRM && !confirmed) return -2;

    const char *text = "";
    float sigma_res  = 1.0f;
    int rc = executor(call, executor_ctx, &text, &sigma_res);
    call->rc           = rc;
    call->sigma_result = clamp01f(sigma_res);
    if (out_text) *out_text = text;
    return rc;
}

/* ---------------- self-test ------------------------------------ */

static int stub_exec_ok(const cos_tool_call_t *call, void *ctx,
                        const char **out_text, float *out_sigma_result) {
    (void)call; (void)ctx;
    *out_text = "42";
    *out_sigma_result = 0.02f;
    return 0;
}

static int stub_exec_noisy(const cos_tool_call_t *call, void *ctx,
                           const char **out_text, float *out_sigma_result) {
    (void)call; (void)ctx;
    *out_text = "???";
    *out_sigma_result = 0.85f;
    return 0;
}

static int stub_exec_fail(const cos_tool_call_t *call, void *ctx,
                          const char **out_text, float *out_sigma_result) {
    (void)call; (void)ctx;
    *out_text = "";
    *out_sigma_result = 1.0f;
    return -7;
}

static int check_guards(void) {
    cos_tool_registry_t r;
    cos_tool_t slots[4];
    if (cos_sigma_tool_registry_init(NULL, slots, 4) == 0) return 10;
    if (cos_sigma_tool_registry_init(&r, NULL, 4)   == 0) return 11;
    if (cos_sigma_tool_registry_init(&r, slots, 0)  == 0) return 12;
    if (cos_sigma_tool_registry_init(&r, slots, 4)  != 0) return 13;
    cos_tool_t t = {.name = "calculator", .risk = COS_TOOL_SAFE};
    if (cos_sigma_tool_register(&r, NULL) == 0) return 14;
    t.name = NULL;
    if (cos_sigma_tool_register(&r, &t) == 0) return 15;
    return 0;
}

static int check_register_and_select(void) {
    cos_tool_registry_t r;
    cos_tool_t slots[8];
    cos_sigma_tool_registry_init(&r, slots, 8);

    cos_tool_t calc    = {.name = "calculator", .description = "math",
                          .risk = COS_TOOL_SAFE, .cost_eur = 0.00001};
    cos_tool_t fread_t = {.name = "file_read",  .description = "read a file",
                          .risk = COS_TOOL_READ, .cost_eur = 0.00005};
    cos_tool_t fwrite_t = {.name = "file_write",.description = "write a file",
                           .risk = COS_TOOL_WRITE, .cost_eur = 0.00005};
    cos_tool_t rm_t    = {.name = "shell_rm",   .description = "delete files",
                          .risk = COS_TOOL_IRREVERSIBLE, .cost_eur = 0.0};
    if (cos_sigma_tool_register(&r, &calc)    != 0) return 20;
    if (cos_sigma_tool_register(&r, &fread_t) != 0) return 21;
    if (cos_sigma_tool_register(&r, &fwrite_t)!= 0) return 22;
    if (cos_sigma_tool_register(&r, &rm_t)    != 0) return 23;
    /* duplicate by name */
    if (cos_sigma_tool_register(&r, &calc)    != -3) return 24;

    float s;
    /* unique name hit */
    cos_tool_call_t c1 = cos_sigma_tool_select(&r, "please calculator 2+2", "2+2", &s);
    if (c1.tool == NULL)                      return 30;
    if (strcmp(c1.tool->name, "calculator"))  return 31;
    if (!(s > 0.0f && s < 0.10f))             return 32;
    /* description-only hit */
    cos_tool_call_t c2 = cos_sigma_tool_select(&r, "i want to read a file", NULL, &s);
    if (c2.tool == NULL)                      return 33;
    if (strcmp(c2.tool->name, "file_read"))   return 34;
    if (!(s > 0.30f && s < 0.40f))            return 35;
    /* no match */
    cos_tool_call_t c3 = cos_sigma_tool_select(&r, "dance the samba", NULL, &s);
    if (c3.tool != NULL)                      return 36;
    if (s < 0.99f)                            return 37;
    return 0;
}

static int check_gate_and_call(void) {
    cos_tool_registry_t r;
    cos_tool_t slots[4];
    cos_sigma_tool_registry_init(&r, slots, 4);
    cos_tool_t calc = {.name = "calculator", .description = "math",
                       .risk = COS_TOOL_SAFE};
    cos_tool_t rm_t = {.name = "shell_rm", .description = "delete files",
                       .risk = COS_TOOL_IRREVERSIBLE};
    cos_sigma_tool_register(&r, &calc);
    cos_sigma_tool_register(&r, &rm_t);

    cos_sigma_agent_t ag;
    cos_sigma_agent_init(&ag, 0.80f, 0.10f);

    float s;
    cos_tool_call_t c_safe = cos_sigma_tool_select(&r, "calculator 2+2", "2+2", &s);
    /* base=0.80, σ≈0.05 → eff=0.76 → READ (0.30) → ALLOW */
    if (cos_sigma_tool_gate(&ag, &c_safe) != COS_AGENT_ALLOW) return 40;

    cos_tool_call_t c_irrev = cos_sigma_tool_select(&r, "shell_rm now", "-rf /", &s);
    /* Even with σ≈0.05 eff=0.76 < 0.95 irrev requirement → CONFIRM band
     * (margin 0.10): 0.76 ≥ 0.85? no. So BLOCK. */
    if (cos_sigma_tool_gate(&ag, &c_irrev) != COS_AGENT_BLOCK) return 41;

    /* IRREV never returns ALLOW even when math would otherwise permit.
     * Full base=1.0, σ=0 → eff=1.0 ≥ 0.95 would normally ALLOW; we
     * force-downgrade to CONFIRM. */
    cos_sigma_agent_t ag2;
    cos_sigma_agent_init(&ag2, 1.0f, 0.10f);
    c_irrev.sigma_select = 0.0f;
    if (cos_sigma_tool_gate(&ag2, &c_irrev) != COS_AGENT_CONFIRM) return 42;

    /* NULL guards. */
    if (cos_sigma_tool_gate(NULL,  &c_safe) != COS_AGENT_BLOCK)   return 43;
    if (cos_sigma_tool_gate(&ag,   NULL)    != COS_AGENT_BLOCK)   return 44;

    /* Execute successful call. */
    const char *text = NULL;
    int rc = cos_sigma_tool_call(&c_safe, &ag, stub_exec_ok, NULL, 0, &text);
    if (rc != 0)                                               return 50;
    if (c_safe.decision != COS_AGENT_ALLOW)                    return 51;
    if (strcmp(text, "42") != 0)                               return 52;
    if (c_safe.sigma_result > 0.05f)                           return 53;

    /* BLOCK path: irrev without confirmation. */
    rc = cos_sigma_tool_call(&c_irrev, &ag, stub_exec_ok, NULL, 0, &text);
    if (rc != -1 && rc != -2)                                  return 54;

    /* CONFIRM path with confirmed=1 executes. */
    rc = cos_sigma_tool_call(&c_irrev, &ag2, stub_exec_ok, NULL, 1, &text);
    if (rc != 0)                                               return 55;
    if (c_irrev.decision != COS_AGENT_CONFIRM)                 return 56;

    /* Noisy result is stamped even on successful rc. */
    cos_tool_call_t c_noisy = cos_sigma_tool_select(&r, "calculator noisy", "?", &s);
    rc = cos_sigma_tool_call(&c_noisy, &ag, stub_exec_noisy, NULL, 0, &text);
    if (rc != 0)                                               return 57;
    if (!(c_noisy.sigma_result > 0.80f))                       return 58;

    /* Executor failure propagates. */
    cos_tool_call_t c_fail = cos_sigma_tool_select(&r, "calculator fail", "?", &s);
    rc = cos_sigma_tool_call(&c_fail, &ag, stub_exec_fail, NULL, 0, &text);
    if (rc != -7)                                              return 59;
    if (c_fail.sigma_result < 0.99f)                           return 60;
    return 0;
}

int cos_sigma_tool_self_test(void) {
    int rc;
    if ((rc = check_guards()))               return rc;
    if ((rc = check_register_and_select()))  return rc;
    if ((rc = check_gate_and_call()))        return rc;
    return 0;
}
