/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v97 — σ-RL self-test driver.
 */

#include "rl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t pass_count = 0;
static uint32_t fail_count = 0;

#define CHECK(cond) do {                                                 \
    if (cond) { ++pass_count; }                                          \
    else {                                                               \
        ++fail_count;                                                    \
        if (fail_count <= 10u)                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                    \
} while (0)

static inline int32_t iabs(int32_t x) { return x < 0 ? -x : x; }

static void test_init(void)
{
    cos_v97_agent_t a, b;
    cos_v97_agent_init(&a, 0xC0FFEEULL);
    cos_v97_agent_init(&b, 0xC0FFEEULL);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V97_SENTINEL);
    CHECK(a.updates == 0u);
    CHECK(cos_v97_ok(&a) == 1u);
    for (uint32_t s = 0u; s < COS_V97_NSTATES; ++s)
        for (uint32_t i = 0u; i < COS_V97_NACTIONS; ++i)
            CHECK(a.Q[s][i] == 0);
}

/* A stationary MDP with only one non-zero reward: Bellman backup must
 * eventually converge the Q-value at the single reward-giving edge to
 * an ε-ball around r/(1−γ) for γ < 1, and it stays bounded. */
static void test_bellman_convergence(void)
{
    cos_v97_agent_t a;
    cos_v97_agent_init(&a, 0);
    const uint32_t goal_s = 3u;
    const uint32_t goal_a = 2u;
    cos_v97_q16_t  r      = (cos_v97_q16_t)(1 * COS_V97_Q1);       /* reward = 1.0 */

    /* Deterministic dynamics: goal transitions back to goal and stays there. */
    cos_v97_q16_t prev = 0;
    for (uint32_t step = 0u; step < 2000u; ++step) {
        cos_v97_q_update(&a, goal_s, goal_a, r, goal_s, 0u);
        cos_v97_q16_t cur = a.Q[goal_s][goal_a];
        CHECK(cur >= prev);                   /* monotonically non-decreasing */
        prev = cur;
    }
    /* Steady-state should approach r / (1 − γ) = 10.0 in Q16.16. */
    int32_t target = 10 * COS_V97_Q1;
    int32_t diff   = iabs((int32_t)a.Q[goal_s][goal_a] - target);
    CHECK(diff < (int32_t)(COS_V97_Q1 / 16));                   /* ≤ 0.0625 */
    CHECK(cos_v97_ok(&a) == 1u);
}

/* Greedy picks the argmax, tie-break on lowest index. */
static void test_greedy(void)
{
    cos_v97_agent_t a;
    cos_v97_agent_init(&a, 0);
    a.Q[0][0] = 1;
    a.Q[0][1] = 2;
    a.Q[0][2] = 3;
    a.Q[0][3] = 0;
    CHECK(cos_v97_greedy(&a, 0) == 2u);
    a.Q[0][3] = 4;
    CHECK(cos_v97_greedy(&a, 0) == 3u);
    a.Q[0][0] = 4;                           /* tie with a=3 */
    CHECK(cos_v97_greedy(&a, 0) == 0u);      /* lowest index wins */
}

/* GAE with constant δ collapses to geometric series  A_t = Σ (γλ)^k · δ. */
static void test_gae_constant_delta(void)
{
    const uint32_t T = 8u;
    cos_v97_q16_t rewards[8], values[9];
    /* Pick δ = r + γ·V' − V = 1.0  with V = 0 and r = 1 − γ·V' · (set V' = 0) */
    for (uint32_t t = 0u; t < T; ++t) rewards[t] = COS_V97_Q1;
    for (uint32_t t = 0u; t <= T; ++t) values[t]  = 0;

    cos_v97_q16_t A[8];
    cos_v97_gae(rewards, values, T, A);

    /* Last advantage = δ = 1.0 (Q16.16). */
    CHECK(A[T - 1] == COS_V97_Q1);
    /* Advantages at earlier t must be strictly larger (positive geom sum). */
    for (uint32_t t = 0u; t + 1u < T; ++t) {
        CHECK(A[t] > A[t + 1u]);
    }
}

/* PPO-clip envelope: inside the trust region [1−ε, 1+ε] it equals ρ·A.
 * Outside, it is flat at the clipped value on the side that hurts. */
static void test_ppo_clip(void)
{
    cos_v97_q16_t A_pos = (cos_v97_q16_t)COS_V97_Q1;
    cos_v97_q16_t A_neg = (cos_v97_q16_t)(-(int32_t)COS_V97_Q1);

    /* ρ = 1 → L_clip = A (unchanged). */
    CHECK(cos_v97_ppo_clip((cos_v97_q16_t)COS_V97_Q1, A_pos) == A_pos);
    CHECK(cos_v97_ppo_clip((cos_v97_q16_t)COS_V97_Q1, A_neg) == A_neg);

    /* A > 0 and ρ ≫ 1+ε: clipping must fire and cap the surrogate at
     * (1+ε)·A.   Here (1+ε)=1.1, A=1 → L_clip = 1.1 in Q16.16. */
    cos_v97_q16_t big_ratio = (cos_v97_q16_t)(2 * COS_V97_Q1);
    cos_v97_q16_t capped    = cos_v97_ppo_clip(big_ratio, A_pos);
    CHECK(capped == (cos_v97_q16_t)(COS_V97_Q1 + COS_V97_CLIP_EPS));

    /* A < 0 and ρ ≪ 1−ε: clipping must fire on the other side.
     *   (1−ε)·A = 0.9 · (−1) = −0.9 in Q16.16. */
    cos_v97_q16_t tiny_ratio = 0;
    cos_v97_q16_t floored    = cos_v97_ppo_clip(tiny_ratio, A_neg);
    CHECK(floored == (cos_v97_q16_t)(-(int32_t)(COS_V97_Q1 - COS_V97_CLIP_EPS)));

    /* Monotone in ρ ∈ [1−ε, 1+ε] when A > 0: sweep the band. */
    cos_v97_q16_t prev = (cos_v97_q16_t)(-2 * COS_V97_Q1);
    cos_v97_q16_t lo   = (cos_v97_q16_t)(COS_V97_Q1 - COS_V97_CLIP_EPS);
    cos_v97_q16_t hi   = (cos_v97_q16_t)(COS_V97_Q1 + COS_V97_CLIP_EPS);
    for (cos_v97_q16_t r = lo; r <= hi; r += 64) {
        cos_v97_q16_t v = cos_v97_ppo_clip(r, A_pos);
        CHECK(v >= prev);
        prev = v;
    }
}

/* Determinism under identical inputs. */
static void test_determinism(void)
{
    for (uint32_t trial = 0u; trial < 64u; ++trial) {
        cos_v97_agent_t a, b;
        cos_v97_agent_init(&a, 0);
        cos_v97_agent_init(&b, 0);
        for (uint32_t step = 0u; step < 256u; ++step) {
            uint32_t s = (trial + step) % COS_V97_NSTATES;
            uint32_t ac = (trial * 7u + step * 3u) % COS_V97_NACTIONS;
            cos_v97_q16_t r = (cos_v97_q16_t)((int32_t)((trial + step) & 0x7FFFu) - 0x4000);
            uint32_t sp = (s + 1u) % COS_V97_NSTATES;
            cos_v97_q_update(&a, s, ac, r, sp, 0u);
            cos_v97_q_update(&b, s, ac, r, sp, 0u);
        }
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    }
}

/* Q-table stays within the declared clamp band. */
static void test_clamp_bound(void)
{
    cos_v97_agent_t a;
    cos_v97_agent_init(&a, 0);
    cos_v97_q16_t huge_r = (cos_v97_q16_t)(10 * COS_V97_Q1);
    for (uint32_t step = 0u; step < 50000u; ++step) {
        uint32_t s  = step % COS_V97_NSTATES;
        uint32_t ac = step % COS_V97_NACTIONS;
        uint32_t sp = (s + 1u) % COS_V97_NSTATES;
        cos_v97_q_update(&a, s, ac, huge_r, sp, 0u);
    }
    for (uint32_t s = 0u; s < COS_V97_NSTATES; ++s) {
        for (uint32_t i = 0u; i < COS_V97_NACTIONS; ++i) {
            int32_t q = (int32_t)a.Q[s][i];
            CHECK(q >= -(int32_t)COS_V97_Q_CLAMP);
            CHECK(q <=  (int32_t)COS_V97_Q_CLAMP);
        }
    }
    CHECK(cos_v97_ok(&a) == 1u);
}

static void test_compose(void)
{
    CHECK(cos_v97_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v97_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v97_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v97_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v97 σ-RL — integer Q-learning + GAE + PPO-clip "
               "reinforcement-learning plane (37-bit composed decision)\n");
        return 0;
    }

    test_init();
    test_bellman_convergence();
    test_greedy();
    test_gae_constant_delta();
    test_ppo_clip();
    test_determinism();
    test_clamp_bound();
    test_compose();

    printf("v97 σ-RL: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
