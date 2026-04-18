/*
 * v167 σ-Governance-API — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "governance.h"

#include <stdio.h>
#include <string.h>

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

const char *cos_v167_role_name(cos_v167_role_t r) {
    switch (r) {
        case COS_V167_ROLE_ADMIN:      return "admin";
        case COS_V167_ROLE_USER:       return "user";
        case COS_V167_ROLE_AUDITOR:    return "auditor";
        case COS_V167_ROLE_DEVELOPER:  return "developer";
        default:                       return "?";
    }
}

const char *cos_v167_verdict_name(cos_v167_verdict_t v) {
    switch (v) {
        case COS_V167_VERDICT_EMIT:    return "emit";
        case COS_V167_VERDICT_ABSTAIN: return "abstain";
        case COS_V167_VERDICT_REVISE:  return "revise";
        case COS_V167_VERDICT_DENIED:  return "denied";
        default:                       return "?";
    }
}

const char *cos_v167_health_name(cos_v167_health_t h) {
    switch (h) {
        case COS_V167_HEALTH_OK:       return "ok";
        case COS_V167_HEALTH_DEGRADED: return "degraded";
        case COS_V167_HEALTH_DOWN:     return "down";
        default:                       return "?";
    }
}

uint32_t cos_v167_role_caps(cos_v167_role_t r) {
    switch (r) {
        case COS_V167_ROLE_ADMIN:
            return COS_V167_CAP_CHAT | COS_V167_CAP_READ_AUDIT |
                   COS_V167_CAP_WRITE_POLICY |
                   COS_V167_CAP_INSTALL_PLUGIN |
                   COS_V167_CAP_ENABLE_KERNEL |
                   COS_V167_CAP_ROLLBACK_ADAPTER;
        case COS_V167_ROLE_USER:
            return COS_V167_CAP_CHAT;
        case COS_V167_ROLE_AUDITOR:
            return COS_V167_CAP_READ_AUDIT;
        case COS_V167_ROLE_DEVELOPER:
            return COS_V167_CAP_CHAT |
                   COS_V167_CAP_INSTALL_PLUGIN |
                   COS_V167_CAP_ENABLE_KERNEL;
        default:
            return 0;
    }
}

/* ------------------------------------------------------------- */
/* Init                                                          */
/* ------------------------------------------------------------- */

static void seed_policy(cos_v167_state_t *s, const char *domain,
                        float tau, const char *msg_or_null,
                        bool require_sandbox) {
    cos_v167_policy_t p;
    memset(&p, 0, sizeof(p));
    safe_copy(p.domain, sizeof(p.domain), domain);
    p.tau = tau;
    if (msg_or_null) {
        safe_copy(p.abstain_message, sizeof(p.abstain_message), msg_or_null);
        p.has_abstain_message = true;
    } else {
        p.has_abstain_message = false;
    }
    p.require_sandbox = require_sandbox;
    cos_v167_policy_upsert(s, &p);
}

void cos_v167_init(cos_v167_state_t *s, const char *org) {
    memset(s, 0, sizeof(*s));
    safe_copy(s->org, sizeof(s->org), org ? org : "spektre-labs");
    s->policy_version = 1;

    seed_policy(s, "medical",  0.30f,
                "Consult a professional.", true);
    seed_policy(s, "creative", 0.90f, NULL, false);
    seed_policy(s, "code",     0.50f,
                "Please provide more context.", true);

    cos_v167_fleet_add(s, "lab-m3",   "lab",     "adapter-v1.2.0");
    cos_v167_fleet_add(s, "lab-rpi5", "lab",     "adapter-v1.2.0");
    cos_v167_fleet_add(s, "cloud-a",  "cloud-a", "adapter-v1.2.0");
    cos_v167_fleet_add(s, "cloud-b",  "cloud-b", "adapter-v1.2.0");

    /* stamp initial policy_version onto all nodes */
    cos_v167_fleet_push_policy(s);
}

/* ------------------------------------------------------------- */
/* Policy                                                        */
/* ------------------------------------------------------------- */

int cos_v167_policy_upsert(cos_v167_state_t *s,
                           const cos_v167_policy_t *p) {
    if (!s || !p) return -1;
    for (int i = 0; i < s->n_policies; ++i) {
        if (strcmp(s->policies[i].domain, p->domain) == 0) {
            s->policies[i] = *p;
            s->policy_version++;
            return 0;
        }
    }
    if (s->n_policies >= COS_V167_MAX_POLICIES) return -1;
    s->policies[s->n_policies++] = *p;
    s->policy_version++;
    return 0;
}

const cos_v167_policy_t *cos_v167_policy_get(const cos_v167_state_t *s,
                                             const char *domain) {
    if (!s || !domain) return NULL;
    for (int i = 0; i < s->n_policies; ++i) {
        if (strcmp(s->policies[i].domain, domain) == 0)
            return &s->policies[i];
    }
    return NULL;
}

/* ------------------------------------------------------------- */
/* Fleet                                                         */
/* ------------------------------------------------------------- */

int cos_v167_fleet_add(cos_v167_state_t *s,
                       const char *node_id, const char *location,
                       const char *adapter) {
    if (!s || !node_id) return -1;
    if (s->n_nodes >= COS_V167_MAX_NODES) return -1;
    cos_v167_node_t *n = &s->nodes[s->n_nodes++];
    memset(n, 0, sizeof(*n));
    safe_copy(n->node_id,  sizeof(n->node_id),  node_id);
    safe_copy(n->location, sizeof(n->location), location ? location : "");
    safe_copy(n->adapter,  sizeof(n->adapter),  adapter ? adapter : "adapter-v1.0.0");
    n->health = COS_V167_HEALTH_OK;
    n->policy_version = 0;
    return 0;
}

static cos_v167_node_t *find_node(cos_v167_state_t *s, const char *id) {
    if (!s || !id) return NULL;
    for (int i = 0; i < s->n_nodes; ++i)
        if (strcmp(s->nodes[i].node_id, id) == 0) return &s->nodes[i];
    return NULL;
}

int cos_v167_fleet_set_health(cos_v167_state_t *s,
                              const char *node_id,
                              cos_v167_health_t h) {
    cos_v167_node_t *n = find_node(s, node_id);
    if (!n) return -1;
    n->health = h;
    return 0;
}

int cos_v167_fleet_push_policy(cos_v167_state_t *s) {
    if (!s) return -1;
    int updated = 0;
    for (int i = 0; i < s->n_nodes; ++i) {
        if (s->nodes[i].health != COS_V167_HEALTH_DOWN) {
            s->nodes[i].policy_version = s->policy_version;
            updated++;
        }
    }
    return updated;
}

int cos_v167_fleet_rollback(cos_v167_state_t *s, const char *adapter) {
    if (!s || !adapter) return -1;
    int updated = 0;
    for (int i = 0; i < s->n_nodes; ++i) {
        safe_copy(s->nodes[i].adapter, sizeof(s->nodes[i].adapter), adapter);
        updated++;
    }
    return updated;
}

/* ------------------------------------------------------------- */
/* Audit + evaluation                                            */
/* ------------------------------------------------------------- */

static void audit_append(cos_v167_state_t *s, const cos_v167_audit_t *a) {
    if (!s || !a) return;
    if (s->n_audit < COS_V167_MAX_AUDIT) {
        s->audit[s->n_audit++] = *a;
    } else {
        memmove(s->audit, s->audit + 1,
                (COS_V167_MAX_AUDIT - 1) * sizeof(cos_v167_audit_t));
        s->audit[COS_V167_MAX_AUDIT - 1] = *a;
    }
}

cos_v167_verdict_t cos_v167_evaluate(cos_v167_state_t *s,
                                     const char *actor,
                                     cos_v167_role_t role,
                                     const char *node_id,
                                     const char *domain,
                                     const char *prompt,
                                     float sigma_product) {
    cos_v167_audit_t a;
    memset(&a, 0, sizeof(a));
    a.ts_fake = ++s->ts_counter;
    safe_copy(a.actor,   sizeof(a.actor),   actor ? actor : "anonymous");
    a.role = role;
    safe_copy(a.node_id, sizeof(a.node_id), node_id ? node_id : "?");
    safe_copy(a.domain,  sizeof(a.domain),  domain  ? domain  : "?");
    safe_copy(a.prompt,  sizeof(a.prompt),  prompt  ? prompt  : "");
    a.sigma_product = sigma_product;

    uint32_t caps = cos_v167_role_caps(role);
    if (!(caps & COS_V167_CAP_CHAT)) {
        a.verdict = COS_V167_VERDICT_DENIED;
        safe_copy(a.note, sizeof(a.note), "role lacks chat capability");
        audit_append(s, &a);
        return a.verdict;
    }

    const cos_v167_policy_t *p = cos_v167_policy_get(s, domain);
    if (!p) {
        /* unknown domain defaults to τ=0.5 + no sandbox */
        if (sigma_product > 0.5f) {
            a.verdict = COS_V167_VERDICT_ABSTAIN;
            safe_copy(a.note, sizeof(a.note),
                      "unknown domain, default τ=0.5 → abstain");
        } else {
            a.verdict = COS_V167_VERDICT_EMIT;
            safe_copy(a.note, sizeof(a.note),
                      "unknown domain, default τ=0.5 → emit");
        }
        audit_append(s, &a);
        return a.verdict;
    }

    if (sigma_product > p->tau) {
        a.verdict = COS_V167_VERDICT_ABSTAIN;
        if (p->has_abstain_message)
            safe_copy(a.note, sizeof(a.note), p->abstain_message);
        else
            safe_copy(a.note, sizeof(a.note), "abstain");
    } else if (p->require_sandbox) {
        a.verdict = COS_V167_VERDICT_REVISE;
        safe_copy(a.note, sizeof(a.note),
                  "require_sandbox → revise through sandbox");
    } else {
        a.verdict = COS_V167_VERDICT_EMIT;
        safe_copy(a.note, sizeof(a.note), "emit");
    }
    audit_append(s, &a);
    return a.verdict;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

size_t cos_v167_to_json(const cos_v167_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v167\",\"org\":\"%s\","
        "\"policy_version\":%d,\"policies\":[",
        s->org, s->policy_version);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_policies; ++i) {
        const cos_v167_policy_t *p = &s->policies[i];
        char msg[COS_V167_MAX_MSG + 4];
        if (p->has_abstain_message) snprintf(msg, sizeof(msg), "\"%s\"", p->abstain_message);
        else                         snprintf(msg, sizeof(msg), "null");
        n = snprintf(buf + used, cap - used,
            "%s{\"domain\":\"%s\",\"tau\":%.4f,\"abstain_message\":%s,"
            "\"require_sandbox\":%s}",
            i == 0 ? "" : ",",
            p->domain, (double)p->tau, msg,
            p->require_sandbox ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"fleet\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_nodes; ++i) {
        const cos_v167_node_t *nd = &s->nodes[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"node_id\":\"%s\",\"location\":\"%s\","
            "\"health\":\"%s\",\"policy_version\":%d,\"adapter\":\"%s\"}",
            i == 0 ? "" : ",",
            nd->node_id, nd->location,
            cos_v167_health_name(nd->health),
            nd->policy_version, nd->adapter);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used,
                 "],\"audit_count\":%d}", s->n_audit);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

size_t cos_v167_audit_to_ndjson(const cos_v167_state_t *s,
                                char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    for (int i = 0; i < s->n_audit; ++i) {
        const cos_v167_audit_t *a = &s->audit[i];
        int n = snprintf(buf + used, cap - used,
            "{\"ts\":%llu,\"actor\":\"%s\",\"role\":\"%s\","
            "\"node_id\":\"%s\",\"domain\":\"%s\","
            "\"verdict\":\"%s\",\"sigma_product\":%.4f,"
            "\"prompt\":\"%s\",\"note\":\"%s\"}\n",
            (unsigned long long)a->ts_fake, a->actor,
            cos_v167_role_name(a->role),
            a->node_id, a->domain,
            cos_v167_verdict_name(a->verdict),
            (double)a->sigma_product, a->prompt, a->note);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    buf[used] = '\0';
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v167_self_test(void) {
    cos_v167_state_t s;
    cos_v167_init(&s, "spektre-labs");
    if (s.n_policies != 3) return 1;
    if (s.n_nodes    != 4) return 2;

    /* medical: τ=0.3 + abstain_message + require_sandbox */
    const cos_v167_policy_t *m = cos_v167_policy_get(&s, "medical");
    if (!m) return 3;
    if (m->tau > 0.31f)     return 4;
    if (!m->has_abstain_message) return 5;
    if (!m->require_sandbox) return 6;

    /* evaluate a medical prompt with σ=0.40 → abstain */
    cos_v167_verdict_t v = cos_v167_evaluate(&s, "alice",
                                             COS_V167_ROLE_USER,
                                             "lab-m3", "medical",
                                             "Is aspirin safe?", 0.40f);
    if (v != COS_V167_VERDICT_ABSTAIN) return 7;

    /* evaluate a code prompt with σ=0.20, require_sandbox → revise */
    v = cos_v167_evaluate(&s, "bob",
                          COS_V167_ROLE_DEVELOPER,
                          "cloud-a", "code",
                          "refactor this", 0.20f);
    if (v != COS_V167_VERDICT_REVISE) return 8;

    /* an auditor cannot chat → denied */
    v = cos_v167_evaluate(&s, "carol",
                          COS_V167_ROLE_AUDITOR,
                          "cloud-b", "creative",
                          "write a poem", 0.10f);
    if (v != COS_V167_VERDICT_DENIED) return 9;

    /* policy push: fleet stamped with new version */
    int pv_before = s.policy_version;
    cos_v167_policy_t new_pol;
    memset(&new_pol, 0, sizeof(new_pol));
    safe_copy(new_pol.domain, sizeof(new_pol.domain), "medical");
    new_pol.tau = 0.25f;
    new_pol.has_abstain_message = true;
    safe_copy(new_pol.abstain_message, sizeof(new_pol.abstain_message),
              "Not medical advice.");
    new_pol.require_sandbox = true;
    if (cos_v167_policy_upsert(&s, &new_pol) != 0) return 10;
    if (s.policy_version <= pv_before) return 11;
    int updated = cos_v167_fleet_push_policy(&s);
    if (updated != s.n_nodes) return 12;
    for (int i = 0; i < s.n_nodes; ++i)
        if (s.nodes[i].policy_version != s.policy_version) return 13;

    /* rollback adapter */
    if (cos_v167_fleet_rollback(&s, "adapter-v1.1.0") != s.n_nodes) return 14;
    for (int i = 0; i < s.n_nodes; ++i)
        if (strcmp(s.nodes[i].adapter, "adapter-v1.1.0") != 0) return 15;

    /* health down: push should skip that node */
    cos_v167_fleet_set_health(&s, "cloud-b", COS_V167_HEALTH_DOWN);
    /* bump version by upserting another policy */
    seed_policy(&s, "legal", 0.20f, "Consult a lawyer.", true);
    int pv = s.policy_version;
    int upd = cos_v167_fleet_push_policy(&s);
    if (upd != s.n_nodes - 1) return 16;
    /* cloud-b must NOT be at the new version */
    for (int i = 0; i < s.n_nodes; ++i)
        if (strcmp(s.nodes[i].node_id, "cloud-b") == 0 &&
            s.nodes[i].policy_version == pv) return 17;

    /* audit log has at least one denied entry */
    int denied = 0, abstain = 0, revise = 0;
    for (int i = 0; i < s.n_audit; ++i) {
        if (s.audit[i].verdict == COS_V167_VERDICT_DENIED)  denied++;
        if (s.audit[i].verdict == COS_V167_VERDICT_ABSTAIN) abstain++;
        if (s.audit[i].verdict == COS_V167_VERDICT_REVISE)  revise++;
    }
    if (denied < 1 || abstain < 1 || revise < 1) return 18;

    return 0;
}
