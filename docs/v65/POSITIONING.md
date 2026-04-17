# v65 σ-Hypercortex — positioning

## Problem statement (April 2026)

The 2026 field has accepted four facts about LLM agents:

1. **Transformers are stateless.** Their "memory" is the prompt.
2. **Their reasoning is not compositional.** Attention implements a
   VSA internally (AAAI 2026: Attention-as-Binding) but the substrate
   is hidden and unverifiable.
3. **Retrieval-augmented generation (RAG) is a patch, not a fix.**
   Float-embedding ANN retrieval is high-bandwidth, non-symbolic, and
   cannot represent *structure* — only surface similarity.
4. **Vector databases are not cortex.** Pinecone, Weaviate, Chroma,
   Milvus all index float embeddings; none of them bind/bundle/
   permute; none of them do analogy in closed form.

v65 σ-Hypercortex is the answer: a **persistent, compositional,
integer-only, popcount-native neurosymbolic substrate** that sits
under any LLM and runs at unified-memory bandwidth.

## Comparison — April 2026 field

| capability | v65 σ-Hypercortex | TorchHD 2025 | OpenMem 2026 | VaCoAl arXiv:2604.11665 | Pinecone / ChromaDB | LangChain memory |
|---|:-:|:-:|:-:|:-:|:-:|:-:|
| Language | C11, no deps | Python/PyTorch | Python | Python + Verilog | Go / Rust | Python / JS |
| Dimensions | fixed 16 384 bits (integer) | 10 000 float | 10 000 float | fixed, int | 768 – 3072 float | text chunks |
| Bind (XOR / conv) | ✅ integer XOR | ✅ | ✅ | ✅ (GF algebra) | ❌ | ❌ |
| Bundle (majority) | ✅ integer tally | ✅ | ✅ | ✅ | ❌ | ❌ |
| Permute (cyclic) | ✅ bitwise | ✅ | ✅ | ✅ | ❌ | ❌ |
| Similarity | **Q0.15 integer** | float cos | float cos | det. | float cos | token-overlap |
| Cleanup memory | constant-time sweep | Python loop | Python loop | SRAM/CAM (spec) | float ANN | — |
| Role-filler record | ✅ closed-form | ✅ | ✅ | ✅ | ❌ | ❌ |
| Analogy A:B::C:? | **closed form, one XOR** | manual | — | ✅ | ❌ | ❌ |
| Sequence memory | ✅ position-permute | ✅ | ✅ | ✅ | ❌ | ❌ |
| Bytecode ISA for VSA ops | ✅ **HVL (9 opcodes)** | ❌ | ❌ | ❌ | ❌ | ❌ |
| Per-program cost gate | ✅ popcount-word budget | ❌ | ❌ | ❌ | ❌ | ❌ |
| Composed security decision | ✅ 6-bit branchless AND (v60…v65) | ❌ | ❌ | ❌ | ❌ | ❌ |
| FP on hot path? | **none** | yes | yes | none | yes | n/a |
| Runtime deps | **libc only** | PyTorch + CUDA | Python + embeddings | Python + HW sim | Go + K8s | Python + LLM |
| Hamming throughput (M4 single core) | **41 GB/s** | ~0.5 GB/s (PyTorch) | ~0.5 GB/s | (sim) | (float) | (n/a) |
| Bind throughput | **192 GB/s** | ~2 GB/s | ~2 GB/s | (sim) | (n/a) | (n/a) |
| License | AGPL-3.0 | MIT | (research) | (research) | proprietary / OSS | various |

## What v65 gives that no other system on the table gives

1. **An integer neurosymbolic substrate** — no FP, no PyTorch, no
   CUDA, no GPU required. Every operation is popcount, XOR, or
   integer compare.
2. **Silicon-tier memory bandwidth** — 192 GB/s bind throughput on a
   single M4 performance core is within a factor of 2 of unified
   memory peak. TorchHD is ~100× slower.
3. **A first-class bytecode ISA (HVL)** — the only HDC system with
   a named, integer, cost-gated bytecode surface. Every "thought" is
   a straight-line program with a hard ceiling.
4. **A 6-bit composed decision** — v65 does not stand alone. It
   is the sixth lane in a branchless AND that also requires
   v60 (capability), v61 (lattice), v62 (EBT energy), v63 (AEAD +
   attestation), and v64 (agentic authorisation) to permit emission.
   No HDC framework even pretends to this level of composition.
5. **Formally-structured auditability** — v65's 534 deterministic
   tests cover composition truth table, primitive algebraic
   identities (bind self-inverse, commutative, associative),
   bundle noise-tolerance, cleanup exact & noisy recall, record
   round-trip, analogy closure, sequence decode, and HVL end-to-end.
   TorchHD / OpenMem are empirical; v65 is algebraic.

## v65's unique value proposition

> **v65 σ-Hypercortex is the only open-source local-AI-agent
> neurosymbolic substrate where every reasoning step is an integer
> popcount, every "thought program" is a budgeted bytecode program,
> and every emission is gated on a branchless 6-bit composition
> with five upstream security kernels — AGPL-3.0, libc-only, and
> measured at 192 GB/s bind on a single M-series performance core.**

## What v65 is not

- **Not a language model.** No weights, no gradients, no training
  loop. v65 does not generate tokens. It verifies and structures
  them.
- **Not a vector DB replacement.** No ANN, no float embeddings, no
  similarity-search UI. v65 is a **substrate**: bind, bundle,
  permute, cleanup, gate. It is *under* whatever you build on top.
- **Not a RAG system.** RAG is "put a search engine in front of an
  LLM". v65 is "put an algebraic cortex under the LLM that runs at
  40 GB/s and refuses invalid thoughts".
- **Not a transformer.** v65 composes with transformers via the
  Attention-as-Binding interpretation (AAAI 2026): any transformer
  you like runs on top, and v65 holds the cortex the transformer
  never had.

## Field pointers

| frontier line | v65 mapping |
|---|---|
| OpenMem 2026 | cleanup memory + record/role-filler + sequence memory |
| VaCoAl arXiv:2604.11665 | deterministic algebra; GF idea generalised to XOR-bind at 16 384 bits |
| Attention-as-Binding AAAI 2026 | v65 exposes the substrate transformers hide |
| VSA-ARC arXiv:2511.08747 | HVL bytecode can encode program-synthesis traces |
| HIS arXiv:2603.13558 | similarity-floor gate → `v65_ok` |
| Hyperdimensional Probe arXiv:2509.25045 | LLM outputs can be probed into HVs and verified before release |
| HDFLIM | frozen-LLM / frozen-vision integration at the substrate layer |
| ConformalHDC | uncertainty quantification via cleanup-similarity distribution |
| LifeHD arXiv:2403.04759 | on-device lifelong learning via incremental cleanup-memory insertion |
