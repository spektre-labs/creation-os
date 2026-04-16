# v33 agent harness (synthetic 50-step router + schema loop)

Host: Darwin Mac 24.6.0 Darwin Kernel Version 24.6.0: Wed Jan  7 21:18:30 PST 2026; root:xnu-11417.140.69.708.2~1/RELEASE_ARM64_T8122 arm64
Date: 2026-04-16T16:32:13Z

This harness exercises `creation_os_v33 --bench-agent` with three routing policies.
CPS (€/successful task) is not computed when all models are local cost=0; see SUMMARY lines for rates.

## Mode: bitnet-only
```
SUMMARY mode=bitnet-only tasks=50 primary=7 fallback=0 abstain=43 schema_rate=1.000 exec_rate=0.140 abstain_rate=0.860 p50_e2e_ms=0.032 metrics_path=/Users/eliaslorenzo/Desktop/creation-os-kernel/benchmarks/v33/metrics_run/session_1776357133_312071000.jsonl
```

## Mode: bitnet+fallback
```
SUMMARY mode=bitnet+fallback tasks=50 primary=7 fallback=8 abstain=35 schema_rate=1.000 exec_rate=0.300 abstain_rate=0.700 p50_e2e_ms=0.030 metrics_path=/Users/eliaslorenzo/Desktop/creation-os-kernel/benchmarks/v33/metrics_run/session_1776357133_316245000.jsonl
```

## Mode: fallback-only
```
SUMMARY mode=fallback-only tasks=50 primary=0 fallback=50 abstain=0 schema_rate=1.000 exec_rate=1.000 abstain_rate=0.000 p50_e2e_ms=0.031 metrics_path=/Users/eliaslorenzo/Desktop/creation-os-kernel/benchmarks/v33/metrics_run/session_1776357133_320619000.jsonl
```

