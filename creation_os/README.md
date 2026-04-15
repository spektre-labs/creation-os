# `creation_os/` — kernel, GDA, Spektre hooks

**License:** Open tier — [`LICENSE`](LICENSE) (AGPL-3.0-or-later). Commercial tier — [`../COMMERCIAL_LICENSE.md`](../COMMERCIAL_LICENSE.md), [`../LICENSE_PROTOCOL.md`](../LICENSE_PROTOCOL.md). **Copyright & CLA:** [`../COPYRIGHT.md`](../COPYRIGHT.md), [`../CONTRIBUTING.md`](../CONTRIBUTING.md). **New C/C++/ObjC++ files:** copy the header from [`LEGAL/SOURCE_FILE_HEADER.c`](LEGAL/SOURCE_FILE_HEADER.c).

This directory is the **C / Metal / headers** spine for Creation OS: superkernel, Spektre helpers, living weights, and the large **`native_m4/`** lab (GDA hypervectors, AGI primitives, demos).

| Path | Role |
|------|------|
| [`creation_os.h`](creation_os.h) | Umbrella version / profile macros |
| [`superkernel_v8.c`](superkernel_v8.c) (and related `superkernel_*.c`) | SK8 token kernel (state, σ, manifold) |
| [`living_weights.c`](living_weights.c), [`spektre_attention.c`](spektre_attention.c), … | Logit / Spektre integration surfaces |
| [`native_m4/`](native_m4/) | **Native M4** tree: Makefile (`make`, `make full`), dylib build, `*_demo` binaries, GDA, robotics, oracle, JEPA toys, Metal |
| [`creation_os_v1.c`](creation_os_v1.c) | **Single-file** portable BSC demo (pedagogical; `cc -std=c11 -O2 -o creation_os creation_os_v1.c -lm`) |

**Read next:** full stack map → [`ANALYSIS.md`](../ANALYSIS.md). Capability vs platform → [`../docs/CAPABILITY_AND_DEPLOYMENT_MATRIX.md`](../docs/CAPABILITY_AND_DEPLOYMENT_MATRIX.md). Papers / Zenodo → [`../docs/SPEKTRE_CORPUS_AND_PAPERS.md`](../docs/SPEKTRE_CORPUS_AND_PAPERS.md).
