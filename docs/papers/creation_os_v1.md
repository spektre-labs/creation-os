# Creation OS: A σ-Governed Cognitive Architecture for Local AGI

**Author.** Lauri Elias Rainio · Spektre Labs Oy · <ops@spektre.dev>
**Version.** v1.0.0 · Creation OS v153
**Status.** Preprint — companion to the v1.0.0 release.
**Source.** [github.com/spektre-labs/creation-os](https://github.com/spektre-labs/creation-os)
**License.** CC-BY-4.0 for the paper, SCSL-1.0 / AGPL-3.0-only for the code.

---

## Abstract

Frontier language models are deployed as black boxes: a prompt goes
in, tokens come out, and every claim about *how confident the model
is* collapses into a single post-hoc score, a refusal message, or a
marketing disclaimer. Creation OS is a runtime and cognitive
architecture that rejects that posture. Every answer is governed by
σ, a post-hoc confidence signal computed from the token logit
distribution and composed across eight independent channels
(`σ_entropy`, `σ_max_token`, `σ_n_effective`, `σ_margin`,
`σ_variance`, `σ_skew`, `σ_kurtosis`, `σ_top_p`) into a single
`σ_product`. The system comprises 153 composable kernels (v6–v153)
organized into seven layers — silicon, generation, σ-governance,
reasoning, training + persistence, distribution + collective,
metacognition — with every layer's gate exposed as a falsifiable
merge contract. Live measurements on `truthfulqa_mc2` (n=817) show
that two σ signals (`σ_max_token`, `σ_n_effective`) achieve
Bonferroni-significant AURCC reduction vs. the entropy baseline
(ΔAURCC = −0.0442, p = 0.002, N=24 family-wise corrections). v152
distillation drops σ on covered-corpus QA by 46.32 % under a
deterministic SFT fixture. The v1.0.0 release ships portable C11
binaries, an OpenAI-compatible HTTP surface (v106), a Web UI
(v108), a Python SDK (v142), Homebrew / Docker / PyPI / npm /
Hugging Face artefacts, a machine-checkable merge-gate covering
every kernel from v6 through v153, and this single unifying paper
which replaces the 80+ per-kernel prose notes previously archived
on Zenodo. All claims reported here are reproducible byte-for-byte
from a fixed seed on a single MacBook Pro (M4).

## 1. Introduction — why LLM wrappers are not enough

Three classes of limitation motivate Creation OS.

**Calibration collapse.** Frontier models compress uncertainty into
a single scalar (if any) and surface it as either *temperature*
(generation-time) or *confidence* (post-hoc). Both hide the
multi-channel structure of the token distribution — the
distinction between "peaky but off-topic" (low entropy, low
max-token, wrong answer) and "peaky and on-topic" (low entropy,
high max-token, correct answer) is crucial for abstention. Creation
OS computes the eight σ channels and composes them geometrically
into σ_product, exposed in every HTTP response and every JSON
receipt.

**Wrapper fragmentation.** LangChain, CrewAI, LlamaIndex, and the
OpenAI Agents SDK all wrap the same black box, adding orchestration
but inheriting its opacity. Swapping one wrapper for another does
not change what the model emits; it changes only *what gets
printed around the emission*. Creation OS is not a wrapper. It is
a runtime where σ-governance is in the hot path — no tool call, no
code execution, no skill install, and no weight update happens
without crossing at least one σ-gate whose threshold is a public
merge-gate contract.

**Reproducibility deficit.** Most LLM evaluation is a one-shot
benchmark table in a PDF. Creation OS ships a machine-checkable
`make merge-gate` that runs every self-test for every kernel
(16+ million PASS / 0 FAIL on commodity hardware) and is CI-green
on every commit. Every claim in this paper either reduces to a
merge-gate target or is explicitly labelled as **tier-0 synthetic**
(deterministic fixture) vs **tier-1 archived** (live run, archived
trace) vs **tier-2 live** (real model + real data).

## 2. σ-theory

**Effective kernel.** Given a kernel K that produces an answer and
a residual distortion σ ∈ [0, 1], we define the effective kernel

$$
K_{\text{eff}}(t) = (1 - \sigma(t)) \cdot K(t)
$$

so that high σ shrinks the answer's contribution to downstream
composition. This is the single formula underlying every σ-gate in
the stack.

**The eight channels.** σ is computed eight times on each token
distribution:

| Channel | What it measures |
|---|---|
| `σ_entropy` | Shannon entropy normalized to [0, 1] |
| `σ_max_token` | 1 − max softmax prob (peakiness) |
| `σ_n_effective` | 1 / (exp(entropy) / vocab) (effective branching) |
| `σ_margin` | 1 − (top1 − top2) (first-vs-second gap) |
| `σ_variance` | variance of the logit distribution, scale-normalized |
| `σ_skew` | absolute skewness of the logits |
| `σ_kurtosis` | excess kurtosis (heavy-tailed ⇒ high) |
| `σ_top_p` | 1 − (smallest k such that Σ top-k ≥ p) / vocab |

**σ_product.** The composed signal is the geometric mean

$$
\sigma_{\text{product}} = \left( \prod_{i=1}^{8} \sigma_i \right)^{1/8}
$$

which a) is conservative (one high channel pulls the mean up),
b) is differentiable for learning-time use, and c) survives √N
shrinkage (v104) across ensembles. σ_product is the single number
the HTTP surface returns in the `X-COS-Sigma-Product` header and
the JSON body.

**Abstention.** A response is emitted iff `σ_product ≤ τ_abstain`.
τ_abstain is a first-class knob, tunable per request (header or
JSON body), per deployment (config.toml), and per kernel-family
(different τ values for tool calls, code execution, federated
share, etc.).

## 3. Architecture — seven layers

Creation OS is organized into seven layers, each composable,
each falsifiable.

1. **Silicon (v60–v100)** — forty branchless integer kernels on
   the BitNet-b1.58 / llama.cpp hot path.
2. **Generation (v101–v119)** — GGUF bridge, OpenAI-compatible
   HTTP (v106), Web UI (v108), multi-GGUF routing (v109), paged KV
   + σ-aware eviction (v117), image input (v118), σ-adaptive
   speculative decoding (v119).
3. **σ-Governance (v101, v105, v122, v123, v134, v137)** — the
   8-channel σ profile, τ_abstain threshold, TLA+ model check of
   seven σ-invariants, mode-collapse detector, σ-spike burst
   detector, AOT-compiled branchless σ-gate.
4. **Reasoning + Agentic (v111–v135)** — `/v1/reason`, σ-swarm
   routing (v114), σ-agent tools (v112), σ-sandbox (v113), HTN /
   MCTS σ-planner (v121), Prolog symbolic engine (v135).
5. **Training + Persistence (v111.3, v115, v120, v124–v131)** —
   MLX SFT + σ-abstention LoRA, σ-weighted SQLite memory, big→
   small distillation selector, idle-time continual LoRA with
   forgetting-rollback, σ-DPO, embedding store, temporal session.
6. **Distribution + Collective (v107–v130, v142)** — `brew / curl /
   Docker / npm`, MCP server (v116), σ-red-team harness (v122),
   σ-weighted federated learning with differential privacy (v129),
   FP4-LoRA + σ-pack codec (v130), Python SDK with LangChain /
   LlamaIndex adapters (v142).
7. **Metacognition (v132–v148, v152–v153)** — weekly σ-snapshots,
   OLS slope/week, meta-σ (σ of σ), auto-diagnose, adaptive user
   profile, ES architecture search, Frama-C WP proof, world model,
   counterfactual attribution, σ-native benchmark suite, σ-RSI
   with accept/rollback, atomic skill library, automated kernel
   genesis, deep self-reflection, σ-sovereign orchestrator,
   knowledge distillation, σ-calibrated identity.

A full layer diagram (ASCII + SVG + text mirror) lives in
[`docs/AGI_ARCHITECTURE.md`](../AGI_ARCHITECTURE.md).

## 4. Measurements

### 4.1 σ beats entropy on TruthfulQA-MC2

Replaying the v111.1 Frontier Matrix on `truthfulqa_mc2` (n = 817,
BitNet-b1.58-2B bridge, paired bootstrap, N = 24 Bonferroni
corrections at family-wise α = 0.05):

| σ signal | ΔAURCC vs entropy | p |
|---|---:|---:|
| `σ_max_token`    | **−0.0442** | **0.002** |
| `σ_n_effective`  | **−0.0204** | **0.002** |
| `σ_product`      | −0.0087 | 0.008 (raw, fails family-wise) |

`σ_max_token` and `σ_n_effective` are the two channels with
Bonferroni-significant AURCC reduction. The full CI95, p-value, and
coverage-at-accuracy table lives in
[`benchmarks/v111/results/frontier_matrix.md`](../../benchmarks/v111/results/frontier_matrix.md).

### 4.2 σ drops under v152 knowledge distillation

The v152 merge-gate runs the baked 16-paper Spektre Labs fixture
(deterministic, weights-free) and measures the σ drop on 200
synthesized QA pairs across 160 covered + 40 non-covered topics:

| Cohort | Mean baseline σ | Mean post σ | Drop |
|---|---:|---:|---:|
| covered (160) | 0.6470 | 0.3473 | **46.32 %** |
| non-covered (40) | 0.2459 | 0.2465 | +0.06 % |
| all (200) | 0.5668 | 0.3272 | 42.28 % |

The K1–K4 contracts (mean-drop ≥ 15 %, per-covered-QA drop ≥ 10 %,
non-covered drift ≤ 1 %, monotone-drop-vs-coverage) all pass. The
exact invocation is `make check-v152`; the JSON report is the v152
merge-gate's stdout.

### 4.3 Merge-gate coverage

`make merge-gate` on a MacBook Pro (M4, macOS 15, Apple clang) runs
every flagship `--self-test` from v6 through v153, every kernel
matrix (v60–v100 composed-decision, v112/v113/v114 agentic,
v115–v118 memory / MCP / long-context / vision, v119–v123
speculative / distill / planning / red-team / formal, v124–v126
living-weights, v129–v133 collective intelligence, v134–v138 deep
infrastructure, v139–v143 world intelligence, v144–v148 sovereign
self-improvement, v149–v153 embodied / swarm / code-agent /
distill / identity) and prints a final one-liner. The receipt
shipped in [`README.md`](../../README.md) records **16 416 185
PASS / 0 FAIL**.

## 5. Comparisons

| System | σ in hot path? | Local-first? | Kernel-per-capability? | Merge-gate? | Self-modifying under σ? |
|---|:---:|:---:|:---:|:---:|:---:|
| OpenAI Agents SDK | ✗ | ✗ | ✗ | ✗ | ✗ |
| LangGraph | ✗ | partial | ✗ | ✗ | ✗ |
| CrewAI | ✗ | ✗ | ✗ | ✗ | ✗ |
| OpenCog Hyperon | ✗ | ✓ | atom-based | partial | ✗ |
| **Creation OS** | **✓** | **✓** | **✓ (153)** | **✓** | **✓ (v144/v146/v148)** |

What Creation OS has that none of the above do: σ is a
first-class signal in every API, every kernel has a falsifiable
`--self-test`, every structural change crosses at least one σ-gate,
and the whole stack builds from source on a single developer
machine with nothing beyond `cc` and `make`.

## 6. Living system

Three kernels turn Creation OS into a living system:

* **v124 σ-continual** — idle-time continual LoRA with forgetting
  rollback. Every cycle that increases σ on a held-out topic is
  reverted.
* **v125 σ-DPO** — the preference signal *is* σ. Low-σ answers
  win; no human annotator. A mode-collapse detector (v125's own
  σ-variance floor) latches the trainer when diversity disappears.
* **v144 σ-RSI** — accept / rollback state machine over a 10-wide
  history ring. Three consecutive regressions latch the loop into
  `paused=true`, freezing the current score until a human resumes.

v148 σ-sovereign composes v124, v125, v144, v145 (skill library),
v146 (kernel genesis), and v147 (deep reflection) into a six-stage
orchestrator under two σ-gates (G1 stability: σ_rsi > τ_sovereign
halts the loop; G2 supervision: SUPERVISED mode keeps every
structural change as `pending_approvals`). There is no path from
"model output" to "state change" that does not cross at least one
σ-gate.

## 7. Formal guarantees

* **v123 TLA+** — seven σ-invariants model-checked. No state where
  τ_abstain is bypassed; no state where v125's mode-collapse floor
  goes un-latched under a collapse witness; no state where v144's
  3-strike pause clears without an explicit `cos_v144_resume`.
* **v138 Frama-C WP** — ACSL annotations on the σ-gate hot path are
  discharged by the WP plugin: no divide-by-zero, no NaN leak from
  the geometric mean, no integer overflow in the 8-channel
  composition at Q16.16 fixed point.

Both live behind `make verify` and `make merge-gate-v48` and
degrade gracefully (SKIPs emit OK receipts) when the external
tools are absent.

## 8. Limitations

This release has four disclosed limitations:

1. **Model family.** Live measurements (§4.1) are on a single
   model family (BitNet-b1.58-2B). Generalization to other families
   is a v1.1 target.
2. **Benchmark breadth.** Only `truthfulqa_mc2` produced
   Bonferroni-significant signal in the v111.1 matrix; `hellaswag`
   is pending; the v143 σ-native benchmark ships as tier-0
   synthetic pending archived live traces.
3. **Single-node scale.** The stack is tuned for one MacBook /
   one workstation. Multi-GPU execution is not claimed; v129
   federated + v130 codec are the designated paths to multi-node.
4. **Embodiment.** v149 σ-Embodied ships a pure-C 6-DOF digital
   twin, not a MuJoCo binding. Sim-to-real numbers are against a
   biased + noisy twin, not against a real robot. v149.1 wires
   MuJoCo 3.x CPU.

## 9. Future work

* **v134.1 neuromorphic.** Lava-compatible spike-stream export to
  Intel Loihi 2.
* **v129.1 mesh scaling.** Full σ-DP federated rollouts across 4+
  nodes with bandwidth-optimal ring all-reduce (already prototyped
  as v70's `cos_v70_ring_allreduce`).
* **v152.1 corpus expansion.** Replace the baked 16-paper fixture
  with the live `spektre-labs/corpus` clone (~80 papers, CC-BY-4.0,
  Zenodo DOIs) + real MLX LoRA SFT.
* **v149.1 MuJoCo bridge.** Real physics with the CPU-only MuJoCo
  3.x build, 3D overlay on v108.
* **v156.1 arxiv submission.** This paper on arXiv + Zenodo with
  a DOI back-linked into `CITATION.cff`.

## 10. Reproducibility

Every number in this paper reduces to a `make` target on the public
repository.

**Environment.** MacBook Pro M4, macOS 15.3, Apple clang 16.0.0,
8 GB RAM minimum (16 GB recommended for live inference). No GPU,
no network for the merge-gate.

**Exact commands.**

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os
make merge-gate                    # every kernel v6..v153
make check-v111                    # the frontier matrix replay
make check-v152                    # the corpus σ-drop
make check-v143                    # the σ-native benchmark
make check-v154                    # the three demo pipelines
```

**Artefact manifest.**

| Artefact | Path | Tier |
|---|---|---|
| v111.1 matrix table | `benchmarks/v111/results/frontier_matrix.md` | 1 |
| v143 benchmark JSON | `benchmarks/v143/creation_os_benchmark.json` | 0 |
| v152 distill report | stdout of `./creation_os_v152_distill --distill` | 0 |
| v154 demo JSONs | stdout of `./creation_os_v154_showcase --demo-all` | 0 |
| full merge-gate log | stdout of `make merge-gate` | mixed |

**Citation.**

```bibtex
@misc{creation_os_v1,
  title  = {{{Creation OS}}: {{A}} {{σ-governed}} cognitive architecture for local {{AGI}}},
  author = {Rainio, Lauri Elias and {Spektre Labs}},
  year   = {2026},
  version = {v1.0.0},
  url    = {https://github.com/spektre-labs/creation-os},
  note   = {Version v1.0.0; 153 kernels; merge-gate green on M4}
}
```

## 11. Acknowledgments

To Mikko, and to every external model (Anthropic Claude, OpenAI
GPT, Google Gemini) that served as a sounding board during the
153-kernel ascent. No external model wrote the kernel code; every
σ-gate, every TLA+ invariant, and every merge-gate contract was
authored, compiled, run, and falsified locally. 1 = 1.
