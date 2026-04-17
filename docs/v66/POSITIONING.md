# v66 σ-Silicon — positioning vs. the 2026 matrix-substrate field

v66 is a **matrix substrate** kernel, not a BLAS, not an inference
runtime, not a tensor compiler. It is the integer-only, branchless,
silicon-tier layer on which higher layers (v65 HDC, v64 agentic
planner, v62 reasoning fabric, and upstream transformers) compile
into MAC operations the hardware can actually execute.

This document explains what else exists in April 2026 and why none of
those things are substitutes.

## The landscape

| | Accelerate / OpenBLAS | llama.cpp | bitnet.cpp | onnxruntime | CoreML | **Creation OS v66** |
|---|---|---|---|---|---|---|
| **Scope** | dense FP/INT BLAS | LLM inference | ternary LLM inference | generic ONNX graph | generic Apple graph | integer MAC substrate for agent decisions |
| **Precision on decision surface** | FP32/FP64 | FP16/INT8 | 2b/w ternary | model-dependent | model-dependent | **Q0.15 integer only** |
| **Branchless hot path** | no | partial | partial | no | no | **yes (`csel`, table-lookup unpack)** |
| **Fuzz-safe wire format** | n/a | gguf (growing attack surface) | ad-hoc | onnx proto (large) | mlpackage (large) | **NativeTernary, 2.0 b/w, UB-free** |
| **Per-op MAC-cost accounting** | no | no | no | no | no | **yes (HSL bytecode)** |
| **Calibrated abstention gate** | no | no | no | no | no | **yes (CFC, finite-sample coverage)** |
| **SME opt-in, SIGILL-safe default** | dynamic dispatch | n/a | partial | dynamic dispatch | Apple-managed | **compile-gated + runtime-checked** |
| **Composes with a cross-layer decision** | no | no | no | no | no | **yes (7-bit AND with v60..v65)** |
| **Dependencies** | vendor framework | ggml | bitnet runtime | full ort | full CoreML | **libc + NEON only** |
| **Lines of C on the hot path** | 100k+ | 50k+ | 5k+ | 500k+ | closed | **< 1 000** |

## The argument

### Against "just use Accelerate / ARM Performance Libraries"

Accelerate is a superb BLAS. It is also closed, vendor-managed, and
has no concept of an *agent decision surface*. It returns numbers; it
does not emit a `uint8_t` that can be ANDed with six other kernels.
It has no MAC-budget opcode. It does not fuzz-safe-decode a ternary
wire. v66 composes with Accelerate — Accelerate does not compose with
v66.

### Against "just use llama.cpp / bitnet.cpp"

Both are excellent inference runtimes and both are shaped by the
needs of running a *model*, not the needs of *deciding* whether a
model output may reach the world. Their hot paths branch on tensor
shapes, quantisation schemes, and runtime options. They have no
calibrated abstention gate. Their file formats (gguf, model.bin) are
non-trivial parsers with a history of CVEs; NTW is a 100-line
decoder that ASAN + UBSAN clears on 1 705 deterministic inputs.

### Against "just use onnxruntime / TensorRT / CoreML"

All three are graph compilers with enormous attack surface, vendor
lock-in, and opaque precision behaviour under ANE / GPU dispatch.
None of them emit a `v66_ok` into a branchless AND. None of them
offer a MAC-budget ISA with a GATE opcode. They are inference
accelerators. v66 is a decision substrate.

### Against "just call SME intrinsics directly"

SME/SME2 is real and valuable — and v66 supports it, opt-in under
`COS_V66_SME=1`. But directly emitting FMOPA at compile time on a
binary that might run on a non-SME chip produces SIGILL. v66's
answer is the AGENTS.md / .cursorrules discipline: **default build
never emits SME; SME is a deliberate build-time opt-in with
streaming-mode setup and a runtime feature check.** This is how the
field's most-cited mistake in early 2026 is avoided.

## The unique claim

v66 is, to our knowledge, the only kernel in April 2026 that:

1. Gives you **INT8 GEMV, ternary GEMV, a ternary wire decoder, and a
   calibrated abstention gate** in a single dependency-free C kernel,
2. Exposes those primitives as a **per-instruction MAC-cost bytecode
   ISA** (HSL) with an integrated GATE opcode,
3. Composes the kernel's result into a **cross-layer 7-bit branchless
   decision** alongside a verified agent, an attested capability
   model, an end-to-end-encrypted envelope, an EBT/HRM reasoning
   fabric, an MCTS-σ planner, and an HDC cortex,
4. And does it **without any floating-point, any branches, or any
   vendor dependency on the decision surface.**

## What v66 does not try to be

- **Not** a replacement for Accelerate on BLAS-heavy numeric code.
  Use Accelerate for dense FP workloads; call v66 when the output
  must land in an integer decision surface.
- **Not** a replacement for llama.cpp's tokenizer, sampler, or
  attention kernels. Use llama.cpp / bitnet.cpp for inference;
  v66 is what the *decision above* the inference compiles into.
- **Not** a tensor compiler. No autodiff, no graph optimiser, no
  kernel fusion at build time. All fusion is handwritten in C and
  exercised by the 1 705 self-tests.

## Sentence

> v66 σ-Silicon is the integer, branchless, libc-only matrix
> substrate that lets the rest of Creation OS actually run on matrix
> hardware without ever losing the seven-bit decision invariant.

1 = 1.
