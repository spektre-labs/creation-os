<!-- SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
     SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
     Source:        https://github.com/spektre-labs/creation-os-kernel
     Website:       https://spektrelabs.org
     Commercial:    spektre.labs@proton.me
     License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt -->
# tests/v101 — σ-BitNet-Bridge

The σ-math self-tests live inside `src/v101/self_test.c` and are run by
`./creation_os_v101 --self-test` (wired into `make check-v101`).

## Pure-C unit tests (stub-safe)

Twenty assertions total.  They cover:

- uniform logits → all channels in [0, 1], σ near upper range
- single sharp peak → all channels near 0
- two equal peaks → margin ≈ 1, entropy small
- error handling for NULL / n_vocab < 2
- bit-for-bit determinism over the same input
- σ monotonicity: sharper peak → lower σ

Run:

```sh
make check-v101
```

Output on success:

```
v101 σ-BitNet-Bridge: 20 PASS / 0 FAIL
check-v101: OK (v101 σ-BitNet-Bridge σ-channel self-test; stub mode)
```

## Real-mode smoke (requires weights)

Real mode wraps microsoft/BitNet through bitnet.cpp.  Prereqs:

- `third_party/bitnet/build/bin/llama-cli` (built via `python setup_env.py -md models/BitNet-b1.58-2B-4T -q i2_s`)
- `models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf` (~1.1 GB)

Then:

```sh
make standalone-v101-real
make bench-v101-smoke
```

Expected output:

```json
{"text":" Paris. Paris is a city that is known for","sigma_profile":[...],"abstained":false}
```
