/*
 * Creation OS — real-time thread hint (Darwin mach).
 * thread_policy_set(THREAD_TIME_CONSTRAINT_POLICY) requests tighter scheduling; it does not
 * preempt the kernel or stop other processes. Values are ns-oriented hints; tune via env.
 */
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <stdlib.h>

extern "C" {

static void parse_u32(const char *s, uint32_t *out, uint32_t def) {
    if (!s || !*s) {
        *out = def;
        return;
    }
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (end == s)
        *out = def;
    else
        *out = (uint32_t)(v > 0xfffffffful ? 0xfffffffful : v);
}

void sk9_thread_rt_begin(void) {
    const char *en = getenv("CREATION_OS_RT_CONSTRAINT");
    if (!en || en[0] == '0')
        return;
    thread_port_t thr = pthread_mach_thread_np(pthread_self());
    uint32_t period = 1000000, computation = 100000, constraint = 500000;
    parse_u32(getenv("CREATION_OS_RT_PERIOD_NS"), &period, period);
    parse_u32(getenv("CREATION_OS_RT_COMPUTATION_NS"), &computation, computation);
    parse_u32(getenv("CREATION_OS_RT_CONSTRAINT_NS"), &constraint, constraint);
    thread_time_constraint_policy_data_t pol{};
    pol.period = period;
    pol.computation = computation;
    pol.constraint = constraint;
    pol.preemptible = TRUE;
    (void)thread_policy_set(thr, THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&pol,
                            THREAD_TIME_CONSTRAINT_POLICY_COUNT);
}

void sk9_thread_rt_end(void) {
    const char *en = getenv("CREATION_OS_RT_CONSTRAINT");
    if (!en || en[0] == '0')
        return;
    thread_port_t thr = pthread_mach_thread_np(pthread_self());
    thread_time_constraint_policy_data_t pol{};
    pol.period = 0;
    pol.computation = 0;
    pol.constraint = 0;
    pol.preemptible = TRUE;
    (void)thread_policy_set(thr, THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&pol,
                            THREAD_TIME_CONSTRAINT_POLICY_COUNT);
}

} /* extern "C" */
