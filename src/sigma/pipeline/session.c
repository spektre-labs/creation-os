/* σ-Session primitive (turns + σ-priority eviction + trend + save/load). */

#include "session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void clamp_strcpy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

int cos_sigma_session_init(cos_session_t *s, cos_session_turn_t *storage,
                           int cap)
{
    if (!s || !storage || cap < 2) return -1;
    memset(s, 0, sizeof *s);
    memset(storage, 0, sizeof(cos_session_turn_t) * (size_t)cap);
    s->slots    = storage;
    s->cap      = cap;
    s->sigma_min = 1.0f;
    s->sigma_max = 0.0f;
    return 0;
}

static int worst_sigma_idx(const cos_session_t *s) {
    int worst = 0;
    for (int i = 1; i < s->count; ++i) {
        if (s->slots[i].sigma > s->slots[worst].sigma) worst = i;
    }
    return worst;
}

int cos_sigma_session_append(cos_session_t *s,
                             const char *prompt, const char *response,
                             float sigma, double cost_eur,
                             uint64_t t_ns, int escalated)
{
    if (!s || !s->slots) return -1;
    if (!(sigma >= 0.0f && sigma <= 1.0f)) return -2;

    int dst_idx;
    if (s->count < s->cap) {
        dst_idx = s->count++;
    } else {
        /* Evict the worst-σ turn *if* the incoming σ is lower than
         * the worst; otherwise refuse to degrade the ring. */
        int w = worst_sigma_idx(s);
        if (sigma >= s->slots[w].sigma) {
            /* Every live turn is more trustworthy than the new one. */
            return -3;
        }
        dst_idx = w;
        s->n_evicted++;
    }

    cos_session_turn_t *t = &s->slots[dst_idx];
    memset(t, 0, sizeof *t);
    clamp_strcpy(t->prompt,   sizeof t->prompt,   prompt);
    clamp_strcpy(t->response, sizeof t->response, response);
    t->sigma     = sigma;
    t->cost_eur  = cost_eur;
    t->t_ns      = t_ns;
    t->escalated = escalated ? 1 : 0;
    t->turn_id   = s->next_turn_id++;
    s->total_cost_eur += cost_eur;
    cos_sigma_session_recompute_trend(s);
    return 0;
}

int cos_sigma_session_size(const cos_session_t *s) {
    return s ? s->count : 0;
}

void cos_sigma_session_recompute_trend(cos_session_t *s) {
    if (!s || s->count <= 0) {
        if (s) { s->sigma_mean = 0.0f; s->sigma_slope = 0.0f;
                 s->sigma_min = 1.0f; s->sigma_max = 0.0f; }
        return;
    }
    double sum = 0.0;
    float mn = 1.0f, mx = 0.0f;
    for (int i = 0; i < s->count; ++i) {
        sum += (double)s->slots[i].sigma;
        if (s->slots[i].sigma < mn) mn = s->slots[i].sigma;
        if (s->slots[i].sigma > mx) mx = s->slots[i].sigma;
    }
    s->sigma_mean = (float)(sum / (double)s->count);
    s->sigma_min  = mn;
    s->sigma_max  = mx;
    /* Slope across turn_id: compare last half vs first half. */
    if (s->count >= 2) {
        /* Sort a scratch of (turn_id, sigma) by turn_id so eviction
         * reshuffles don't corrupt slope. */
        float ordered[256]; /* cap-bounded; real usage fits */
        int n = s->count < 256 ? s->count : 256;
        int by_id[256];
        for (int i = 0; i < n; ++i) by_id[i] = i;
        for (int i = 1; i < n; ++i) {
            for (int j = i; j > 0; --j) {
                if (s->slots[by_id[j-1]].turn_id > s->slots[by_id[j]].turn_id) {
                    int t = by_id[j-1]; by_id[j-1] = by_id[j]; by_id[j] = t;
                } else break;
            }
        }
        for (int i = 0; i < n; ++i) ordered[i] = s->slots[by_id[i]].sigma;
        int half = n / 2;
        double first = 0.0, last = 0.0;
        for (int i = 0; i < half; ++i) first += (double)ordered[i];
        for (int i = n - half; i < n; ++i) last += (double)ordered[i];
        first /= (double)half;
        last  /= (double)half;
        s->sigma_slope = (float)(last - first);
    } else {
        s->sigma_slope = 0.0f;
    }
}

int cos_sigma_session_is_drifting(const cos_session_t *s,
                                  float slope_thresh, int min_count)
{
    if (!s || s->count < min_count) return 0;
    return (s->sigma_slope > slope_thresh) ? 1 : 0;
}

/* --- save / load ------------------------------------------------ */

static void write_escaped(FILE *out, const char *s) {
    if (!s) return;
    for (const char *p = s; *p; ++p) {
        if (*p == '\\')      fputs("\\\\", out);
        else if (*p == '\n') fputs("\\n",  out);
        else                 fputc(*p, out);
    }
}

static int read_escaped_line(FILE *in, char *dst, size_t cap) {
    if (!in || !dst || cap == 0) return -1;
    size_t o = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (c == '\n') break;
        if (c == '\\') {
            int c2 = fgetc(in);
            if (c2 == 'n')  { if (o + 1 < cap) dst[o++] = '\n'; }
            else if (c2 == '\\') { if (o + 1 < cap) dst[o++] = '\\'; }
            else if (c2 == EOF) break;
            else if (o + 1 < cap) dst[o++] = (char)c2;
        } else {
            if (o + 1 < cap) dst[o++] = (char)c;
        }
    }
    dst[o < cap ? o : cap - 1] = '\0';
    if (c == EOF && o == 0) return -1;
    return (int)o;
}

static uint64_t fnv64(const char *s) {
    uint64_t h = 14695981039346656037ULL;
    for (; *s; ++s) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
    return h;
}

int cos_sigma_session_save(const cos_session_t *s, FILE *out) {
    if (!s || !out) return -1;
    uint64_t cs = fnv64("COS-SESSION-1");
    for (int i = 0; i < s->count; ++i) {
        const cos_session_turn_t *t = &s->slots[i];
        cs ^= (uint64_t)t->turn_id;
        cs *= 1099511628211ULL;
        cs ^= fnv64(t->prompt);
        cs ^= fnv64(t->response);
    }
    fprintf(out, "COS-SESSION 1 %d %d %d %.6f %016llx\n",
            s->count, s->next_turn_id, s->n_evicted,
            s->total_cost_eur, (unsigned long long)cs);
    for (int i = 0; i < s->count; ++i) {
        const cos_session_turn_t *t = &s->slots[i];
        fprintf(out, "T %d %d %.6f %.6f %llu\n",
                t->turn_id, t->escalated,
                (double)t->sigma, t->cost_eur,
                (unsigned long long)t->t_ns);
        fputs("P ", out); write_escaped(out, t->prompt);   fputc('\n', out);
        fputs("R ", out); write_escaped(out, t->response); fputc('\n', out);
    }
    return 0;
}

int cos_sigma_session_load(cos_session_t *s, cos_session_turn_t *storage,
                           int cap, FILE *in)
{
    if (!s || !storage || !in || cap < 2) return -1;
    cos_sigma_session_init(s, storage, cap);

    char hdr[256];
    if (!fgets(hdr, sizeof hdr, in)) return -2;
    int header_count, next_id, n_evicted;
    double total_cost;
    unsigned long long cs_in = 0;
    if (sscanf(hdr, "COS-SESSION 1 %d %d %d %lf %llx",
               &header_count, &next_id, &n_evicted,
               &total_cost, &cs_in) != 5) return -3;
    if (header_count < 0 || header_count > cap) return -4;

    for (int i = 0; i < header_count; ++i) {
        int turn_id, escalated;
        float sigma;
        double cost;
        unsigned long long t_ns;
        char line[256];
        if (!fgets(line, sizeof line, in)) return -5;
        if (sscanf(line, "T %d %d %f %lf %llu", &turn_id, &escalated,
                   &sigma, &cost, &t_ns) != 5) return -6;
        char tag;
        if (fscanf(in, "%c ", &tag) != 1 || tag != 'P') return -7;
        cos_session_turn_t *t = &storage[i];
        memset(t, 0, sizeof *t);
        read_escaped_line(in, t->prompt,   sizeof t->prompt);
        if (fscanf(in, "%c ", &tag) != 1 || tag != 'R') return -8;
        read_escaped_line(in, t->response, sizeof t->response);
        t->turn_id   = turn_id;
        t->escalated = escalated;
        t->sigma     = sigma;
        t->cost_eur  = cost;
        t->t_ns      = (uint64_t)t_ns;
    }
    s->count        = header_count;
    s->next_turn_id = next_id;
    s->n_evicted    = n_evicted;
    s->total_cost_eur = total_cost;

    /* Checksum check. */
    uint64_t cs = fnv64("COS-SESSION-1");
    for (int i = 0; i < s->count; ++i) {
        cs ^= (uint64_t)s->slots[i].turn_id;
        cs *= 1099511628211ULL;
        cs ^= fnv64(s->slots[i].prompt);
        cs ^= fnv64(s->slots[i].response);
    }
    if (cs != (uint64_t)cs_in) return -9;

    cos_sigma_session_recompute_trend(s);
    return 0;
}

/* ---------------- self-test ------------------------------------ */

static int check_init_guards(void) {
    cos_session_t s;
    cos_session_turn_t slots[4];
    if (cos_sigma_session_init(NULL, slots, 4) == 0) return 10;
    if (cos_sigma_session_init(&s, NULL, 4)   == 0) return 11;
    if (cos_sigma_session_init(&s, slots, 1)  == 0) return 12;
    if (cos_sigma_session_init(&s, slots, 4)  != 0) return 13;
    if (cos_sigma_session_append(&s, "p", "r", 1.5f, 0.0, 0, 0) != -2) return 14;
    return 0;
}

/* Tiny local fabsf to avoid pulling math.h for one call. */
static float fabsf_(float x) { return (x < 0.0f) ? -x : x; }

static int check_append_and_trend(void) {
    cos_session_t s;
    cos_session_turn_t slots[4];
    cos_sigma_session_init(&s, slots, 4);
    float sigs[] = {0.10f, 0.15f, 0.05f, 0.12f};
    for (int i = 0; i < 4; ++i)
        if (cos_sigma_session_append(&s, "q", "a", sigs[i], 0.001,
                                     (uint64_t)(i + 1), 0) != 0) return 20;
    if (s.count != 4)                         return 21;
    if (fabsf_(s.sigma_mean - 0.105f) > 1e-3f) return 22;
    return 0;
}

static int check_sigma_priority_evict(void) {
    cos_session_t s;
    cos_session_turn_t slots[4];
    cos_sigma_session_init(&s, slots, 4);
    /* Fill with high-σ turns, then push low-σ turns — they evict
     * the worst (highest σ). */
    float sigs[] = {0.80f, 0.60f, 0.40f, 0.20f};
    for (int i = 0; i < 4; ++i)
        cos_sigma_session_append(&s, "q", "a", sigs[i], 0.0,
                                 (uint64_t)(i + 1), 0);
    int rc = cos_sigma_session_append(&s, "q", "a", 0.10f, 0.0, 99, 0);
    if (rc != 0)                             return 30;
    if (s.n_evicted != 1)                    return 31;
    /* 0.80 should have been evicted; remaining max σ = 0.60. */
    float mx = 0.0f;
    for (int i = 0; i < s.count; ++i) if (s.slots[i].sigma > mx) mx = s.slots[i].sigma;
    if (fabsf_(mx - 0.60f) > 1e-5f)          return 32;

    /* A high-σ turn when everything is low-σ should be refused. */
    rc = cos_sigma_session_append(&s, "q", "a", 0.90f, 0.0, 100, 0);
    if (rc != -3)                            return 33;
    return 0;
}

static int check_drift(void) {
    cos_session_t s;
    cos_session_turn_t slots[8];
    cos_sigma_session_init(&s, slots, 8);
    /* Drifting up: [0.05, 0.05, 0.05, 0.05, 0.40, 0.45, 0.50, 0.55]
     * first half mean = 0.05, last half mean = 0.475, slope = 0.425. */
    float up[] = {0.05f, 0.05f, 0.05f, 0.05f, 0.40f, 0.45f, 0.50f, 0.55f};
    for (int i = 0; i < 8; ++i)
        cos_sigma_session_append(&s, "q", "a", up[i], 0.0, (uint64_t)i, 0);
    if (!cos_sigma_session_is_drifting(&s, 0.20f, 4)) return 40;
    if (cos_sigma_session_is_drifting(&s, 0.50f, 4))  return 41;
    /* Not drifting if n < min_count. */
    cos_session_t s2;
    cos_session_turn_t slots2[4];
    cos_sigma_session_init(&s2, slots2, 4);
    cos_sigma_session_append(&s2, "q", "a", 0.05f, 0.0, 1, 0);
    cos_sigma_session_append(&s2, "q", "a", 0.55f, 0.0, 2, 0);
    if (cos_sigma_session_is_drifting(&s2, 0.20f, 4)) return 42;
    return 0;
}

static int check_save_load_roundtrip(void) {
    cos_session_t s;
    cos_session_turn_t slots[4];
    cos_sigma_session_init(&s, slots, 4);
    cos_sigma_session_append(&s, "hello\nworld", "response with \\ slash",
                             0.12f, 0.001, 42, 0);
    cos_sigma_session_append(&s, "next prompt", "next response",
                             0.08f, 0.002, 43, 1);

    FILE *tmp = tmpfile();
    if (!tmp) return 50;
    if (cos_sigma_session_save(&s, tmp) != 0) { fclose(tmp); return 51; }
    rewind(tmp);

    cos_session_t s2;
    cos_session_turn_t slots2[4];
    if (cos_sigma_session_load(&s2, slots2, 4, tmp) != 0) { fclose(tmp); return 52; }
    fclose(tmp);

    if (s2.count != 2)                                 return 53;
    if (strcmp(s2.slots[0].prompt, "hello\nworld") != 0)    return 54;
    if (strcmp(s2.slots[0].response, "response with \\ slash") != 0) return 55;
    if (s2.slots[0].turn_id  != 0)                     return 56;
    if (s2.slots[1].escalated != 1)                    return 57;
    if (s2.next_turn_id != 2)                          return 58;
    if (fabsf_(s2.slots[1].sigma - 0.08f) > 1e-5f)     return 59;
    return 0;
}

int cos_sigma_session_self_test(void) {
    int rc;
    if ((rc = check_init_guards()))          return rc;
    if ((rc = check_append_and_trend()))     return rc;
    if ((rc = check_sigma_priority_evict())) return rc;
    if ((rc = check_drift()))                return rc;
    if ((rc = check_save_load_roundtrip()))  return rc;
    return 0;
}
