/*
 * v169 σ-Ontology — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ontology.h"

#include <stdio.h>
#include <string.h>

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (splitmix64(s) >> 40) / 16777216.0f;
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

const char *cos_v169_class_name(cos_v169_class_t c) {
    switch (c) {
        case COS_V169_CLASS_PERSON:   return "Person";
        case COS_V169_CLASS_SOFTWARE: return "Software";
        case COS_V169_CLASS_METRIC:   return "Metric";
        case COS_V169_CLASS_KERNEL:   return "Kernel";
        case COS_V169_CLASS_DEVICE:   return "Device";
        case COS_V169_CLASS_CONCEPT:  return "Concept";
        default:                      return "?";
    }
}

/* ------------------------------------------------------------- */
/* Fixture                                                       */
/* ------------------------------------------------------------- */

static const char *S_SUBJECTS[] = {
    "lauri", "rene", "mikko", "topias",
    "creation_os", "bitnet", "cursor",
    "sigma", "tau", "entropy",
    "v101", "v106", "v115", "v135", "v163", "v169",
    "rpi5", "jetson", "macbook_m3"
};
static const int N_SUB = sizeof(S_SUBJECTS) / sizeof(S_SUBJECTS[0]);

static const char *S_PREDS[] = {
    "mentions", "uses", "implements", "extends",
    "instance_of", "requires", "depends_on", "measures"
};
static const int N_PRED = sizeof(S_PREDS) / sizeof(S_PREDS[0]);

static const char *S_OBJECTS[] = {
    "creation_os", "bitnet", "sigma",
    "tau", "entropy", "memory",
    "v101", "v106", "v115",
    "rpi5", "macbook_m3", "person",
    "software", "metric", "kernel"
};
static const int N_OBJ = sizeof(S_OBJECTS) / sizeof(S_OBJECTS[0]);

void cos_v169_fixture_50(cos_v169_ontology_t *o) {
    uint64_t s = o->seed ^ 0x169AAABBCCDD1069ULL;
    for (int i = 0; i < COS_V169_N_ENTRIES; ++i) {
        cos_v169_entry_t *e = &o->entries[i];
        memset(e, 0, sizeof(*e));
        e->id = i;
        e->entropy_bits = splitmix64(&s);

        /* Compose a short synthetic sentence; the extractor
         * parses its (subject, predicate, object) triples. */
        const char *sub = S_SUBJECTS[(e->entropy_bits >>  0) % N_SUB];
        const char *prd = S_PREDS   [(e->entropy_bits >> 16) % N_PRED];
        const char *obj = S_OBJECTS [(e->entropy_bits >> 32) % N_OBJ];
        snprintf(e->text, sizeof(e->text),
                 "[#%02d] %s %s %s (auto)", i, sub, prd, obj);
    }
    o->n_entries = COS_V169_N_ENTRIES;
}

void cos_v169_init(cos_v169_ontology_t *o, uint64_t seed) {
    memset(o, 0, sizeof(*o));
    o->seed = seed ? seed : 0x169DEF11AB200169ULL;
    o->tau_extract = 0.35f;
    cos_v169_fixture_50(o);
}

/* ------------------------------------------------------------- */
/* Extraction                                                    */
/* ------------------------------------------------------------- */

/* Parse the fixture text "[#NN] SUB PRED OBJ (auto)" into one
 * canonical triple + up to 1 inferred `mentions` triple if the
 * object also appears in the subject vocabulary.  σ_extraction
 * is derived from the SplitMix64 stream keyed by (seed, entry_id)
 * so the distribution is stable across runs. */
static int extract_from_entry(cos_v169_ontology_t *o,
                              const cos_v169_entry_t *e,
                              cos_v169_triple_t *out, int cap) {
    if (cap <= 0) return 0;
    /* Scan the text for the three words after the "[#NN] " prefix. */
    const char *p = e->text;
    /* skip "[#NN] " */
    while (*p && *p != ']') ++p;
    if (!*p) return 0;
    ++p;                                 /* skip ']' */
    while (*p == ' ') ++p;

    char sub[COS_V169_MAX_NAME] = {0};
    char prd[COS_V169_MAX_NAME] = {0};
    char obj[COS_V169_MAX_NAME] = {0};
    int fields = sscanf(p, "%47s %47s %47s", sub, prd, obj);
    if (fields != 3) return 0;

    uint64_t s = o->seed ^ 0x169E17ACULL ^ (uint64_t)e->id * 0x9E3779B97F4A7C15ULL;
    float sigma1 = clampf(0.05f + 0.45f * frand(&s), 0.0f, 1.0f);

    int written = 0;

    /* canonical triple */
    safe_copy(out[written].subject,   sizeof(out[written].subject),   sub);
    safe_copy(out[written].predicate, sizeof(out[written].predicate), prd);
    safe_copy(out[written].object,    sizeof(out[written].object),    obj);
    out[written].sigma_extraction = sigma1;
    out[written].source_entry_id  = e->id;
    out[written].kept             = sigma1 <= o->tau_extract;
    written++;
    if (written >= cap) return written;

    /* If the object is also a known subject-token, add a
     * "mentions" triple from sub to obj.  Its σ is slightly
     * higher (inferred) so ~55 % survive the gate. */
    for (int k = 0; k < N_SUB; ++k) {
        if (strcmp(obj, S_SUBJECTS[k]) == 0) {
            float sigma2 = clampf(0.10f + 0.40f * frand(&s), 0.0f, 1.0f);
            safe_copy(out[written].subject,   sizeof(out[written].subject),   sub);
            safe_copy(out[written].predicate, sizeof(out[written].predicate), "mentions");
            safe_copy(out[written].object,    sizeof(out[written].object),    obj);
            out[written].sigma_extraction = sigma2;
            out[written].source_entry_id  = e->id;
            out[written].kept             = sigma2 <= o->tau_extract;
            written++;
            break;
        }
    }
    return written;
}

int cos_v169_extract_all(cos_v169_ontology_t *o) {
    o->n_triples = 0;
    o->n_kept    = 0;
    for (int i = 0; i < o->n_entries; ++i) {
        if (o->n_triples >= COS_V169_MAX_TRIPLES) break;
        int n = extract_from_entry(o, &o->entries[i],
                                    &o->triples[o->n_triples],
                                    COS_V169_MAX_TRIPLES - o->n_triples);
        o->n_triples += n;
    }
    for (int i = 0; i < o->n_triples; ++i)
        if (o->triples[i].kept) o->n_kept++;
    return o->n_triples;
}

/* ------------------------------------------------------------- */
/* Typing                                                        */
/* ------------------------------------------------------------- */

static cos_v169_class_t classify(const char *name, float *out_sigma) {
    /* Simple prefix/exact classifier. */
    if (out_sigma) *out_sigma = 0.08f;
    if (strcmp(name, "lauri") == 0 || strcmp(name, "rene") == 0 ||
        strcmp(name, "mikko") == 0 || strcmp(name, "topias") == 0)
        return COS_V169_CLASS_PERSON;
    if (strcmp(name, "creation_os") == 0 || strcmp(name, "bitnet") == 0 ||
        strcmp(name, "cursor") == 0)
        return COS_V169_CLASS_SOFTWARE;
    if (strcmp(name, "sigma") == 0 || strcmp(name, "tau") == 0 ||
        strcmp(name, "entropy") == 0) {
        if (out_sigma) *out_sigma = 0.06f;
        return COS_V169_CLASS_METRIC;
    }
    if (name[0] == 'v' && name[1] >= '0' && name[1] <= '9') {
        if (out_sigma) *out_sigma = 0.05f;
        return COS_V169_CLASS_KERNEL;
    }
    if (strcmp(name, "rpi5") == 0 || strcmp(name, "jetson") == 0 ||
        strcmp(name, "macbook_m3") == 0) {
        if (out_sigma) *out_sigma = 0.07f;
        return COS_V169_CLASS_DEVICE;
    }
    /* Fallback: generic concept with higher σ (less confident) */
    if (out_sigma) *out_sigma = 0.22f;
    return COS_V169_CLASS_CONCEPT;
}

static int find_or_add_entity(cos_v169_ontology_t *o, const char *name) {
    for (int i = 0; i < o->n_entities; ++i)
        if (strcmp(o->entities[i].name, name) == 0) return i;
    if (o->n_entities >= COS_V169_MAX_ENTITIES) return -1;
    int idx = o->n_entities++;
    cos_v169_entity_t *ent = &o->entities[idx];
    memset(ent, 0, sizeof(*ent));
    safe_copy(ent->name, sizeof(ent->name), name);
    float sig = 0.0f;
    ent->cls = classify(name, &sig);
    ent->sigma_type = sig;
    ent->mention_count = 0;
    return idx;
}

int cos_v169_type_entities(cos_v169_ontology_t *o) {
    o->n_entities = 0;
    for (int i = 0; i < o->n_triples; ++i) {
        if (!o->triples[i].kept) continue;
        int si = find_or_add_entity(o, o->triples[i].subject);
        int oi = find_or_add_entity(o, o->triples[i].object);
        if (si >= 0) o->entities[si].mention_count++;
        if (oi >= 0) o->entities[oi].mention_count++;
    }
    return o->n_entities;
}

/* ------------------------------------------------------------- */
/* Query                                                         */
/* ------------------------------------------------------------- */

const cos_v169_entity_t *
cos_v169_entity_get(const cos_v169_ontology_t *o, const char *name) {
    if (!o || !name) return NULL;
    for (int i = 0; i < o->n_entities; ++i)
        if (strcmp(o->entities[i].name, name) == 0) return &o->entities[i];
    return NULL;
}

int cos_v169_query(const cos_v169_ontology_t *o,
                   const char *predicate, const char *object,
                   int *out_entry_ids, int cap) {
    if (!o) return -1;
    int hits = 0;
    /* Dedup via linear scan (entry_ids are small). */
    for (int i = 0; i < o->n_triples; ++i) {
        if (!o->triples[i].kept) continue;
        if (predicate && strcmp(o->triples[i].predicate, predicate) != 0) continue;
        if (object    && strcmp(o->triples[i].object,    object)    != 0) continue;
        int eid = o->triples[i].source_entry_id;
        int dup = 0;
        for (int j = 0; j < hits; ++j)
            if (out_entry_ids && out_entry_ids[j] == eid) { dup = 1; break; }
        if (dup) continue;
        if (out_entry_ids && hits < cap) out_entry_ids[hits] = eid;
        hits++;
    }
    return hits;
}

/* ------------------------------------------------------------- */
/* JSON / OWL-lite                                               */
/* ------------------------------------------------------------- */

size_t cos_v169_to_json(const cos_v169_ontology_t *o, char *buf, size_t cap) {
    if (!o || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
                     "{\"kernel\":\"v169\",\"n_entries\":%d,"
                     "\"n_triples\":%d,\"n_kept\":%d,\"n_entities\":%d,"
                     "\"tau_extract\":%.4f,\"triples\":[",
                     o->n_entries, o->n_triples, o->n_kept,
                     o->n_entities, (double)o->tau_extract);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < o->n_triples; ++i) {
        const cos_v169_triple_t *t = &o->triples[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"s\":\"%s\",\"p\":\"%s\",\"o\":\"%s\","
            "\"sigma\":%.4f,\"kept\":%s,\"src\":%d}",
            i == 0 ? "" : ",",
            t->subject, t->predicate, t->object,
            (double)t->sigma_extraction,
            t->kept ? "true" : "false", t->source_entry_id);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"entities\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < o->n_entities; ++i) {
        const cos_v169_entity_t *e = &o->entities[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"name\":\"%s\",\"class\":\"%s\",\"sigma_type\":%.4f,"
            "\"mentions\":%d}",
            i == 0 ? "" : ",",
            e->name, cos_v169_class_name(e->cls),
            (double)e->sigma_type, e->mention_count);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

size_t cos_v169_owl_lite_to_json(const cos_v169_ontology_t *o,
                                 char *buf, size_t cap) {
    /* OWL-lite-as-JSON: top-level classes + per-class members. */
    if (!o || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
                     "{\"kernel\":\"v169\",\"owl_lite_json\":true,"
                     "\"classes\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int c = 0; c < COS_V169_N_CLASSES; ++c) {
        n = snprintf(buf + used, cap - used,
                     "%s{\"class\":\"%s\",\"members\":[",
                     c == 0 ? "" : ",", cos_v169_class_name((cos_v169_class_t)c));
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
        int m = 0;
        for (int i = 0; i < o->n_entities; ++i) {
            if ((int)o->entities[i].cls != c) continue;
            n = snprintf(buf + used, cap - used,
                         "%s\"%s\"", m == 0 ? "" : ",",
                         o->entities[i].name);
            if (n < 0 || (size_t)n >= cap - used) return 0;
            used += (size_t)n;
            m++;
        }
        n = snprintf(buf + used, cap - used, "]}");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v169_self_test(void) {
    cos_v169_ontology_t o;
    cos_v169_init(&o, 0x169FACADE0169ULL);
    if (o.n_entries != 50) return 1;

    int n_triples = cos_v169_extract_all(&o);
    if (n_triples < 50) return 2;   /* ≥ 1 triple per entry */
    if (o.n_kept <= 0)   return 3;
    if (o.n_kept >= o.n_triples) return 4; /* gate dropped *some* */

    /* σ_extraction distribution should span a range */
    float smin = 1.0f, smax = 0.0f;
    for (int i = 0; i < o.n_triples; ++i) {
        float s = o.triples[i].sigma_extraction;
        if (s < smin) smin = s;
        if (s > smax) smax = s;
    }
    if (!(smax - smin > 0.1f)) return 5;

    int n_ent = cos_v169_type_entities(&o);
    if (n_ent < 6) return 6;

    /* lauri must be a Person */
    const cos_v169_entity_t *l = cos_v169_entity_get(&o, "lauri");
    if (!l || l->cls != COS_V169_CLASS_PERSON) return 7;
    /* creation_os must be Software */
    const cos_v169_entity_t *c = cos_v169_entity_get(&o, "creation_os");
    if (!c || c->cls != COS_V169_CLASS_SOFTWARE) return 8;
    /* sigma must be Metric */
    const cos_v169_entity_t *s = cos_v169_entity_get(&o, "sigma");
    if (!s || s->cls != COS_V169_CLASS_METRIC) return 9;

    /* Query: any kept triple with predicate="mentions" */
    int hits = cos_v169_query(&o, "mentions", NULL, NULL, 0);
    if (hits <= 0) return 10;

    /* Determinism: re-init + re-extract yields identical counts */
    cos_v169_ontology_t o2;
    cos_v169_init(&o2, 0x169FACADE0169ULL);
    cos_v169_extract_all(&o2);
    cos_v169_type_entities(&o2);
    if (o.n_triples  != o2.n_triples)  return 11;
    if (o.n_kept     != o2.n_kept)     return 12;
    if (o.n_entities != o2.n_entities) return 13;

    return 0;
}
