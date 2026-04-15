# Security — developer-oriented notes (Creation OS)

**This is not** a full product threat model. It lists **abuse patterns** that maintainers should recognize when reviewing PRs and releases. User disclosure policy remains in [SECURITY.md](../SECURITY.md).

---

## 1. Epistemic / reputational attacks (documentation layer)

| Pattern | Risk | Mitigation in-repo |
|---------|------|---------------------|
| **Benchmark laundering** | New numbers in README without `make bench` log + host metadata | [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md), [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md), PR template |
| **Evidence-class smuggling** | Merging microbench and harness claims in one headline | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) forbidden merges, [COMMON_MISREADINGS.md](COMMON_MISREADINGS.md) |
| **Scope inflation** | “Toy demo” language upgraded to “production safety” without engineering | [ADVERSARIAL_REVIEW_CHECKLIST.md](ADVERSARIAL_REVIEW_CHECKLIST.md), README *Limitations* |

---

## 2. Build and supply chain (C / Makefile)

| Pattern | Risk | Mitigation |
|---------|------|------------|
| **Malicious compiler flags** in PRs | `-D` backdoors, unexpected includes | Code review; keep default `CFLAGS` conservative; CI logs flags indirectly via `make check` |
| **Obfuscated macro payloads** | Hard-to-audit `define` tricks | Reject unless justified; prefer explicit code |
| **Binary blobs in git** | Undocumented executables | Do not commit `*.dylib`, large zips, or weights (see README / CONTRIBUTING norms) |

---

## 3. Future networked wrappers (forward-looking)

If HTTP or RPC layers wrap `creation_os` or native dispatch:

- Treat **untrusted input** as hostile; rate-limit; validate sizes before HV fills.  
- **Do not** expose raw `system()` or shell bridges from receipt paths without hardening — out of scope for the standalone C file today, but relevant when extending.

---

## 4. Reporting

Follow [SECURITY.md](../SECURITY.md) for coordinated disclosure.

---

*Spektre Labs · Creation OS · 2026*
