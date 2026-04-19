/*
 * v242 σ-Kernel-OS — operating-system abstraction for
 *   Creation OS: process / scheduler / IPC /
 *   filesystem / boot / shutdown.  v242 is what turns
 *   the name "Creation OS" into the real thing.
 *
 *   Process model (v0):
 *     Every kernel is a typed process.  The v0 fixture
 *     runs with 12 processes covering the core
 *     reasoning and identity kernels.  Each process
 *     carries a σ (confidence) value; the scheduler
 *     orders the ready queue by **lowest σ first**
 *     (higher priority for high-confidence work) and
 *     walks it deterministically — priority = rank
 *     in sorted σ order, ascending.
 *
 *   IPC mechanisms (v0 manifest, exactly three):
 *     MESSAGE_PASSING — typed envelopes between procs
 *     SHARED_MEMORY   — v115 SQLite as the shared tape
 *     SIGNALS         — v134 σ-spike as OS signal
 *
 *   Filesystem (v0 manifest, exactly five directories
 *   under ~/.creation-os/):
 *     models/  — weights, adapters         ("programs")
 *     memory/  — memories, ontology         ("data")
 *     config/  — TOML configuration
 *     audit/   — append-only audit chain
 *     skills/  — installed skill packs
 *
 *   Boot sequence (v0, exactly six ordered steps;
 *   any deviation is a gate failure):
 *     1. v29  σ-core
 *     2. v101 bridge
 *     3. v106 server
 *     4. v234 presence check
 *     5. v162 compose (profile load)
 *     6. v239 runtime (activation)
 *   After step 6, the OS is READY with a minimum
 *   five-kernel configuration (v29, v101, v106, v234,
 *   v162) active; further kernels load on demand via
 *   v239.
 *
 *   Shutdown sequence (v0, exactly three steps):
 *     1. v231 snapshot   (persist state)
 *     2. v181 audit      (log shutdown)
 *     3. v233 legacy     (update legacy)
 *
 *   σ_os (boot health):
 *       σ_os = boot_step_fail_count / n_boot_steps
 *     The v0 boot path must exit with σ_os == 0.0 and
 *     `state == READY`.
 *
 *   Contracts (v0):
 *     1. n_processes == 12; every process carries a
 *        σ ∈ [0, 1] and a unique kernel id.
 *     2. Scheduler priority is a dense permutation of
 *        [0, n_processes) and is exactly the argsort
 *        of σ ascending (ties broken by kernel id).
 *     3. Exactly three IPC mechanisms named verbatim:
 *        MESSAGE_PASSING, SHARED_MEMORY, SIGNALS.
 *     4. Exactly five filesystem dirs named verbatim
 *        (models, memory, config, audit, skills),
 *        each with path prefix `~/.creation-os/`.
 *     5. Boot sequence: exactly 6 steps, order
 *        v29 → v101 → v106 → v234 → v162 → v239.
 *     6. state == READY after boot.
 *     7. Minimum-active kernels after boot ≥ 5 and
 *        includes {29, 101, 106, 234, 162}.
 *     8. Shutdown: exactly 3 steps, order v231 → v181
 *        → v233; final state == STOPPED.
 *     9. σ_os ∈ [0, 1] AND σ_os == 0.0.
 *    10. FNV-1a chain replays byte-identically.
 *
 *   v242.1 (named, not implemented): real fork/exec
 *     hooks into v107 installer, POSIX signal bridge,
 *     userspace filesystem mount, cgroup / sandbox
 *     integration for v113.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V242_KERNEL_OS_H
#define COS_V242_KERNEL_OS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V242_N_PROCESSES   12
#define COS_V242_N_IPC          3
#define COS_V242_N_FS_DIRS      5
#define COS_V242_N_BOOT_STEPS   6
#define COS_V242_N_SHUTDOWN     3
#define COS_V242_MIN_READY_KERNELS 5

typedef enum {
    COS_V242_STATE_COLD    = 0,
    COS_V242_STATE_BOOTING = 1,
    COS_V242_STATE_READY   = 2,
    COS_V242_STATE_STOPPED = 3
} cos_v242_state_e;

typedef struct {
    int    kernel_id;
    char   name[24];
    float  sigma;
    int    priority;   /* 0 == highest (lowest σ) */
    bool   ready;
} cos_v242_proc_t;

typedef struct {
    char   name[32];   /* MESSAGE_PASSING, SHARED_MEMORY, SIGNALS */
    char   backing[48];
} cos_v242_ipc_t;

typedef struct {
    char   name[16];   /* models, memory, config, audit, skills */
    char   path[48];   /* "~/.creation-os/<name>/" */
} cos_v242_fsdir_t;

typedef struct {
    int    step;
    int    kernel_id;
    char   name[24];
    bool   ok;
} cos_v242_bootstep_t;

typedef struct {
    cos_v242_proc_t     procs  [COS_V242_N_PROCESSES];
    cos_v242_ipc_t      ipc    [COS_V242_N_IPC];
    cos_v242_fsdir_t    fsdirs [COS_V242_N_FS_DIRS];
    cos_v242_bootstep_t boot   [COS_V242_N_BOOT_STEPS];
    cos_v242_bootstep_t shutdown[COS_V242_N_SHUTDOWN];

    int                 n_ready_kernels;
    int                 boot_step_fail_count;
    float               sigma_os;
    cos_v242_state_e    state;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v242_state_t;

void   cos_v242_init(cos_v242_state_t *s, uint64_t seed);
void   cos_v242_run (cos_v242_state_t *s);

size_t cos_v242_to_json(const cos_v242_state_t *s,
                         char *buf, size_t cap);

int    cos_v242_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V242_KERNEL_OS_H */
