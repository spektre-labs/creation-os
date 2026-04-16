// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Creation OS v61 — example eBPF policy (Linux 6.x LSM BPF).
//
// This tracepoint blocks any attempt by the agent process to invoke
// `dlopen`/`execve` unless σ-Shield has stamped a σ-key into a pinned
// BPF map (`/sys/fs/bpf/sigma_shield_keys`).  The kernel enforces the
// deny; userspace only decides when to publish a key.
//
// Tier claim:
//   - I (interpreted): the example in this file.
//   - P (planned): Shield userspace publishes σ-keys per authorised
//     action.  Not yet wired.
//
// Build (Linux only; no macOS path):
//   clang -O2 -target bpf -Wall -c ebpf/sigma_shield.bpf.c -o sigma_shield.bpf.o
//   sudo bpftool prog loadall sigma_shield.bpf.o /sys/fs/bpf/sigma_shield
//
// On macOS `make ebpf-policy` reports SKIP.

#ifdef __linux__

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key,   __u64);   // sigma key
    __type(value, __u32);   // 1 = allowed
} sigma_shield_keys SEC(".maps");

SEC("lsm/bprm_check_security")
int BPF_PROG(block_execve, struct linux_binprm *bprm, int ret)
{
    if (ret != 0) return ret;
    // Accept only if σ-Shield has published the calling PID tag.
    __u64 key = bpf_get_current_pid_tgid();
    __u32 *v = bpf_map_lookup_elem(&sigma_shield_keys, &key);
    if (!v || *v != 1) {
        return -1; // EPERM
    }
    return 0;
}

char _license[] SEC("license") = "GPL";

#else

/* Non-Linux build: empty translation unit.  `make ebpf-policy` SKIPs. */
int sigma_shield_bpf_placeholder(void) { return 0; }

#endif
