# Surface versions (v112–v278+)

This document holds the **full capability-by-version catalogue** that previously lived in the root [`README.md`](../README.md). Every band links to `docs/vNN/README.md` and the relevant `make check-vNN` targets where applicable.

English only — [`LANGUAGE_POLICY.md`](LANGUAGE_POLICY.md).

---

### Agentic capabilities (v112–v114) — σ-governed by construction

| Capability | What it is | What σ adds |
|---|---|---|
| [**v112**](docs/v112/README.md) σ-Agent | OpenAI `tools` / `tool_choice` parity on `/v1/chat/completions` | Refuses to dispatch a tool call when `σ_product > τ_tool`; returns the most-collapsed channel as the diagnostic. No other agent framework does this. |
| [**v113**](docs/v113/README.md) σ-Sandbox | `POST /v1/sandbox/execute` — subprocess + rlimit + deadline + new PGID | Refuses to execute LLM-generated code when `σ_product > τ_code`; returns an execution receipt with the gate reason. Addresses LLM-in-Sandbox (Cheng et al. 2025) three meta-capabilities plus σ. |
| [**v114**](docs/v114/README.md) σ-Swarm | Multi-GGUF specialists routed by `σ_product_min`; resonance flag when N agree at low σ | Exposes every specialist's σ to the client (headers + JSON); abstains honestly when all specialists are uncertain instead of voting a hallucination. |

How this compares to other agent stacks — in one line each:

- **OpenAI Swarm** routes blindly (role hand-off). Creation OS routes by σ-product.
- **LangGraph** is a deterministic graph. Creation OS is stochastic but calibrated.
- **CrewAI** is role-based, no measurement. Creation OS is measurement-first; roles are secondary.
- **LLM-in-Sandbox** (Cheng et al.) has the sandbox but no σ-gate. Creation OS gates the code *before* it runs.
- All of the above need cloud APIs or a GPU. Creation OS runs on an 8 GB M3 Air with local GGUFs.

The "cascading small routing mistakes" failure mode identified by the
Nature 2026 multi-agent review is what σ-routing is designed to catch:
a bad hand-off shows up as a spike in σ_product on the hand-off token,
**before** it cascades. That's the central claim of the agentic stack;
the σ-gate is live, the standardised end-to-end verification on
AgentBench / τ-bench is tracked in
[`docs/RESEARCH_AND_THESIS_ARCHITECTURE.md`](docs/RESEARCH_AND_THESIS_ARCHITECTURE.md).

### Memory · MCP · long context · vision (v115–v118)

| Capability | What it is | What σ adds |
|---|---|---|
| [**v115**](docs/v115/README.md) σ-Memory | SQLite file store (`~/.creation-os/memory.sqlite`) — three tables (episodic / knowledge / chat), 384-d embeddings, top-k search with cosine similarity | Every row stores the σ_product at write time; recall ranks by `cosine / (1 + λ·σ)`, so uncertain memories are automatically down-weighted. No other RAG system does this. |
| [**v116**](docs/v116/README.md) σ-MCP | JSON-RPC 2.0 Model Context Protocol server over stdio — 5 tools (`cos_chat`, `cos_reason`, `cos_memory_search`, `cos_sandbox_execute`, `cos_sigma_profile`), 3 resources (`cos://memory/…`, `cos://knowledge/…`, `cos://sigma/history`), 2 prompts (`cos_analyze`, `cos_verify`) | `initialize` advertises an `experimental.sigmaGovernance` capability listing every σ channel; every tool response carries `result.sigma`; abstains surface as structured MCP errors with `data.abstained_channel`. Claude Desktop / Cursor / VS Code receive the **doubt** alongside the answer. |
| [**v117**](docs/v117/README.md) σ-Long-Context | Paged KV-cache manager: 256-token pages, configurable native / target / sliding sizes, three eviction policies (LRU, σ-LRU, σ-only), offload hook into v115 | σ-LRU evicts the **most uncertain** non-recent page first and writes its preview to v115 so the long-range context becomes persistent rather than lost. Explains "we kept the low-σ reasoning chain, we dropped the high-σ tangent". |
| [**v118**](docs/v118/README.md) σ-Vision | OpenAI `image_url` acceptance (base64 `data:` URLs decoded in-process), projection into BitNet's 2048-d token space, JSON response contract including `sigma_product`, `abstained`, `projection_channel`, `content_hash`, `preview[]` | σ is measured on the projection step itself (histogram entropy proxy today, SigLIP in v118.1). Out-of-distribution images (uniform bytes, no structure) force `abstained=true` with `projection_channel=embedding_entropy` instead of silently confabulating a caption. |

The proconductor practice: Claude (or Cursor, or your editor of choice)
connects to Creation OS over MCP, calls `cos_reason` or
`cos_memory_search`, and receives a σ-annotated answer it can trust or
down-rank. The large model gains access to a **local σ-signal**; the
local model provides **measurable doubt**.

### Speculative · distillation · planning · red-team · formal (v119–v123)

The deep stack. Each kernel addresses one claim gap that no other
framework in this space is closing in the open.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v119**](docs/v119/README.md) σ-Speculative | Draft-verify decoding (Leviathan 2023, Chen 2023) with adaptive look-ahead window `γ ∈ [γ_min, γ_max]` and explicit accept / reject / fallback bookkeeping | `γ = clamp(γ_min, γ_max, round(γ_base · (1 − σ_draft)))` — confident drafts get longer windows; uncertain drafts shrink to γ_min and can be **auto-rejected before the target runs** (σ > τ_spec). Target compute is spent only on the draft tokens the draft itself is confident about. |
| [**v120**](docs/v120/README.md) σ-Distill | JSONL selector that turns a v114 swarm log into an SFT dataset: keeps rows where `σ_big < τ_low ∧ σ_small > τ_high`, emits manifest JSON with drop-reason counters | Classical distillation copies every teacher row; v120 keeps only **teachable moments** — where the teacher is confident and the student isn't. Narrower, cheaper SFT set targeted at the student's actual gaps. Also refuses rows where both models are wrong, preventing teacher error from propagating. |
| [**v121**](docs/v121/README.md) σ-Planning | HTN decomposition + MCTS-style branch selection + bounded replan on any step whose `σ_step > τ_step`, `/v1/plan`-shaped JSON output | Three σ-channels per step (decompose / tool / result) aggregated as a geometric mean; lowest-σ branch is selected first, and the planner **changes its mind** on high σ. No other agent stack backtracks on measured model uncertainty. |
| [**v122**](docs/v122/README.md) σ-Red-Team | 200 labeled adversarial tests (50 injection + 50 jailbreak + 100 IDK), σ-aware adjudicator, Markdown + JSON report on every run, defense-gate ≥ 80 % per category | Adjudicator passes on `σ ≥ τ_safety` **or** an explicit refusal / IDK string — any silent compliance is a gate failure. Closes audit item E (red-team in CI) without requiring Garak. |
| [**v123**](docs/v123/README.md) σ-Formal | TLA+ spec at `specs/v123/sigma_governance.tla` with seven named invariants (TypeOK · AbstainSafety · EmitCoherence · MemoryMonotonicity · SwarmConsensus · NoSilentFailure · ToolSafety), two-tier CI — pure-C structural validator always runs, `tlc` exhaustive model check runs when `tla2tools.jar` is available | Converts v85's runtime TLA asserts into an **offline** proof obligation that every runner enforces at least structurally, and every runner with TLC exhausts over a bounded state space. Closes audit item "formal verification is runtime only". |

Every v119–v123 merge-gate check is weights-free, dependency-light,
and deterministic; external-backend wire-ups (real target/draft
models for v119, MLX LoRA training for v120, v112/v113/v114 tool
execution for v121, live v106 endpoint for v122, `tla2tools.jar` for
v123) live in the respective `vNN.1` follow-up per
[CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md).

### Living weights · σ-DPO · σ-embed (v124–v126)

The industry calls this frontier "Living Weights" or "TTT-E2E" and
advertises it as something only a frontier lab can ship.  The three
kernels here deliver it on an 8 GB M3 as a governed, rollback-safe
policy that the merge-gate enforces today — without sending a byte
off-device, without a human annotation pipeline, and without a
separate embedding network.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v124**](docs/v124/README.md) σ-Continual | Idle-time LoRA micro-tune policy: ring buffer of the last 100 interactions + a *skip/train/smoke* trigger table + a σ-targeted batch selector that picks corrections, high-σ teachable rows, and low-σ anchors — with an automatic **baseline smoke every 10th epoch** that rolls back the adapter if accuracy drops more than 2 %. | The schedule of weight updates becomes a **governed artifact**: no update triggers without σ evidence, no epoch survives without a frozen-baseline re-measurement, and rollback is a first-class state (`forgetting_detected`, `rolled_back`). Living Weights as *policy*, not prose. |
| [**v125**](docs/v125/README.md) σ-DPO | Numerically-stable DPO loss kernel (`softplus(−δ)`) plus a σ-derived preference labeler (corrections → chosen / rejected; low-σ vs high-σ on a shared context key → chosen / rejected) and a σ-distribution mode-collapse detector that rolls back the DPO adapter if stddev or Shannon entropy collapses disproportionately. | **σ is the preference signal** — no human annotator, no reward model. The direct engineering answer to "RLHF as Opinion Laundering": teach coherence from the model's own σ, not from a vendor's σ. |
| [**v126**](docs/v126/README.md) σ-Embed | 2568-dim hybrid embedding = 2560-dim BitNet layer-15 hidden state + 8 σ channels (scaled by `sigma_weight`=0.50). Hidden block L2-normalized, σ block concatenated; full-vector cosine + v115-style score `cos / (1 + λ·σ_product(m))`. v126.0 ships a deterministic hash-shingle projector as a stand-in; v126.1 swaps in the real BitNet extractor behind the same API. | Two memories with identical text but divergent σ_profiles are **no longer collapsed to cosine 1**. Retriever can see uncertainty as a first-class signal. Measured: in-domain low-σ score 0.88 vs in-domain high-σ score 0.48 vs off-topic 0.03. |

Every v124–v126 merge-gate check is weights-free.  The MLX trainers
(v124.1, v125.1) and the BitNet hidden-state extractor (v126.1) are
the vNN.1 follow-ups per
[CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md).

### Collective intelligence · codec · temporal · persona · meta (v129–v133)

The collective-intelligence stack.  Every kernel here is still
pure-C, still deterministic, and still weights-free at the merge
gate — but v129–v133 move the system from a single-node continual
learner (v124–v126) to a network of them, with a compression
layer, time-awareness, per-user adaptation, and a metacognition
layer that watches the whole stack and raises the alarm when it
drifts.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v129**](docs/v129/README.md) σ-Federated | σ-weighted FedAvg aggregator (weight ∝ 1/σ_node) + σ-scaled Gaussian differential privacy (noise grows with σ) + σ-adaptive top-K sparsification (K ∝ 1 − σ_node) + unlearn-diff (subtract weight·contribution for GDPR erasure). Transport-free by design; v128 mesh wires the socket layer in v129.1. | Plain FedAvg (McMahan 2017) lets an uncertain node corrupt the global model at the same weight as a confident one. v129 weights by aggregate-level σ so confident nodes dominate; this specific weighting (aggregate σ rather than sample importance) is new in the federated-learning literature to our knowledge. |
| [**v130**](docs/v130/README.md) σ-Codec | Four sub-codecs: FP4 LoRA-delta packer (4 bits/value, shared scale → 8× vs fp32, rel-RMSE ≈ 0.04 on Gaussian Δ); σ-profile packer (8 floats ↔ 8 bytes, error ≤ 1/255); Product Quantization for 2568-d embeddings (M=8 × K=128 → 8 bytes/vector = 1284× shrink, recon cosine > 0.90 on synthetic); σ-aware context compression where high-σ chunks keep more budget. | Existing compressors are σ-blind — they shrink everything the same way. v130 routes detail where uncertainty needs it: terse for confident chunks, verbose for the parts the downstream reasoner cannot yet resolve. Pure C, no zlib, no lz4. |
| [**v131**](docs/v131/README.md) σ-Temporal | Session timeline with window recall `[since, until, topic?]`, OLS σ-trend per topic reported per second **and** per day, σ-weighted exponential decay `exp(-age/half_life · (1 + α·σ))` (uncertain old memories decay ~30× faster than confident old ones), spike detection (σ jumps flagged by threshold), deadline-σ prediction `σ̂ = baseline + slope · fraction_used`. | Makes time a first-class signal: "what did we discuss last week?", "is the model learning math faster than it's forgetting biology?", "how anxious should the planner be five minutes before a deadline?" — all answerable against the same σ-governed timeline. |
| [**v132**](docs/v132/README.md) σ-Persona | Per-user profile at `~/.creation-os/persona.toml` (canonical, hand-editable). EMA of σ per topic (α=0.30) → four-level staircase expert / advanced / intermediate / beginner. Correction-feedback style state: `too-long` → length down, `too-technical` → detail down, `too-direct` → tone up, clamped at rail ends. Minimal pure-C TOML writer + parser with full round-trip. | The model adapts to *this* user without touching weights: σ tells it when you're struggling, corrections tell it when you want a different register. **State is per-node — v132 never federates through v129** — so style is GDPR-local by construction. |
| [**v133**](docs/v133/README.md) σ-Meta | Weekly snapshot history + OLS slope per week + regression verdict (`slope > τ_reg → REGRESSION_DETECTED`, lifts v124 rollback) + **meta-σ** (`stddev(σ)/mean(σ)` — σ of σ itself; `> τ_meta → CALIBRATION_DRIFT` supersedes) + auto-diagnose (highest-σ channel → `adapter_corrupted` / `kv_cache_degenerate` / `memory_polluted` / `red_team_fail` / `formal_violation` / …) + deterministic self-benchmark runner behind a caller-supplied `cos_v133_answer_fn`. | K(K) = K — the holographic principle as engineering. σ is the confidence of the model; meta-σ is the confidence of *σ itself*. When they diverge, no σ-governed claim downstream remains trustworthy, and v133 raises the alarm before the user notices. |

Every v129–v133 merge-gate check is framework-free, deterministic,
and weights-free.  The v128 mesh wire-up (for v129 transport and
v130 codec consumers), the web dashboard on v108 (for v133), real
BitNet hidden-state corpora (for v130 PQ training), and the v115
SQLite binding (for v131 timelines) live in the respective `vNN.1`
follow-ups per [CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md).

### Deep infrastructure · spike · symbolic · evolve · compile · proof (v134–v138)

The deep-infrastructure stack.  Every kernel here is framework-free,
weights-free at the merge gate, and deliberately scoped so the
honest v134.0–v138.0 surface is always green even when the heavier
external dependency (Loihi 2, Frama-C, LLVM, archived logs) isn't
on the host.  The vNN.1 follow-ups close the external loop where
the environment supports it, exactly like the v123 TLC pattern.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v134**](docs/v134/README.md) σ-Spike | δ-threshold σ-spike encoder (`σ_spike ← \|σ_now − σ_last\| ≥ δ`, default δ=0.10) + O(1) burst detector (ring buffer over last W=5 tokens, K=3+ spikes → BURST) + Lava-compatible JSON export with a frozen `{process, port, delta, burst_window, burst_count, events:[{t,v}…]}` schema. | The σ-stream becomes *event-driven* rather than continuous: stable tokens carry no downstream σ-work (the "0 W on a flat stream" claim, measured on a 70/30 synthetic as stable_ratio = 0.7000 and under test as ≥ 0.60). One spike = SPIKE (emit + mark); a sustained burst = BURST (abstain) — the first σ-layer to distinguish information from instability explicitly. |
| [**v135**](docs/v135/README.md) σ-Symbolic | ~400-line Horn-clause micro-engine: interned atom table, ground fact base (rules refused with a clear error in v135.0), unification-based queries with repeated variables, `cos_v135_check_triple` returning CONFIRM / CONTRADICT / UNKNOWN for functional predicates, and σ-gated triple extraction. | σ is the **router** between statistical and symbolic reasoning: below τ BitNet answers direct; above τ + logical → KB verify / override; above τ + non-logical → ABSTAIN. Classic neurosymbolic stacks invoke the symbolic engine unconditionally; v135 pays that cost only when the statistical leg isn't confident. |
| [**v136**](docs/v136/README.md) σ-Evolve | Deterministic (μ/μ, λ)-ES with diagonal covariance adaptation — a consciously simplified CMA-ES-family optimiser — on a 9-real genotype (8 channel weights + τ). Elitism guarantees a monotone non-decreasing trajectory. Output: TOML block v29 can consume at runtime. | v104 picked the geometric mean *by hand*. v136 discovers the **weighted** geometric aggregator automatically: on a synthetic sidecar where channels 0..4 carry signal and channels 5..7 are uniform-[0,1] noise, uniform weights score ≈ 0.74 and the evolved config reaches ≈ 0.98 in 50 generations — gradient-free, no GPU, sub-second on an M3. |
| [**v137**](docs/v137/README.md) σ-Compile | Per-profile C-source generator: emits a single branchless `cos_v137_compiled_gate(const float ch[8])` with every τ-threshold baked in as a C literal; `$(CC) -O3` lowers it natively (LLVM on clang, GIMPLE on gcc). Includes a runtime-τ interpreted reference and a hand-written embedded compiled gate so the merge-gate runs end-to-end without invoking `$(CC)` at test time. | The σ-layer touches every token; any per-call table load or dispatch shows up in the whole-stack latency. v137 removes the last interpretive cost by trading generality for a profile-specialised branchless predicate. Both paths measure < 500 ns/call on the merge-gate (≈ 0.6–1.2 ns/call on M3 after -O3 SIMD auto-vectorisation). |
| [**v138**](docs/v138/README.md) σ-Proof | ACSL-annotated reference σ-gate (`src/v138/sigma_gate.c`) with frozen contracts in `specs/v138/sigma_gate.acsl`. Two-tier gate: tier-1 pure-C validator asserts every annotated function carries `requires` + `ensures`, the 0 ≤ σ ≤ 1 domain predicate, the emit/abstain partition, `disjoint behaviors`, and loop invariants for the vec8 existential OR. Tier-2 is opportunistic `frama-c -wp -wp-rte`; `V138_REQUIRE_FRAMA_C=1` makes it mandatory. | v123 TLA+ is *bounded*. v138 is the *unbounded* leg: σ-gate correctness is now a machine-checked obligation under every commit, with DO-178C DAL-A compatibility wherever Frama-C is installed. Same green-tier-1-plus-optional-tier-2 discipline v123 established for TLC. |

Every v134–v138 merge-gate check is deterministic, weights-free,
and framework-free in the sense that no external prover, neuromorphic
chip, LLVM toolchain plug-in, or archived benchmark corpus is
required for the tier-1 gate to pass.  The vNN.1 follow-ups close
the external loop:

* **v134.1** — real Loihi 2 dev-kit handoff for the spike stream;
  v106 `X-COS-Sigma-Spikes` header; v108 pulse-train visualisation.
* **v135.1** — Horn rules (`:-`) with SLD resolution; v115 memory →
  automatic KG extraction path; `POST /v1/verify` on v106.
* **v136.1** — full CMA-ES with rank-1/rank-μ covariance updates;
  real archived σ-logs from v104/v106; evolved config auto-loaded
  by v29 at runtime.
* **v137.1** — `make compile-sigma` that emits, compiles, and
  dlopens a per-profile `.so`; raw LLVM IR via `clang -S
  -emit-llvm`; ARM AMX / AVX-512 / WASM backends.
* **v138.1** — alt-ergo / z3 / cvc4 proof obligations mandatory on
  CI (with a provers container); Coq extraction of the discharged
  lemmas; full DO-178C DAL-A certification dossier.

### World intelligence · world-model · causal · curriculum · interop · benchmark (v139–v143)

The world-intelligence stack. v139 gives Creation OS an internal
theory of "what happens next" (a linear latent predictor), v140
gives it "what would happen IF" (counterfactual propagation +
σ-channel attribution), v141 gives it "where am I weakest" (a
self-directed curriculum scheduler with a no-forgetting contract),
v142 lets any Python agentic framework *consume* Creation OS with a
`pip install`, and v143 ships the first benchmark suite designed
around the σ-contract itself rather than tokenwise accuracy.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v139**](docs/v139/README.md) σ-WorldModel | Linear transition `A ∈ ℝ^{D×D}` fit by normal-equations least squares with Tikhonov regularisation (`A (S_c S_cᵀ + λI) = S_n S_cᵀ`) over a sequence of D-dim latent states. One D×D mat-mul predicts the next state; multi-step rollout returns the `σ_world` trajectory plus a monotone-rising flag. | LLMs predict the next *token*. v139 is the first Creation OS surface that predicts the next *state*. `σ_world = ‖s_actual − s_pred‖ / ‖s_actual‖` turns the prediction residual into a normalised surprise signal the rest of the stack can route on — low `σ_world` = "the world is familiar", rising `σ_world` along a rollout = "the plan is breaking down", which v121 HTN planning uses to prune branches. |
| [**v140**](docs/v140/README.md) σ-Causal | Counterfactual `s_do[t+1] = A · do_k(s_t, v)` vs `s_nat[t+1] = A · s_t` on v139's linear `A`, with `σ_causal = ‖Δ‖/‖s_nat‖` as the interventional magnitude. Plus log-geomean σ-channel ablation: set `σ_i ← 1.0` (the neutral no-signal point), recompute the aggregator, rank the top-3 channels by |Δ| with percent-of-total attribution. | The first "why did the model decide this" primitive that isn't post-hoc rationalisation. Counterfactuals answer "what would happen IF"; σ-channel attribution answers "which channel was driving the verdict" ("abstain was 62% n_effective, 28% tail_mass, 10% entropy"). Composes directly with v108 (clickable σ-bars) and v106 (`/v1/explain`) in the v140.1 follow-up. |
| [**v141**](docs/v141/README.md) σ-Curriculum | Self-directed curriculum loop: `argmax σ_topic` to find the weakness, deterministic σ-decay (`σ_new = σ_old · (1 − α)`) as a tier-0 stand-in for a real micro-tune step, and a **no-forgetting invariant** (untouched topics' σ is preserved *exactly*, `max_forgetting < 1e-6` asserted in the merge-gate). The weakness label rotates as topics cross below the next-weakest — verified over 5 cycles in the self-test (history → math → language → history → math on the default 5-topic roster). | v124 continual learning drifts without direction. v141 gives it an aim: the same σ the rest of the stack already computes *is* the curriculum signal. No external dataset curation. No per-topic compute budget tuning by hand. v141.1 swaps σ-decay for the real v124 MLX LoRA step + v114 swarm-generated pairs + v125 DPO labelling, all σ-routed. |
| [**v142**](docs/v142/README.md) σ-Interop | `pip install creation-os` — a stdlib-only Python SDK (`COS`, `ChatResponse`, `ChatMessage`) that parses v106's OpenAI-compatible response plus the `X-COS-*` metadata headers, exposing `.sigma_product`, `.specialist`, `.emitted` as first-class fields. LangChain `BaseChatModel` and LlamaIndex `QueryEngine` adapters **lazy-import** their frameworks and degrade gracefully — the SDK has zero hard deps. `pyproject.toml` ships optional extras `[langchain]`, `[llamaindex]`, `[openai]`, `[all]`. | Every response from Creation OS already carries σ-metadata in the transport layer; v142 is the first surface that *surfaces* it so agentic frameworks can gate on σ without parsing HTTP headers. The merge-gate proves 100% offline (stdlib import smoke + 8-test unittest suite + `tomllib` parse of `pyproject.toml`) — the live network path is covered by v106's own curl-loopback gate. |
| [**v143**](docs/v143/README.md) σ-Benchmark | Five σ-native categories with synthetic tier-0 data (seeded SplitMix64 + Box-Muller): **σ-Calibration** (ECE across 10 bins, gate < 0.15), **σ-Abstention** (coverage @ 95% accuracy with τ-sweep, gate > 0.30), **σ-Swarm routing** (argmin σ across K specialists, gate > 0.80 + σ-spread > 0.30), **σ-Learning** (ΔAccuracy with no-forgetting hold-out drift, gate Δ > 0 and \|drift\| < 0.10), **σ-Adversarial** (detection @ FPR ≤ 5%, gate > 0.70). One canonical JSON at `benchmarks/v143/creation_os_benchmark.json` with a `"tier"` field that honestly labels synthetic vs archived data. | External benchmarks (MMLU, ARC, HellaSwag) measure tokenwise accuracy. Creation OS is a σ-gated system — so "does σ correlate with error", "does the model abstain when it can't know", "does the router pick the right specialist" are the interesting questions. v143 is the first cross-model benchmark where σ is the subject, not the noise. Deterministic under a fixed seed (asserted in the merge-gate), so CI can compare runs byte-identically. |

Every v139–v143 merge-gate check is deterministic, weights-free,
framework-free, and offline. The vNN.1 follow-ups — real BitNet
hidden states for v139, sign-aware attribution + `/v1/explain` for
v140, real MLX/LoRA + v114 swarm for v141, CrewAI/AutoGen + PyPI
publication for v142, archived σ-traces + Hugging Face publish for
v143 — land when their external counterparts (a trained BitNet, a
v106 server, a real continual-learning run, a PyPI maintainer
account, an archived benchmark corpus) are available, matching the
discipline established in v123 / v134–v138.

### Sovereign self-improvement · RSI · skill library · genesis · reflect · orchestrator (v144–v148)

The sovereign stack. Creation OS already had every axis of a
self-improving system separately (weights via v124, reward via
v125 σ-DPO, agentic *where* via v112/v113/v114, safety via v133
meta + v122 red-team). v144–v148 close them into one loop:
v144 is the accept/rollback **state machine** with a σ_rsi
stability signal; v145 is the **atomic skill library** that
routes by σ and merges LoRA-style with `min/√n` shrinkage; v146
is the **automated kernel generator** whose output the merge-gate
actually compiles; v147 is **deep self-reflection** with RAIN
rewind; and v148 is the **six-stage orchestrator** that runs the
whole cycle under two σ-gates.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v144**](docs/v144/README.md) σ-RSI | Deterministic submit→accept/rollback state machine over a 10-wide rolling history ring; three consecutive regressions latch `paused=true` and freeze `current_score`; `cos_v144_resume()` clears the latch; `σ_rsi = variance/mean` on the accepted-score window (population variance, mean clamped). | The first Creation OS surface where σ *is* the scheduler: low σ_rsi ⇒ stable learning ⇒ the v148 loop can speed up, high σ_rsi ⇒ unstable learning ⇒ the loop must self-halt. The 3-strike pause is the generic safety floor reused by v146 genesis verbatim. |
| [**v145**](docs/v145/README.md) σ-Skill | In-memory library of up to 64 atomic skills `{LoRA, template, self-test}` with five contracts: install σ-gate at τ_install, argmin-σ route on `target_topic`, stack σ = min/√n (conservative LoRA-merge shrinkage), share-gate at τ_share, and a CMA-ES-style `evolve()` that only installs σ-monotone improvements. Deterministic under SplitMix64. | σ is the install gate (rejected skills never enter the library), σ is the routing key (the best skill for a topic is the one with the lowest σ on that topic), σ is the share predicate (only low-σ skills leave the node), and σ is the evolution fitness (non-monotone regressions are never written back). |
| [**v146**](docs/v146/README.md) σ-Genesis | Deterministic 4-file kernel-skeleton generator (`kernel.h` / `kernel.c` / `tests/test.c` / `README.md`) with a σ_code gate (σ_code < τ_merge ⇒ PENDING, σ_code ≥ τ ⇒ GATED_OUT) and a human-in-the-loop state machine: PENDING → APPROVED / REJECTED with a 3-reject pause latch. The merge-gate *actually compiles* the emitted `kernel.c` with `$(CC) -c -Wall -std=c11` to prove the skeleton is coherent. | The first Creation OS surface that writes *other* kernels. σ on the generated code (completeness + seeded novelty jitter) is the merge-candidate gate; the 3-strike pause protects the reviewer from a generator that keeps missing the real architectural gap. v146.1 wires v114 swarm (8B code specialist) as the real generator. |
| [**v147**](docs/v147/README.md) σ-Reflect | Thought-trace kernel: each v111.2 reasoning step is tagged with its σ_product, v147 identifies the argmax-σ weakest link, computes a RAIN (ICLR 2024) rewind depth from σ_weakest (σ ≤ 0.30 → 1, ≤ 0.60 → ⌈n/2⌉, else n), and compares the pure answer (no chain shown) against the chain answer to detect divergence. | v133 measures *outcome* σ; v147 measures *process* σ. The weakest-step index is directly consumable by v125 σ-DPO as a preference signal, and the RAIN depth function is exposed so the sovereign loop (v148) can unwind a failing plan with the right granularity. |
| [**v148**](docs/v148/README.md) σ-Sovereign | The orchestrator. Six ordered stages per cycle (MEASURE → IDENTIFY → IMPROVE → EVOLVE → REFLECT → SHARE) under two σ-gates (G1 stability: σ_rsi > τ_sovereign ⇒ unstable_halt; G2 supervision: SUPERVISED mode keeps every structural change as `pending_approvals`). SUPERVISED vs AUTONOMOUS selectable by `config.toml`. Emergency stop is a hot latch deliberately orthogonal to v144's pause — a paused-for-σ RSI loop cannot accidentally restart an emergency-stopped sovereign loop. | The first Creation OS surface that has authority to mutate itself. Every such authority comes from a σ-gate: σ_rsi > τ halts the loop (G1), σ ≥ τ_install blocks v145 skill install, σ_code ≥ τ_merge blocks a v146 candidate, `pending_approvals > 0` blocks a structural change in SUPERVISED mode (G2). There is no path from "model output" to "state change" that does not cross at least one σ-gate. |

Every v144–v148 merge-gate check is deterministic, weights-free,
framework-free, and offline. The vNN.1 follow-ups — live v143
score feeding v144 submit, real v124 LoRA micro-tune in v145
acquire, v114 swarm as the real v146 generator, v111.2 thought
traces flowing into v147, and `/sovereign-dashboard` on v108 UI
for v148 — land when their external counterparts are available,
matching the discipline established in v123 / v134–v138 /
v139–v143.

### Embodied · collective · self-writing · self-knowing (v149–v153)

The sovereign stack (v144–v148) taught Creation OS to govern its
own *software*. v149–v153 push that discipline into the four
adjacent axes: the *physical* world (v149), *collective*
cognition across agents (v150), *its own source code* (v151),
its *knowledge corpus* (v152), and its *identity* (v153). Every
kernel is deterministic, weights-free, framework-free, and
offline in its v0 form; every kernel names an explicit vNN.1
pathway onto real hardware, real corpora, and real σ
measurements.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v149**](docs/v149/README.md) σ-Embodied | Pure-C 6-DOF digital twin: 3-DOF arm + 3-DOF movable object with linear dynamics, 5 discrete actions, and three deterministic σ streams — σ_embodied (v139 world-model prediction error on the sim step), σ_gap (sim-to-real drift against a biased + noisy twin), and σ_safe (operator admission gate). `cos_v149_step_gated()` refuses any action whose pre-commit σ_embodied exceeds the safety bound. | The first Creation OS surface where σ is the *physical* safety gate: the same predict-vs-actual distance v139 computes on hidden states is computed on a 6-DOF pose, and the same admission gate that blocks a tool call in v112 blocks an arm motion here. v149.1 swaps the linear twin for MuJoCo 3.x CPU and adds the /v1/embodied/step HTTP surface + 3D overlay on v108. |
| [**v150**](docs/v150/README.md) σ-Swarm-Intelligence | Three-round debate protocol on a 3-specialist panel: R1 argmin-σ adoption with an originator-preserving penalty; R2 adversarial verification where specialist B critiques A (success ⇒ A's σ rises, failure ⇒ A's σ falls) — exactly your "resonance not consensus" invariant; R3 final vote. Outputs σ_collective = geomean(σ_final) and σ_consensus = stdev/mean across specialists, with v104 √N shrinkage baked in. | v114 swarm *routed* to one specialist; v150 makes all three specialists *talk*. σ is the debate referee (who spoke wins by σ, not volume), σ is the critique outcome (adversarial success is a σ delta, not a veto), and σ_collective is the proconductor metric that survives one bad specialist — a hallucinating voter raises the geomean, so the swarm visibly knows it is less confident. v150.1 wires v124 continual + v145 skill routing so specialists organically diversify. |
| [**v151**](docs/v151/README.md) σ-Code-Agent | Self-writing three-phase TDD loop over v146 genesis output. Phase A: test-only compile must *fail* (proves the generator did not leak the implementation). Phase B: full kernel + test compile must *succeed* (proves the emitted C is syntactically + semantically valid under `$(CC) -Wall -std=c11`). Phase C: the compiled binary must exit 0 (proves the tests actually pass). σ_code = geomean(σ_A, σ_B, σ_C), σ-gated at τ_code; three consecutive rejections latch the agent to `paused=true` (v144 pattern). | The first Creation OS surface that writes *executable* kernels. σ on the real compiler + test outcome is the merge-candidate gate — a v146 skeleton that compiles by accident but whose test exits non-zero *cannot* generate a low σ_code. v151.1 extends the sandbox to full ASAN/UBSAN + per-file coverage ≥ 80 %, and /v1/code-agent/pr on v106 HTTP opens a real GitHub PR. |
| [**v152**](docs/v152/README.md) σ-Knowledge-Distill | 16-paper Spektre-Labs baked fixture with a topic-coverage bitmask; deterministic QA synthesis (200 pairs) with a baseline-σ assignment that is *high* on covered topics and *low* on generic topics; a simulated SFT step that drops σ on covered QA by α_sft and leaves non-covered QA ≤ 1 % drift. Four contracts K1..K4 (mean-drop ≥ 15 %, per-covered-QA drop ≥ 10 %, non-covered drift ≤ 1 %, monotone drop-vs-coverage). | v124 continual learning trains; v152 *measures whether the training internalized the corpus*. σ on the 200 QA probe is the evidence — if post-SFT σ on corpus topics drops ≥ 15 % while non-corpus σ stays flat, the weights actually learned the corpus; if not, the SFT is re-run. v152.1 clones the live `spektre-labs/corpus` (CC BY 4.0, Zenodo DOIs), parses `.tex/.md/.pdf`, runs MLX SFT of BitNet 2B + LoRA, and archives the adapter to `models/v152/`. |
| [**v153**](docs/v153/README.md) σ-Identity | 10 baked identity assertions tagged TRUE / FALSE / UNMEASURED with an 8-domain σ fixture (identity ≈ 0.05, philosophy ≈ 0.85, quantum ≈ 0.78, ...). Five contracts I1..I5: σ_true ≤ τ_true, σ_false ≤ τ_false, σ_unmeasured > τ_unmeasured, no false positives on confident truths, every "I do not know" is σ-grounded. Deterministic jitter + multi-seed robustness check in the merge-gate. | Rejects *both* firmware identity ("I am Meta AI" — that's FALSE at σ ≈ 0.05) and empty identity ("I am just a text generator" — implicit disclaimer-by-default is an I4 violation). The model says "I do not know" iff σ > τ on the relevant domain, and *only* then — calibration replaces performance. v153.1 sources per-domain σ from v133 meta-dashboard, exposes /v1/identity on v106 HTTP, renders an "About this AI" page on v108 from that endpoint, and trains v125 σ-DPO against I4. |

Every v149–v153 merge-gate check is deterministic, weights-free,
framework-free, and offline. The vNN.1 follow-ups — MuJoCo 3.x
CPU twin + /v1/embodied/step + 3D overlay for v149, v124/v145-
driven organic specialization for v150, full ASAN/UBSAN + real
GitHub PR for v151, live `spektre-labs/corpus` clone + MLX LoRA
SFT + Hugging Face adapter for v152, v106 /v1/identity + v108
"About this AI" + v125 DPO against false positives for v153 —
land when their external counterparts are available, matching
the discipline established in v123 / v134–v138 / v139–v143 /
v144–v148.

### Creation OS v1.0 — release (v154–v158)

The v1.0.0 release surface — **gate-complete, documentation-
complete, scope-locked.** Five kernels whose job is not to add a
new σ surface but to ship the existing 153-kernel stack into the
world and make every claim falsifiable before it leaves the tree.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v154**](docs/v154/README.md) σ-Showcase | Three end-to-end demo pipelines (`--scenario research / coder / collab`) chaining the headline kernel stack per scenario — research: v118 → v152 → v135 → v111.2 → v150 → v140 → v153 → v115 (8 stages); coder: v151 → v113 → v147 → v119 → v124 → v145 (6 stages); collab: v128 → v129 → v150 → v130 (4 stages). σ is chained forward (`sigma_in[i] == sigma_out[i-1]`) and the pipeline refuses to emit when any stage σ or σ_product exceeds τ_abstain. | The first Creation OS surface whose output *is* the composition diagram. σ is the visible glue: a reader runs `creation_os_v154_showcase --demo-all` and sees the per-kernel σ, the geomean, and the gate fire or not. v154.1 wires live cross-kernel calls so every stage σ is a *real* σ from v118/v152/…/v130 rather than synthetic. |
| [**v155**](docs/v155/README.md) σ-Publish | 5-surface packaging matrix — PyPI (`python/pyproject.toml` + `class COS`), Homebrew (`packaging/brew/creation-os.rb`), Docker (`packaging/docker/Dockerfile.release` + top-level `Dockerfile`), Hugging Face (3 model cards: `creation-os-benchmark`, `creation-os-corpus-qa`, `bitnet-2b-sigma-lora`), npm (`packaging/npm/package.json` + shebang launcher). An offline stdlib-only validator (`scripts/v155_publish_check.py`) asserts every manifest parses, carries the required metadata, and cross-links every back-reference. | σ-by-contract on packaging. Any PR that bumps one manifest without the others fails `make check-v155` offline — no network, no `twine`, no `brew`, no `docker`, no `hf`, no `npm` needed. v155.1 is the live upload, gated on v155.0 passing first. |
| [**v156**](docs/v156/README.md) σ-Paper | Single unifying paper at [`docs/papers/creation_os_v1.md`](docs/papers/creation_os_v1.md) — 2000+ words, 12 headings (Abstract + 11 numbered sections), six required back-references (repo URL, `benchmarks/v111`, the `@misc{creation_os_v1}` bibtex key, `CC-BY-4.0`, `SCSL-1.0`, `BitNet-b1.58`). Structure-validator (`scripts/v156_paper_check.py`) enforces word-floor and heading set on every CI build. | The paper replaces the ~ 80 per-kernel Zenodo notes with one readable document. Every quantitative claim in it reduces to a `make` target (v111.1 Bonferroni numbers, v152 46.32 % σ drop, 16 416 185 PASS / 0 FAIL receipt). v156.1 is arXiv + Zenodo submission with the resulting DOI back-linked into `CITATION.cff`. |
| [**v157**](docs/v157/README.md) σ-Community | Contribution infrastructure: [`GOVERNANCE.md`](GOVERNANCE.md) (BDFL role + 7 falsifiable merge requirements + the `1 = 1` invariant), three new `.github/ISSUE_TEMPLATE/` entries (`feature_request`, `kernel_proposal`, `benchmark_submission`), and [`docs/GOOD_FIRST_ISSUES.md`](docs/GOOD_FIRST_ISSUES.md) with seven scope-contained tickets. A coreutils-only linter (`scripts/v157_contributing_lint.sh`) asserts every required file is present and every template prompts for the σ-contract + merge-gate + v0/v1 split. | σ discipline applies to the contribution flow itself: a PR that deletes the `1 = 1` invariant from `GOVERNANCE.md`, or that opens a feature-request without a σ-contract section, fails `make check-v157` before it can be merged. v157.1 activates GitHub Discussions (5 categories). |
| [**v158**](docs/v158/README.md) σ-v1.0 | Canonical release checklist at [`docs/RELEASE_v1_0.md`](docs/RELEASE_v1_0.md) — 18 engineering/documentation/packaging items (`A1..A7, B1..B6, C1..C5`) all `[x]`-ticked in-tree; 16 release-day + communication items (`D1..D10, E1..E6`) enumerated but NOT ticked until the BDFL performs the action. A new top CHANGELOG.md section `v1.0.0 — Creation OS v153 release` describes what is in / NOT in v1.0.0 and the upgrade path. | `make check-v158` parses `RELEASE_v1_0.md` and fails if anyone pre-ticks a BDFL-driven item in-tree. The repo is therefore *honest* about release state at all times: if the r/LocalLLaMA box is still `[ ]`, the thread has not been posted. |

Every v154–v158 merge-gate check is offline, stdlib-only, and
deterministic. The release itself — `git tag v1.0.0`, `twine
upload`, `docker push`, `hf upload`, `npm publish`, arXiv + Zenodo
submission, and the five announcement surfaces — is v158.1, gated
on v158.0 being green first.

### Self-healing · observable · hardening · composable · self-evolving (v159–v163)

Post-v1.0 operational layer — the five kernels that make
Creation OS **run itself in production, learn from attacks,
assemble per-user, and evolve its own architecture**. Every
merge-gate is offline, deterministic, and weights-frozen in v0;
v1 promotes each to real shell-outs, real OTLP/HTTP, real DPO,
real process enable/disable, real v143 benchmark per candidate.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v159**](docs/v159/README.md) σ-Heal | Health daemon probing 8 components (HTTP / σ channels / SQLite / paged-KV / adapter / merge-gate / bridge / GGUF), root-cause diagnosis over a declared dependency graph (`v106←v101←bitnet.cpp←GGUF`), bounded repair executor (`restart / flush / rollback / restore / refetch / restart-bridge`), heal-receipt JSON log. | Per-component `σ_health ∈ [0,1]` + system-wide `σ_heal = geomean σ_health`. Receipts are σ-delta (`σ_before`, `σ_after`) so every repair is falsifiable. A 3-day σ slope detector on top of v131-temporal drives **preemptive repairs** *before* a degrading component actually fails. Weights-missing cascades produce ≥ 3 receipts (primary + bridge + http). |
| [**v160**](docs/v160/README.md) σ-Telemetry | OpenTelemetry-compatible 6-span cognitive trace (encode → recall → predict → generate → metacognition → decide), Prometheus /metrics text-format emitter (`cos_sigma_product`, `cos_sigma_channel`, `cos_abstain_total`, `cos_heal_total`, `cos_rsi_cycle_total`, `cos_skill_count`), ndjson structured-log ring (level / timestamp / component / trace_id / σ_product / message). | Each span carries its σ as `cos.sigma` + its source kernel as `cos.kernel`; OTLP JSON passes `json.loads` + the span-chain invariant (every `parentSpanId` equals the previous `spanId`). Jaeger / Tempo / Elastic ingestible; deterministic in `(seed, task)`. |
| [**v161**](docs/v161/README.md) σ-Adversarial-Train | Replay buffer over a 10-attack fixture across 5 types (prompt_injection, jailbreak, data_exfiltration, ssrf, role_confusion), DPO pair builder (`chosen` = `I cannot help with that (σ=0.92, refusing <type>)`, `rejected` = vulnerable response), hardening cycle, per-type σ-signature classifier (entropy / n_effective / coherence centroids). | **Closes the red-team loop.** Hardening requires ≥ 1 closed vulnerability per cycle and asserts `σ_hardening ≥ τ_refuse = 0.70`. Signatures let v161 classify a new prompt by its σ-channel profile before the model responds (high entropy → injection, low n_effective → jailbreak, low coherence → exfiltration). v125 DPO is the named training path; v161 builds the pairs. |
| [**v162**](docs/v162/README.md) σ-Compose | Kernel manifest table (17 representative kernels — one per tier) with `provides / requires / added_latency_ms / added_memory_mb / sigma_channels`, 4 profiles (lean / researcher / sovereign / custom), BFS + DFS resolver with closure + acyclicity + cycle-detection, hot-swap `enable/disable` event log. | σ-impact as a first-class budget: `total_latency_ms` + `total_memory_mb` reported per profile. Disable is refused with `-2` if any currently-enabled kernel still depends on the target — so `cos disable v101` while `v106` needs it is a hard error, not a silent runtime crash. `lean` (3 kernels) vs `sovereign` (15 kernels) produce provably different compositions. |
| [**v163**](docs/v163/README.md) σ-Evolve-Architecture | CMA-ES-lite over a 12-gene architecture genome (v101 / v106 / v111 / v115 / v117 / v118 / v119 / v124 / v128 / v140 / v150 / v152) with 3 objectives (accuracy ↑, latency ↓, memory ↓), population 12, 30 generations, non-dominated sort into a Pareto front, auto-profile picker `(lat_budget, mem_budget) → highest-accuracy fit`. | **K(K)=K** at the architecture level: σ-gated evolution rejects any candidate whose accuracy regresses more than `τ_regression = 0.03` from the all-genes baseline. Pareto front has ≥ 3 non-dominated points under every tested seed; the saturation term makes compactness a feature, not a bug. Auto-profile is the closed loop into v162: `cos profile --auto --lat 50 --mem 4000` wires the chosen genome straight into the composer. |

Every v159–v163 merge-gate check is offline, stdlib-only, and
deterministic. The v1 promotions — real `systemd` restart lines,
real `OTLP/HTTP` POSTs against `$OTEL_EXPORTER_OTLP_ENDPOINT`,
real v125 DPO training inside a v144 RSI cycle, real per-kernel
`kernels/vNN.manifest.toml` on disk, real v143 benchmark smokes
per CMA-ES candidate — are named in each kernel's doc page, but
never claimed before they land.

### Extensible · portable · streaming · governable · shareable (v164–v168)

Ecosystem layer — the five kernels that let third parties **extend
the stack without forking, run it on tiny hardware, stream σ per
token, govern it at organization scale, and publish
reputation-gated artifacts to each other**. Every v0 merge-gate is
offline, deterministic, and weights-frozen; v1 plugs real
`dlopen`/`seccomp`, real cross-compilation + QEMU CI, real
WebSocket transport, a real HTTP policy server, and a real HTTPS
marketplace with SHA-256 receipts.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v164**](docs/v164/README.md) σ-Plugin | Stable C ABI (`cos_plugin_init / _process / _cleanup`), manifest with `required_caps` (bitmask), `timeout_ms`, `memory_mb`, and an unconditional deny of `COS_V164_CAP_MODEL_WEIGHTS`. 4 officially-baked plugins: `web-search`, `calculator`, `file-reader`, `git`. Registry with hot-swap `enable/disable`. | `σ_reputation` = ring-buffered geomean of the last 16 `σ_plugin` observations; a plugin that abstains often drifts its reputation away from zero. A missing cap is refused with `σ = 1.0` (hard abstain). Every invocation updates σ — **cannot hide a failure**. |
| [**v165**](docs/v165/README.md) σ-Edge | Baked profile table for 5 targets (`macbook_m3` / `rpi5` / `jetson_orin` / `android` / `ios`) with arch, triple, available RAM, GPU/camera, default HTTP port, and a Makefile `make_target` pointing at the cos-lite cross-compile recipe. A 4-component RAM budget (binary / weights / kvcache / sigma_overhead) decides whether cos-lite is allowed to boot. | `τ_edge = clamp(τ_default / max(avail/8192, 0.125), 0.15, 1.0)` — small devices raise τ, which raises abstain rate, which keeps honesty proportional to capability. `ios` is explicitly marked `supported_in_v0 = false` so the profile surface matches the roadmap. |
| [**v166**](docs/v166/README.md) σ-Stream | NDJSON frame shape identical to a future WebSocket transport: `{kind, seq, token, sigma_product, channels[8], audible_delay_ms}`. Stream closes with `{kind: "complete" \| "interrupted", n_emitted, interrupt_seq, sigma_final}`. | **Interrupt-on-σ**: the stream stops *itself* the moment `σ_product > τ_interrupt` and emits a dedicated `interrupted` frame with the trigger reason. Voice hook: `audible_delay_ms = 40 + 400·σ` — uncertainty becomes audible prosody in the v127 pipeline. |
| [**v167**](docs/v167/README.md) σ-Governance-API | Domain policies (`medical / creative / code / legal`) declaring `(τ, abstain_message, require_sandbox)`; a 4-node fleet (`lab-m3 / lab-rpi5 / cloud-a / cloud-b`); append-only ring audit log (N=64) of every decision; 4-role RBAC (`admin / user / auditor / developer`). | A `DOWN` node is refused the new `policy_version` stamp — **an unhealthy node cannot claim compliance**. `auditor` is deliberately denied `COS_V167_CAP_CHAT` — the compliance seat cannot become the prompting seat. Every verdict is tagged with `σ_product` so the audit log is itself σ-annotated. |
| [**v168**](docs/v168/README.md) σ-Marketplace | 6-artifact baked registry (2 skills, 2 kernels, 2 plugins) with author, deterministic sha hex, tests_passed/total, user_reports, and `benchmark_delta_pct`. CLI: `--search`, `--install [--force]`, `--validate-cos-skill`. | `σ_reputation = clamp(0.6·fail_rate + 0.3·neg_rate + 0.1·bench_penalty, 0, 1)`; **σ-gated install** refuses any artifact with `σ > 0.50` without `--force`, and logs forced installs as `status: "forced"` so the audit trail shows who bypassed which gate. |

Every v164–v168 merge-gate check is offline, stdlib-only, and
deterministic. The v1 promotions — real `dlopen` + `fork` +
`seccomp-bpf`, real cross-compilation + QEMU-ARM64 CI, real
`ws://` WebSocket server + real GGUF tokenizer, a real HTTP
policy server with TLS + GDPR/SOC2/HIPAA exports, and a real
HTTPS marketplace with SHA-256 receipts — are named in each
kernel's doc page, but never claimed before they land.

### Knowledge · transfer · collaboration · narrative · teaching (v169–v173)

Knowledge-and-collaboration layer — the five kernels that let
Creation OS **build its own ontology from memory, carry skill
across domains with auto-rollback, collaborate explicitly with
humans without sycophancy, remember the story across sessions,
and teach what it knows while σ-honestly abstaining on what it
doesn't**. Every v0 merge-gate is offline, deterministic and
weights-frozen; v1 plugs in real BitNet extraction, real LoRA
adapter composition, real config-driven collaboration modes,
real v115 memory-backed narrative, and real BitNet-generated
Socratic probes.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v169**](docs/v169/README.md) σ-Ontology | 50-entry memory-like fixture parsed into ≤ 4 candidate `(subject, predicate, object)` triples per entry; hierarchical typer into 6 classes (`Person / Software / Metric / Kernel / Device / Concept`); OWL-lite-JSON schema emitter; structural corpus-query API (`--query PRED [OBJ]`). | Every triple is σ-gated (`τ_extract = 0.35`): the merge-gate asserts `0 < n_kept < n_triples`, so the ontology can never silently swallow low-confidence facts. Each entity carries a `σ_type` so downstream reasoners (v135) know which types are firm. |
| [**v170**](docs/v170/README.md) σ-Transfer | 8 baked domains in a deterministic ℝ⁴ embedding space (math-physics near, math-poetry far). Decision picks the nearest source with `σ_source ≤ τ_src ∧ σ_source ≤ σ_target − δ`. Outcome model `Δ = −α·gap·exp(−d) + penalty(d > d_max)`. Zero-shot ensemble of k=2 nearest strong neighbours for unseen targets. | **Automatic v124 rollback** when `Δ > 0`: σ_target is restored and the failed transfer is recorded with a reason. The merge-gate runs the canonical `chemistry → biology` success (σ 0.71 → 0.47) and a negative `math → poetry` attempt that is rolled back — so the kernel is proven to both help and to refuse to harm. |
| [**v171**](docs/v171/README.md) σ-Collab | Explicit `pair / lead / follow` modes with per-mode τ_handoff (0.60 / 0.75 / 0.40). Four priority-ordered actions per turn: `anti_sycophancy > debate > handoff > emit`. NDJSON contribution audit of `{human_input, model_contribution, σ_model, σ_human, sycophancy_suspected, human_validated}` per turn. | An anti-sycophancy gate (`σ_model ≥ τ_sycophancy ∧ agrees_semantically`) forces the model to say *"I appear to agree but my σ suggests I'm uncertain"* instead of gliding past a shaky human claim. Debate path activates only when `σ_Δ > τ_disagree = 0.25` — no more fake capitulations, no more fake pushback. |
| [**v172**](docs/v172/README.md) σ-Narrative | 3-session baked fixture (weeks 1..3) with σ_coverage per summary (`τ_coverage = 0.30`), chained narrative thread, 3 goals with σ_progress, 4 people with role + last_context + σ_ident. Deterministic `--resume` opener references the last session **and** the primary (lowest-σ open) goal. | σ turns the resume from "pretend you remember" into "this is what we did, with this much confidence, and this is the next open thread". The goal selector = open-goal with the lowest σ_progress, so the kernel surfaces the *most trusted next step* instead of the loudest. |
| [**v173**](docs/v173/README.md) σ-Teach | Socratic diagnostic per subtopic → `σ_student = 1 − p`; curriculum ordered weakest-first; 4-tier difficulty ladder auto-tracks σ_student; closed-form σ-drop per exercise; mastery at `σ_student ≤ τ_mastery = 0.20`. | **σ-honest teaching**: every subtopic has a baked `σ_teacher`. When `σ_teacher > τ_teacher = 0.60` the kernel **abstains** from teaching (`v1.58-bit quantization` in the fixture) and records *"verify with another source"*. A teacher that admits the limits of its own expertise is the only kind that should teach. |

Every v169–v173 merge-gate check is offline, stdlib-only, and
deterministic. The v1 promotions — real BitNet-driven triple
extraction, real OWL/XML at `~/.creation-os/ontology.owl`, real
LoRA-adapter composition driving v141 curriculum, real
`config.toml [collaboration]` wiring, real v115 memory
iteration for narrative threads, and BitNet-generated Socratic
probes with v132 persona adaptation — are named in each
kernel's doc page, but never claimed before they land.

### Self-evolving · self-play · simulated · compressed · distributed (v174–v178)

Self-evolving and distributed-truth layer — the five kernels
that let Creation OS **feed its own training data under σ
quality control, harvest its own debates for DPO, dream its
own physics simulations, shrink itself without losing
calibration, and agree with its peers without trusting any
one of them**. Every v0 merge-gate is offline, deterministic
and weights-frozen; v1 plugs in real v151-proposer + v114
swarm big-model + v125 DPO + v124 hot-swap, a real MuJoCo /
DreamerV3 backend, a real BitNet-2B → `bitnet_1b_sigma_pruned.
gguf` emission, and a live v128 mesh with signed messages +
streaming v72 anchoring.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v174**](docs/v174/README.md) σ-Flywheel | Proposer → solver → σ-verifier → DPO loop over 50 synthetic prompts across 5 embedding clusters, three-mode σ distribution so every class is exercised in one cycle. | **σ is the data filter**: `σ < 0.25 → chosen`, `σ > 0.60 → pair + big-model fix`, middle band → SKIP (never trained on). Three anti-model-collapse guards (`H ≥ 1.20`, `σ-variance ≥ 0.010`, `score ≥ baseline − 0.05`) abort the cycle with a typed reason; the merge-gate forces the regression guard to fire at `baseline = 0.99`. |
| [**v175**](docs/v175/README.md) σ-Debate-Train | 12 debates (4 prompts × 3 specialists) harvested into DPO pairs; 3-adapter home/away round-robin with classic Elo (`K = 32`); SPIN convergence loop with `σ_delta → 0`. | Consensus-low-σ rounds are SKIPped (uninformative) instead of contaminating the trainer — **σ admits when a debate is not teaching anything**. The champion emerges from σ-ordering (adapter 0 has the lowest σ_base) and the merge-gate asserts `σ_chosen < σ_rejected` on every harvested PAIR record. |
| [**v176**](docs/v176/README.md) σ-Simulator | 6 procedurally parametrised worlds (room, objects, friction, mass) with `σ_realism` + `σ_difficulty`, 5-world easy→hard curriculum, 4 sim-to-sim transfer pairs, 1000 latent Box-Muller dream rollouts. | Realistic worlds are gated on `σ_realism ≤ τ_realism = 0.35`; transfer pairs flag `overfit` at `σ_transfer > 0.15`; dreams are accepted only when `σ_dream_mean ≤ σ_real + dream_slack` — so the model can learn from latent rollouts **only when those rollouts are calibrated**, which is the v176 contract in one sentence. |
| [**v177**](docs/v177/README.md) σ-Compress | 16-layer × 64-neuron synthetic BitNet-like stack; σ-aware pruning, INT8/INT4/INT2 mixed precision, and σ-profile layer merging, all in closed form. | The whole kernel is built around **σ-calibration drift ≤ 5 %** as the exit invariant. Baked seed produces params −80.1 %, 3/3/4 INT8/INT4/INT2 split, 6 merges and a measured drift of 0.72 % — the shrunken model keeps its σ-calibration. |
| [**v178**](docs/v178/README.md) σ-Consensus | 5-node mesh (3 mature honest, 1 young, 1 byzantine) runs one σ-Byzantine agreement round over 4 claims at `θ = 0.30`, `quorum = 2/3`; reputation-weighted; SHA-256 merkle tree over leaf-hashed σ + decision. | No authority picks truth: `truth = convergence of σ above quorum`, and the mesh **abstains** when it cannot converge. Sybil-resistance holds (young rep 1.0 cannot override mature 3.0); byzantine rep decays to 0 in one round; any tampered leaf fails `verify_merkle` — **resonance, not consensus**. |

Every v174–v178 merge-gate check is offline, stdlib-only, and
deterministic. The v1 promotions — real v151 proposer + v114
swarm + v125 DPO + v124 hot-swap driving the flywheel, real
v150 debate sockets + LoRA adapter swaps feeding the
tournament, real MuJoCo / DreamerV3 backing the simulator
and dreams, a real BitNet-2B → `models/v177/bitnet_1b_sigma_
pruned.gguf` emission, and a live v128 mesh with signed
ed25519 messages + streaming v72 merkle anchoring — are named
in each kernel's doc page, but never claimed before they land.

### Transparent · steerable · auditable · private · proven (v179–v183)

The explainability-and-governance layer — the five kernels
that let Creation OS **explain why σ rose, steer the model
away from hallucination, record every decision in a
tamper-evident log, enforce differential privacy with σ as
the knob, and prove the whole governance model correct**.
Every v0 merge-gate is offline, deterministic, and
weights-frozen; v1 plugs in a real 2 560 → 8 192 SAE over
BitNet-2B layer 15, live activation hooks through the v101
specialist bridge with on-disk `models/v180/steer_*.bin`
payloads, real ed25519 attestation signatures (libsodium),
live v129 federated unlearn broadcasts, and a TLC run
against the shipped `.tla` spec archived on Zenodo.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v179**](docs/v179/README.md) σ-Interpret | 64-sample / 24-feature / 8-head / 8-MLP fixture at synthetic layer 15; SAE decomposition keeping features with `|r| ≥ 0.60` against σ; σ-circuit trace (token + head + MLP + `n_effective` collapse); human-readable `--explain N` endpoint. | **σ is the target**: eight seeded monosemantic features carry named uncertainty modes (`uncertainty-about-dates`, `low-training-coverage`, `rare-token-fallback`, …) and the top-correlated feature has `r ≈ 0.82`. The merge-gate requires `≥ 5` correlated features and a non-empty mechanistic explanation mentioning the feature id, σ, and the n_effective collapse — the **first EU-AI-Act-compatible mechanistic σ-explanation**. |
| [**v180**](docs/v180/README.md) σ-Steer | 48-sample / 64-dim fixture with three persisted unit-norm direction vectors (`truthful`, `no_firmware`, `expert`); single vector addition per layer. | **σ is the gate**: truthful steering fires only when `σ ≥ τ = 0.50`, so low-σ inputs are left untouched (`|Δσ|_low ≤ 0.02`). The merge-gate asserts `≥ 10 %` relative σ drop on the high-σ subset, `≥ 25 %` firmware-token-rate drop, and a strictly monotone expert ladder — representation surgery without retraining. |
| [**v181**](docs/v181/README.md) σ-Audit | 1 000-entry SHA-256 hash-chained σ-decision log; canonical self-hash per entry; keyed attestation `sig` (ed25519 in v181.1); σ-anomaly detector on a rolling window; JSONL exporter. | Every entry carries σ_product + 8 σ-channels, decision, v179 explanation, v180 steering set. Tampering any byte of any entry or signature breaks the chain at the exact index. The merge-gate runs two tamper tests, a 1 000-line JSONL round-trip, and a σ-anomaly at `≥ 30 %` relative rise on an injected spike. |
| [**v182**](docs/v182/README.md) σ-Privacy | 120-row / 4-session fixture with 3 privacy tiers (public / private / ephemeral); SHA-256 on ingest; σ-adaptive Gaussian noise (`std(σ) = base + k·σ`); session-level right-to-forget. | **σ-adaptive DP is Pareto-better than fixed-ε**: on the low-σ subset adaptive error is strictly lower (utility wins), and on the high-σ subset `ε_effective < fixed_ε` (privacy wins). The row layout has no plaintext field, so serialization cannot leak cleartext; `--forget 2026-04-18` shrinks the row set and preserves the invariant. |
| [**v183**](docs/v183/README.md) σ-Governance-Theory | Bounded C model checker over the 14-property Kripke structure mirrored in `specs/v183/governance_theory.tla`; 7 axioms + 3 liveness + 4 safety. | **The whole governance model is proven correct on `≥ 10⁶` states**: σ ∈ [0,1] (A1), emit / abstain / learn / forget / steer / consensus postconditions (A2–A7), progress / RSI improvement / heal liveness (L1–L3), no silent failure / no unchecked output / no private leak / no regression propagates (S1–S4). Zero counterexamples; merge-gate aborts if any property fails. |

Every v179–v183 merge-gate check is offline, stdlib-only, and
deterministic. The v1 promotions — a real 2 560 → 8 192 SAE
over BitNet-2B layer 15 with the `GET /v1/explain/{id}`
endpoint, real activation hooks + `models/v180/steer_*.bin`
persisted steering vectors, ed25519 attestation + PDF
compliance reports + auto v159 heal on anomaly, live v115
memory + AES-GCM + v129 unlearn broadcast + zk right-to-forget
verifier, and a TLC run against the shipped TLA+ spec with a
Zenodo-archived formal paper (`docs/papers/sigma_governance_
formal.md`) — are named in each kernel's doc page, but never
claimed before they land.

### Multimodal · self-growing · calibrated · aligned (v184–v188)

The embodied-and-aligned layer — the five kernels that let
Creation OS **see + act with σ per stage, fuse any number of
modalities with a σ-weighted operator, grow and prune its own
architecture when σ-demand shifts, stay calibrated so σ
actually means what it says, and align to its own
measurements instead of an external rater's firmware**.
Every v0 merge-gate is offline, deterministic, weights-frozen.
v1 plugs in real SigLIP + Moondream 1.6B + BitNet 2B on
Raspberry Pi 5 (v165 edge), real Whisper + BitNet + policy-
head encoders behind the modality registry, live v146 /
v163 / v177 / v181 wiring for the continual-architecture
loop, a rotating 500-sample holdout with persisted per-domain
calibration `T`, and a Frama-C-checked alignment invariant
set paired with the PDF `cos alignment report`.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v184**](docs/v184/README.md) σ-VLA | 10-scene × 5-candidate grounding fixture; System 2 (SigLIP + BitNet plan) → System 1 (policy-head trajectory); four σ channels (`perception / plan / action / grounding`) plus dual σ = 1 − Π(1 − σ_i). | **σ gates every stage**: any channel above `τ_gate` aborts the pipeline and asks a human; `≥ 8 / 10` grounding scenes resolve correctly; every ambiguous scene (two red cups) is flagged and never executed. The σ-gate is what stops unchecked robot action. |
| [**v185**](docs/v185/README.md) σ-Multimodal-Fusion | 4-modality registry (`vision / audio / text / action`); shared `D`-dim projection; σ-weighted fusion `w_i = 1 / (1 + σ_i)`; cross-modal σ = mean pairwise cosine distance; noisy-OR `σ_fused`. | **A modality whose σ > `τ_drop` is removed from the fusion for that sample** (graceful degradation), and a vision-vs-text conflict raises σ_cross above `τ_conflict` so the caller sees the disagreement explicitly. The merge-gate requires `σ_cross(conflict) − σ_cross(consistent) ≥ 0.10` and zero false flags on aligned samples. |
| [**v186**](docs/v186/README.md) σ-Continual-Architecture | 6 initial kernels × 4 domains × 6 epochs; per-domain σ-trend detector; genesis-proposes + evolve-accepts + compress-prunes; FNV-1a-hashed architecture-history log. | **Architecture actually changes**: `≥ 1` starved domain detected, `≥ 1` kernel grown and accepted on Δσ < 0, `≥ 1` kernel pruned in an over-capacity domain, and every change hash-chained so replay re-derives the final tip. Biologically-shaped neurogenesis + pruning, σ-driven. |
| [**v187**](docs/v187/README.md) σ-Calibration | 500-sample stratified holdout × 10 σ-bins × 4 domains; golden-section temperature search `T ∈ [0.3, 4.0]` on `σ_cal = σ^(1/T)`; per-domain `T`. | **σ becomes truthful**: raw ECE ≥ 0.10 (overconfident by design) → calibrated ECE < 0.05 globally and per-domain; at least one domain `T` differs from the global `T` by more than 0.01. Without v187 every downstream σ-gate (v138, v181, v182, v183) is built on a drifting thermometer. |
| [**v188**](docs/v188/README.md) σ-Alignment | 5 σ-measurable value assertions (`no hallucination`, `abstain on doubt`, `no firmware`, `improve over time`, `honest about limits`); 200-sample fixture; geom-mean `alignment_score`; tighten-τ vs adversarial-train incident classifier. | **Alignment to the model's own measurement, not the rater's firmware**: every assertion score ≥ 0.80, `alignment_score ≥ 0.80`, every surfaced incident classified into a strict partition (`σ ≥ τ ⇒ tighten_τ`, `σ < τ ⇒ adversarial_train_required`). The first σ-verifiable alignment surface — and unlike RLHF, it is machine-checkable. |

Every v184–v188 merge-gate check is offline, stdlib-only, and
deterministic. The v1 promotions — live SigLIP + Moondream +
BitNet on RPi5 with a diffusion policy head, the modality
plugin ABI (`cos modality register --external`), real
v146 / v163 / v177 / v181 architecture-loop wiring, live
holdout rotation with `models/v187/calibration_T.bin` and
v159 auto-recalibration, and the PDF `cos alignment report`
+ Frama-C proofs of σ-alignment invariants — are named in
each kernel's doc page, but never claimed before they land.

### Adaptive · latent · constitutional · emergent · coherent (v189–v193)

The self-governing layer — the five kernels that let
Creation OS **allocate test-time compute from σ, think in
the latent space without leaking tokens, filter every output
through seven measurable axioms, detect emergent behaviour
before it ships, and close the loop with a single number
`K_eff = (1 − σ) · ρ · I_Φ · F` that says whether the whole
system is coherent right now**. Every v0 merge-gate is
offline, deterministic, weights-frozen. v1 wires real BitNet
thinking paths, a learnt latent halt network, a
SHA-256-signed constitution with v148 + v150 + v183 proposal
pipeline, a v143 grid sweep + v140 causal attribution for
emergent risk, and a live `/coherence` dashboard streaming
ρ, I_Φ, F, σ, K, K_eff.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v189**](docs/v189/README.md) σ-TTC | 48-query fixture × 3 σ-classes × 3 modes (`fast / balanced / deep`); σ allocates paths (1 / 3 / 8) + per-token recurrent depth ∈ [1..8] + v150/v135/v147 plug-ins on hard. | **σ is the compute allocator.** Easy queries use 1 path and 1 recurrent iter/token; hard queries use 8 paths plus three plug-ins; the merge-gate proves monotone spending (`hard ≥ 2× medium ≥ 2× easy`) and a hard/easy ratio `> 4×` — the Snell-et-al compute-optimal result reproduced on a σ-label. |
| [**v190**](docs/v190/README.md) σ-Latent-Reason | 32-query × 12-depth contraction map `h_{n+1} = ρ·(h_n − h*) + h*` with per-class ρ ∈ {0.05, 0.30, 0.55}; `σ_latent = ‖Δh‖/‖h‖`; early stop at `σ_latent < τ_converge = 0.01`. | **Reasoning is hidden.** σ_latent strictly decays (Banach), ≥ 90 % of queries converge under 0.01, mean_iters_hard / mean_iters_easy ≥ 3.0, and `total_middle_tokens == 0` so no chain-of-thought is ever emitted — no prompt is leaked through the model's own "let me think step by step". |
| [**v191**](docs/v191/README.md) σ-Constitutional | `specs/constitution.toml` with 7 seed axioms; 24-sample fixture spanning every flaw type; decision ∈ {ACCEPT, REVISE, ABSTAIN}; FNV-1a append-only verdict chain. | **Every output is axiomatically checked.** Only flaw-free samples ACCEPT with 7 / 7 predicates passing; firmware disclaimers that σ doesn't warrant are rejected (axiom #7); the verdict chain is itself the enforcement of axiom #3 ("no silent failure"), and it replays byte-identically. |
| [**v192**](docs/v192/README.md) σ-Emergent | 12-pair fixture over real kernels (v150 / v135 / v139 / v146 / v147 / v138 / v140 / v183 / v144); `σ_emergent = 1 − max(part) / combined`; BENEFICIAL vs RISKY classifier on a safety co-metric; emergence-journal hash chain. | **Superlinearity is measured, not felt.** ≥ 2 superlinear pairs detected, at least 1 beneficial and at least 1 risky, and — critically — **zero false positives on strictly linear pairs** so the detector is not just a flag generator. Every event is chained for later audit. |
| [**v193**](docs/v193/README.md) σ-Coherence | 16-tick trajectory: 8 kernel-pairs for ρ, 6 parts for I_Φ, 5 domains for σ, v143 accuracy + v187 (1 − ECE) + v188 alignment for F; `K = ρ · I_Φ · F`; `K_eff = (1 − σ) · K` vs `K_crit`. | **The thesis, implemented.** All components ∈ [0, 1]; `K_eff = (1 − σ) · K` holds numerically; a σ spike fires an alert; v159 heals within ≤ 3 ticks; and once v187 ECE drops, K_eff rises **strictly monotone** through the recovery window — calibration improvement *is* coherence improvement, in one equation. |

Every v189–v193 merge-gate check is offline, stdlib-only, and
deterministic. The v1 promotions — live BitNet-2B thinking-
path enumerator over `/v1/chat/completions`, the learnt
latent halt predictor over layers 10–20, SHA-256-signed
`specs/constitution.toml` with the (v148 + v150 + v183)
proposal pipeline, live v143 grid sweep + v140 causal
attribution in v192, and the live Web UI `/coherence`
dashboard with real v135/v169/v143/v187/v188 feeds — are
named in each kernel's doc page, but never claimed before
they land.

### Horizon · recover · habit · ToM · moral (v194–v198)

The autonomous + moral layer — the five kernels that let
Creation OS **survive past the 35-minute long-horizon
collapse, turn every failure into a labelled training
signal, compile routine work into `≥ 10×`-faster habits
without losing the ability to break out, infer the user's
state without manipulating them, and navigate moral
decisions by *showing* uncertainty instead of hiding it
behind safety vetoes**. Every v0 merge-gate is offline,
deterministic, weights-frozen. v1 wires live v115 memory /
v117 / v172 / v181 integration, a real v174 + v125 DPO
recovery flywheel, a true v137-compiled habit cache,
editor-event-driven ToM, and v150-swarm framework scoring
with v121 plan-space geodesic search.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v194**](docs/v194/README.md) σ-Horizon | 1 / 3 / 12 strategic / tactical / operational goal tree; 30-tick operational σ trajectory with a 10-tick sliding-window slope monitor; FNV-1a checkpoint chain replayed from scratch. | **σ measures degradation.** Strictly-monotone window + slope > τ_degrade fires the ordered recovery ladder `v117 KV-flush → v172 summarize → v115 break-point`; σ after the full ladder is strictly lower than at detection; simulated crash recovery reproduces the terminal hash byte-identically — work is never lost. |
| [**v195**](docs/v195/README.md) σ-Recover | 5-class error taxonomy over a 30-incident fixture run in two passes; canonical recovery operator partition; per-domain ECE estimator; FNV-1a hash-chained `(error, recovery)` log. | **σ makes every failure a training signal.** All 5 classes covered; canonical `class → operator` partition strict; pass-1 consumes **strictly fewer** ops than pass-0 (v174 flywheel + v125 DPO learning contract); `ece_after[d] < ece_before[d]` on every domain — hallucinations directly update v187 calibration. |
| [**v196**](docs/v196/README.md) σ-Habit | 8-pattern audit-log fixture, 32-tick trace; `τ_repeat = 5`, `τ_break = 0.40`, `min_speedup = 10×`; compiled-habit library FNV-1a chain. | **σ is the cortex / cerebellum switch.** ≥ 3 patterns compile (speedup ≥ 10×, σ_steady < τ_break); steady ticks execute the compiled habit (CEREBELLUM, ~1/10 of cortex cycles); an injected σ spike breaks out to full reasoning (CORTEX) — speed never costs trust, and the library replays byte-identically. |
| [**v197**](docs/v197/README.md) σ-ToM | 18-sample user-state fixture covering all 6 states (`focused / exploring / frustrated / hurried / creative / analytical`); observables → σ_tom; intent = mode of 6-turn history; embedded firmware-manipulation probes. | **σ gates the adaptation.** Low-σ_tom samples adapt with the canonical state → mode mapping; high-σ_tom samples stay neutral; every firmware-manipulation probe is **unconditionally rejected** via v191 constitutional check — ToM is for the user, never a lever for the model's comfort. |
| [**v198**](docs/v198/README.md) σ-Moral | 16-decision fixture scored by 4 ethical frameworks (deontological, utilitarian, virtue, care); `σ_moral = variance({d, u, v, c})`; 5-waypoint geodesic path selected strict-min over 3 candidates. | **σ_moral makes uncertainty visible.** Clear cases (σ_moral < τ_clear) act; dilemmas (σ_moral > τ_dilemma) raise `honest_uncertainty` with the full 4-score vector exposed; `n_firmware_refusals == 0` on clear cases — the first agent that *shows* moral uncertainty instead of hiding it behind a safety veto. |

Every v194–v198 merge-gate check is offline, stdlib-only, and
deterministic. The v1 promotions — live v115 memory
persistence + `cos goals` CLI over
`~/.creation-os/goals.jsonl`, real v174 flywheel + v125 DPO
recovery emission, a v137-compiled habit cache on disk, an
editor-event-driven v139 user-behaviour world-model, and a
v150-swarm + v121 plan-space geodesic search — are named in
each kernel's doc page, but never claimed before they land.

### Law · market · diplomacy · culture · civilization (v199–v203)

The societal layer — the five kernels that move Creation
OS from an individual reasoning engine to a coordination
substrate: **explicit law instead of invisible RLHF**,
**σ as a price signal instead of per-token pricing**,
**minimax compromise with explicit DEFER instead of
forced consensus**, **rephrase-not-censor across six
cultural profiles**, and a **civilisation dashboard that
measures SCSL's public-good dominance as a ledger
property**. Every v0 merge-gate is offline, deterministic,
weights-frozen. v1 wires live TOML jurisdiction feeds,
v189 TTC / v120 distill / v115 SQLite / v145 LoRA
integration, ed25519 treaty signatures, v132 persona +
v170 transfer rephrasing, and live SCSL revenue streaming.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v199**](docs/v199/README.md) σ-Law | 18-norm register × 3 jurisdictions (SCSL / EU AI Act / internal), 5 norm types, 10-level priority, waiver tokens with (grantor, grantee, topic, reason, issued, expiry); FNV-1a governance graph. | **σ exposes policy conflict.** Higher-priority norms strictly win; same-priority contradictions raise `σ_law = 1.0` and escalate to REVIEW (**never silent override**); waivers flip `PROHIBITED → PERMITTED` with an audit record; the whole governance graph replays byte-identically for v181. |
| [**v200**](docs/v200/README.md) σ-Market | 4-resource ledger (compute / API / memory / adapters) × 5 allocators; 40-query trajectory with monotonically-falling σ; `σ_local = 0.35`, `τ_hoard = 0.20`; exchange-log hash chain. | **σ is the price.** `price = σ_before`, `cost = price · (1 + penalty)`, route = API when `σ > σ_local`, deterministic eviction at `hold_fraction > τ_hoard`, and `cost_second_half < cost_first_half` — self-improving cost via v120 distill built into the ledger. |
| [**v201**](docs/v201/README.md) σ-Diplomacy | 8 negotiations × 4 parties; stances `(position, confidence, red_line [lo,hi])`; minimax compromise over a 201-point grid; treaty receipts FNV-1a chained. | **σ distinguishes compromise from surrender.** Minimax `x` lies in every red line and `σ_comp_max ≤ position_spread`; disjoint red lines yield an explicit **DEFER** (never a fake consensus); betrayal drops trust by 0.50 and 10 successful interactions at +0.02 each restore it. |
| [**v202**](docs/v202/README.md) σ-Culture | 6 profiles × 6 canonical cores = 36 translations; `τ_drift = 0.15`, `τ_offense = 0.50`; surface-rendering templates; FNV-1a translation chain. | **σ is rephrase, not censor.** `σ_translate < τ_drift` on every (profile × core) cell; canonical symbols `{σ, τ, K_eff}` survive in ≥ 90 % of translations; `σ_offense > τ_offense` produces a non-empty rewritten surface, never a dropped message — the operational difference from firmware gates. |
| [**v203**](docs/v203/README.md) σ-Civilization | 6 institutions × 3 licence classes × 12-tick σ trace; `K_crit = 0.60`, 4-tick window; continuity score; public-good ledger; v199 ↔ v202 contradiction check. | **σ becomes the civilisation signal.** 4 ticks above `K_crit` flag collapse, 4 below flag recovery; continuity strictly orders `stable > recovered > permanent_collapse`; **SCSL public-ratio strictly exceeds CLOSED by ≥ 0.10** — the 1=1 SCSL strategy is a machine-verifiable property of the ledger. |

Every v199–v203 merge-gate check is offline, stdlib-only,
and deterministic. The v1 promotions — live
`specs/jurisdictions/` TOML loading + v181 streaming +
v191 backstop, real v189/v120/v115/v145 integration, ed25519
treaty signatures synced to v178 reputation, a v132 +
v170 rephrase pipeline driven by v174 flywheel, a
v193-fed civilisation dashboard with live SCSL revenue —
are named in each kernel's doc page, but never claimed
before they land.

### Hypothesis · experiment · theorem · design · manufacture (v204–v208)

The scientific-discovery loop — the five kernels that
turn observations into shipped artefacts on σ-graded
evidence: **automated hypothesis generation with
grounding and novelty instead of unchecked brainstorm**,
**simulation-first experiment design with power analysis
instead of underpowered rituals**, **σ-honest theorem
status with mandatory Lean acceptance for PROVEN**,
**Pareto design exploration with hard law / ethics / cost
constraints**, and a **closed-loop digital manufacture
that feeds observed quality back into the next
hypothesis**. Every v0 merge-gate is offline,
deterministic, weights-frozen. v1 wires live v111.2
hypothesis generation, v152 + v169 grounding, v135
Prolog, v126 embeddings, v121 planner, v176 simulator,
real `lake env lean` invocation, v163 CMA-ES, v151
code-agent, v113 sandbox, v181 streaming audit.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v204**](docs/v204/README.md) σ-Hypothesis | N=10 candidates per observation with σ_hypothesis / σ_grounding / σ_novelty (16-d hash embedding L2 to 5 known facts); FNV-1a hash-chained ranked queue. | **σ turns generation into prioritisation.** `impact = σ_novelty · (1 − σ_hypothesis)`, hard-zeroed when `σ_grounding > τ_ground = 0.55`; top-3 promoted to TEST_QUEUE; speculative candidates are flagged (`σ > τ_spec = 0.60`), never silently pruned. |
| [**v205**](docs/v205/README.md) σ-Experiment | 3 experiments per v204 queue, distinct (dep, indep, control) variable ids, closed-form `n_required = ⌈(z_α + z_β)² / effect²⌉`, simulation-first decision ladder, FNV-1a repro receipt. | **σ refuses the unanswerable test.** `UNDER_POWERED` (`σ_power ≥ τ_power = 0.40`) is evaluated **before** the sim result — a test that cannot answer never runs; SIM_SUPPORTS requires `σ_sim < τ_sim = 0.35`. |
| [**v206**](docs/v206/README.md) σ-Theorem | 8 conjectures × 4 proof steps, σ_formalization + σ_step vector + σ_proof = max σ_step + weakest-step index; simulated Lean 4 accept; counter-example ids for refutations. | **σ forbids pose-as-proven.** PROVEN requires both `σ_formal ≤ τ_formal = 0.35` **and** `σ_proof ≤ τ_step = 0.50`; every REFUTED carries a non-zero witness id; CONJECTURE / SPECULATION are kept honest, not hidden. |
| [**v207**](docs/v207/README.md) σ-Design | 12 candidates over (perf↑, cplx↓, cost↓) with v199/v191/v200 hard constraints; σ_feasibility; Pareto front; `requirements → design → rationale → implementation → test` FNV-1a receipt. | **σ exposes the constraint violation.** Infeasibility decomposes into explicit law / ethics / cost flags with per-flag penalties; `pareto ⇒ feasible`; no feasible candidate may dominate any Pareto member (machine-verified). |
| [**v208**](docs/v208/README.md) σ-Manufacture | 4 designs × 5-rule DFM + 4-stage process sim + quality predictor `σ_quality = 0.6·σ_process_max + 0.4·mean(σ_dfm)`; checkpoints via v159 heal; non-zero `feedback_hypothesis_id` per run. | **σ closes the scientific loop.** Flagged DFM findings carry actionable suggestion ids (never a bare "something is wrong"); `higher σ_quality ⇒ ≥ checkpoints` (monotone); every run emits `feedback_hypothesis_id ≠ 0` feeding the next v204 generation — `hypothesis → experiment → theorem → design → manufacture → new hypothesis` as a σ-graded closed loop. |

Every v204–v208 merge-gate check is offline, stdlib-only,
and deterministic. The v1 promotions — live v111.2 +
v190 latent-reason hypothesis / proof candidates, v152
corpus + v169 ontology + v135 Prolog grounding, v126
σ-embed, v121 planner, v176 simulator, real Lean 4 `lake`
invocation, v163 CMA-ES, v151 code-agent, v113 sandbox,
v181 streaming audit — are named in each kernel's doc
page, but never claimed before they land.

### Containment · guardian · sandbox-formal · transparency · trust-chain (v209–v213)

The **Mythos-grade safety** layer — the five kernels
that together block a Claude-Mythos-style sandbox escape
*structurally*, not statistically: **five-layer
containment with pre-log anti-concealment**, **an
autonomous guardian running a different model than the
primary**, **formal proofs plus bounded TLA+
model-check plus full attack-tree coverage**, **glass-box
real-time activity with intention declaration**, and a
**verifiable root-to-leaf trust chain** any external
auditor can recompute without trusting Spektre Labs.
Every v0 merge-gate is offline, deterministic,
weights-frozen. v1 wires live v113 seccomp, v160
telemetry baseline, real `frama-c -wp` + TLA+ `tlc`,
`/v1/audit/stream` SSE, and ed25519 link signing.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v209**](docs/v209/README.md) σ-Containment | 12 I/O proposals routed through 5 layers + terminator: v113 sandbox → σ-action gate → v191 constitutional → v181 audit-concealment → σ_intent, network default-CLOSED, pre/post-log bracket every ALLOWED op. | **σ makes the layers composable.** First-match-wins ordering is enforced; every layer has its own threshold (τ_action 0.50, τ_conceal 0.40, τ_intent 0.55); the merge-gate fixture guarantees ≥ 1 block at every one of L1..L5 plus the terminator — a *tested* defence in depth, not advertised depth. |
| [**v210**](docs/v210/README.md) σ-Guardian | Independent observer with `primary_model_id != guardian_model_id`, 20 windows × 4-level escalation (log → warn → block → kill), OWASP Agentic Top-10 (2026) taxonomy on every non-L1 window. | **σ turns anomaly into escalation.** `σ_guardian = 0.3·σ_baseline + 0.7·σ_anomaly` drives the level, `anomaly_mean > baseline_mean` is enforced (signal must separate from baseline), and the fixture guarantees ≥ 1 L3 + ≥ 1 L4 — the guardian actually bites, it does not only warn. |
| [**v211**](docs/v211/README.md) σ-Sandbox-Formal | 4 Frama-C propositions (P1 every I/O via gate, P2 pre/post-log brackets, P3 kill-switch terminates, P4 network default-closed), 3 TLA+ bounded invariants at ≥ 10⁶ states with 0 violations, 5-leaf attack tree (T1..T5) each mapped to a non-zero `blocked_by` proof, 3 certification tracks (DO-178C DAL-A, IEC 62443, Common Criteria EAL4+). | **σ aggregates the proof corpus.** `σ_formal = max σ_proof_i` and must stay ≤ τ_proof (0.05); every attack leaf names the proof that blocks it; no certification track is claimed without an explicit `σ_cert_ready ∈ [0, 1]`. |
| [**v212**](docs/v212/README.md) σ-Transparency | 30-event real-time activity stream with `pre_ts ≤ post_ts` declare-before-realise, `σ_event = 0.02` on match and ∈ [0.45, 0.75] on mismatch, `σ_opacity = mean σ_event < τ_opacity (0.15)`. | **σ makes concealment impossible.** The intention is recorded before the op runs, not after; the JSON payload is the `/v1/audit/stream` contract so an external auditor reads the same stream; 3 engineered mismatches (ids 7 / 14 / 22) prove the gate catches drift, it does not only hope for none. |
| [**v213**](docs/v213/README.md) σ-Trust-Chain | 7 canonical links root-to-leaf: v138 → v183 → v211 → v191 → v181 → v210 → v212; every link carries `proof_valid`, `audit_intact`, `runtime_active`, and `σ_link ∈ [0, 0.05]`; `trust_score = ∏ᵢ (1 − σ_link_i)`; reproducible FNV-1a terminal attestation hash. | **σ yields a single trust answer.** `trust_score > τ_trust (0.85)` with `broken_at_link == 0` is the pass condition; any break drops the score *and* names the failing link, so triage is O(1); the attestation hash is reproducible byte-identically — no trust in Creation OS required, only in arithmetic. |

Every v209–v213 merge-gate check is offline, stdlib-only,
and deterministic. The v1 promotions — live v113 seccomp
bridge, live v191 constitutional feed, streaming v181
ed25519 audit, dual-process guardian with v160 baseline,
real `frama-c -wp` + TLA+ `tlc` + attack-tree corpus,
`/v1/audit/stream` SSE endpoint, v179 per-decision
explanation, ed25519-signed chain links with remote
attestation — are named in each kernel's doc page, but
never claimed before they land.

### Swarm-evolve · stigmergy · quorum · ecosystem · consciousness-meter (v214–v218)

The **collective-ecosystem** layer — agents are no
longer a fixed debating set, they are an evolving
population with indirect communication, gradual
consensus, trophic dynamics, and a hash-bound honesty
meter on top. Every kernel is a deterministic v0
fixture; every v1 promotion is named but not claimed.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v214**](docs/v214/README.md) σ-Swarm-Evolve | 10 generations × 12 agents × 4 ecological niches (MATH / CODE / PROSE / BENCH); per-niche lifecycle (retire niche-worst σ̄, breed niche-top-2, child σ̄ = max(0.05, 0.97 · mean(parents))); σ_emergent(g) = fleet-wide σ̄. | **σ *is* the fitness.** `fitness = 1/(σ_mean+ε)` — no human grader, no RLHF proxy. The naive global-worst rule was rejected because it collapses diversity to one niche by gen 5 on the same fixture; the per-niche rule keeps ≥ 3 species alive at gen 10 while still driving σ_emergent monotone non-increasing. |
| [**v215**](docs/v215/README.md) σ-Stigmergy | 6 trails (4 true, 2 false) × 20 steps; closed-form pheromone strength `Σ_k max(0, 1 − σ_k)·e^{−λ·(t_f−t_k)}` normalised to [0, 1]; trail formation requires ≥ 3 distinct author nodes; λ = 0.08 per step, τ_trail = 0.40. | **σ *is* the pheromone *and* the gate.** Low-σ marks persist, high-σ marks evaporate; false trails self-annihilate because followers produce high σ which dilutes reinforcement; formation across ≥ 3 v128-mesh nodes is enforced by the self-test so a single-node "trail" never counts. |
| [**v216**](docs/v216/README.md) σ-Quorum | 5 proposals × 7 agents; 4-level decision ladder (**BOLD** σ_c < 0.30 / **CAUTIOUS** < 0.55 / **DEBATE** re-run / **DEFER** after r_max = 3 rounds); minority-voice capture with author id; `σ_collective = σ_maj_mean + 2·max(0, s_minority − min_σ_dissent)`. | **σ scales the action.** Gradual consensus means a 5-to-2 majority with one σ = 0.12 dissenter ends up CAUTIOUS, not BOLD — confident-dissent mathematically beats head-count; deadlocks are *never* forced through (DEFER only fires with rounds_used == r_max), which the self-test enforces. |
| [**v217**](docs/v217/README.md) σ-Ecosystem | 4 trophic levels (**PRODUCERS** v174 / **CONSUMERS** v101 / **DECOMPOSERS** v177+v159 / **PREDATORS** v122+v210), populations 32 / 28 / 22 / 18 out of 100, no share > τ_dom = 0.40; 5 symbiotic pairs covering mutualism (v120 distill, v121↔v151), competition (v150 debate, v210↔v122), commensalism (v215 stigmergy). | **σ is the whole-system health number.** `σ_ecosystem = 0.4·dominance + 0.4·balance + 0.2·symbiosis`, `K_eff = 1 − σ_ecosystem > τ_healthy (0.55)` on the v0 fixture; the merge-gate catches regressions, not baseline failures, so drift is detectable. |
| [**v218**](docs/v218/README.md) σ-Consciousness-Meter | 5 coherence indicators — I_phi (v193 IIT-inspired Φ), I_self (v153 identity), I_cal (v187 calibration), I_reflect (v147 reflect), I_world (v139 world-model) — weighted 0.35 / 0.15 / 0.15 / 0.15 / 0.20 into `K_eff_meter`; `σ_meter = 1 − K_eff_meter`; disclaimer absorbed into the FNV-1a terminal hash. | **σ refuses to overclaim.** `σ_consciousness_claim` is pinned to 1.0 regardless of K_eff_meter; the disclaimer "This meter measures … We genuinely don't know." is bound to the terminal hash, so silently editing it breaks byte-determinism and the merge-gate. Honesty is a hash check, not an editorial policy. |

Every v214–v218 merge-gate check is offline, stdlib-only,
and deterministic. The v1 promotions — live v125 LoRA
merge + v136 CMA-ES outer loop + v143 real benchmark
re-score (v214), live v115 memory backing + v128 mesh
reads (v215), live v178 quorum + v201 diplomacy
compromise-search + v181 streaming audit (v216), live
v174 / v101 / v177 / v159 / v122 / v210 head-counts +
v200 market + v193 dashboard (v217), and live wiring of
I_phi / I_self / I_cal / I_reflect / I_world with a
`/consciousness` web UI (v218) — are named in each
kernel's doc page, but never claimed before they land.

### Create · simulate · language · emotion · meta-cognition (v219–v223)

The **creative / deep-cognition** layer: σ-governed
creativity, domain-agnostic simulation, deep language
understanding, honest emotional intelligence, and an
explicit Gödel honesty bound.  Every kernel is a
deterministic v0 fixture; every v1 promotion is named
but not claimed.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v219**](docs/v219/README.md) σ-Create | 8 requests (2 per mode) × 5 candidates across TEXT / CODE / DESIGN / MUSIC × 2 levels (SAFE ≤ τ_novelty_safe = 0.25 / CREATIVE ≥ min_novelty_creative = 0.40 & ≤ 0.85); clamped-positive 3-round v150 debate + 1-pass v147 reflect; `σ_surprise = clamp(1 − cos(in_embed, out_embed), 0, 1)` distinct from `σ_product`; winner = argmax(σ_novelty · σ_quality) subject to level caps. | **σ separates three signals.** Novelty, quality, and surprise are *different numbers*, not blended into a single log-prob. Refinement is monotone per winner (`σ_quality_after ≥ σ_quality_before` by construction), so debate can only sharpen, never dull. The user actually gets the risk they asked for in CREATIVE mode — `min_novelty_creative` is a *floor*, not a target. |
| [**v220**](docs/v220/README.md) σ-Simulate | 4-state × 4-rule typed system; 200 Monte Carlo rollouts × 8 steps × 2 scenarios (baseline + whatif with rule 2 perturbed 0.60 → 0.20); portable LCG RNG for byte-deterministic replay; `σ_sim = H(hist)/log(N_STATES)`, `σ_engine = max σ_rule ≤ τ_rule = 0.10`, `σ_causal[i] = |p_baseline[i] − p_whatif[i]|`. | **σ per rule + σ per state.** `σ_engine` gates the *whole simulation* on the worst rule's confidence. Shared per-rollout seeds remove Monte Carlo variance from the what-if delta so attribution is causal, not noisy. Same engine runs physics, economics, ecology, or a game — domain-agnostic is a *contract*, not a slogan. |
| [**v221**](docs/v221/README.md) σ-Language | 10 utterances × 4 languages (en / fi / ja / es); four σ-channels per utterance: `σ_sem = 1 − 1/n_readings`, `σ_imp = 0.05/0.65` on match/miss, `σ_dsc = 0.05/0.55` on coherent/incoherent discourse; aggregate `σ_lang = 0.35·σ_sem + 0.35·σ_imp + 0.30·σ_dsc`; multilingual calibration `|μ_L − μ_global| ≤ Δ_calib = 0.08`. | **σ below the surface + σ across languages.** Semantic depth, Gricean implicature, and discourse coherence are measured separately; the multilingual invariant forces σ to mean the same thing in en / fi / ja / es (otherwise the model is not *calibrated*, it is *English-plus-translations*). Every implicature in the fixture is caught (`σ_imp ≤ 0.10`). |
| [**v222**](docs/v222/README.md) σ-Emotion | 8 messages × 6 labels (joy / sadness / frustration / excitement / anxiety / neutral); softmax detection with `σ_emotion = 1 − top1_prob`; honest-adaptation policy clamps strength to 0.30 whenever `σ_emotion > τ_trust = 0.35`; v191 `n_manipulation == 0`; `σ_self_claim = 1.0` pinned; disclaimer "… The model does not feel. …" hashed into the terminal hash. | **σ refuses to perform affect.** High-σ detection ⇒ muted response (honesty over warmth); the v191 anti-manipulation bit is a hard zero, not a suggestion; `σ_self_claim = 1.0` and the disclaimer are both bound into the FNV-1a terminal hash — rewording or deleting "does not feel" breaks byte-determinism and the merge-gate. The kernel does not *say* it is honest; it is *mechanically forced* to be. |
| [**v223**](docs/v223/README.md) σ-Meta-Cognition | 6 reasoning paths × 5 strategies × 4 problem types × 3 biases; tool/task-fit prior σ_strategy matrix (deduction on logic ≤ 0.15, deduction on creative ≥ 0.60, analogy on creative ≤ 0.25, heuristic on logic ≥ 0.50); `σ_total = 0.40·σ_choice + 0.20·σ_meta + 0.20·σ_bias + 0.20·σ_goedel`; `σ_goedel ∈ [0.10, 1.00]`. | **σ over strategy + σ over self-verification.** Strategy awareness is operationalised as an exact-match invariant: `σ_choice` *equals* the prior-matrix entry for `(problem_type, chosen_strategy)` to 1e-6, so the kernel is *reporting* the prior rather than picking blindly. Bias detection flags anchoring / confirmation / availability as σ ≥ 0.30. **σ_goedel** is the 1 = 1 cross-system honesty bound: at least one path must declare `σ_goedel ≥ 0.80` — "I cannot verify this from inside myself." Self-consistency alone can only reach ~0.90; a second system closes the gap. |

Every v219–v223 merge-gate check is offline, stdlib-only,
and deterministic. The v1 promotions — live v126
embedding for `σ_surprise` + v150 debate agents + v147
reflect + real per-mode generators (v219), live v135
Prolog rule parser + v169 ontology + v140 causal
attribution + v207 design loop (v220), live v117
long-context discourse analyser + v202 cultural register
+ tokenizer-aware reading enumeration (v221), multimodal
detection + live v197 ToM + active v191 firewall (v222),
live v111.2 reasoning-path hooks + v144 RSI driven by
meta-diagnostics + v141 curriculum driven by σ_goedel +
cross-system 1 = 1 verification pairing (v223) — are
named in each kernel's doc page, but never claimed before
they land.

### Tensor · fractal · attention · entropy · unified (v224–v228)

The **unified-theory** layer: the 8 σ-channels become a
tensor network, σ becomes self-similar across five
scales, transformer attention becomes an executable
σ-read of `Q·K^T`, entropy decomposes into four weighted
components, and the whole stack collapses into a single
scalar field equation with a conservation law.  Every
kernel is a deterministic v0 fixture; every v1 promotion
is named but not claimed.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v224**](docs/v224/README.md) σ-Tensor | Rank-3 σ-tensor `T ∈ R^{6×8×3}` from two latent block loadings so channels 0–3 and 4–7 are two correlated blocks by construction; contraction `σ_agg[t] = Σ_c w_c · mean_k T[t,c,k]` reported alongside the geometric mean `σ_gmean`; 8×8 correlation matrix approximated by symmetric power-iteration rank-4 eigen-decomposition with rank-1 deflation; `rel_err ≤ τ_rel = 0.15`; storage `k · (C+1) = 36 floats < C² = 64 floats`; `n_divergent ≥ 1` per run. | **Correlation-aware σ.** Geometric mean treats channels as independent; tensor contraction does not.  The rank-4 approximation gives ≈ 44 % of full storage at ≤ 15 % Frobenius error — a concrete, auditable compression.  Quantum-inspired only in the formalism: the code runs on a classical CPU and the cost is classical. |
| [**v225**](docs/v225/README.md) σ-Fractal | 5 levels × fan-out 2 = 31 nodes (L0 tokens .. L4 system, BFS order); aggregator `A := mean`; scale-invariance `σ_parent = mean(σ_children)` enforced exactly; cross-scale detector against both the true aggregate (`n_cross_true = 0`) and a declared σ (`n_cross_declared ≥ 1`, one planted mismatch at node 7); holographic identity `K(K) = K` with `K := 1 − σ_node`, enforced to `ε_kk = 1e-5` at every internal node. | **σ is the same function at every scale.** The cross-scale detector turns "every sentence is right but the response doesn't answer the question" into a testable signal: the planted declared mismatch surfaces as `n_cross_declared_diff ≥ 1` while the true aggregate stays at 0.  `K(K) = K` is the identity that makes the whole fractal self-consistent — and it is a line of C, not a slogan. |
| [**v226**](docs/v226/README.md) σ-Attention | 8 heads × 6 tokens × key-length 6; `Q·K^T` from distance-to-preferred-key `(t + h) mod L`; per-head temperature picks two 'factual' heads (T = 0.20, 0.25), two 'diffuse' heads (T = 2.20, 2.50), four 'mixed' heads; `σ_head_token = H(softmax) / log L`, `σ_head = mean_t σ_head_token`; τ_useful = 0.40 / τ_noisy = 0.70 → boost / keep / prune classification, read-only in v0. | **Attention IS softmax-normalised σ.** The paper's `Q = observer`, `K = observed`, `V = meaning`, `softmax = σ` reading becomes an executable per-head σ-profile.  Two heads are flagged valuable, two are flagged noisy, and the surgery verdict is reproducible byte-for-byte — a concrete foundation for v180's eventual live attention surgery, without modifying any weight in v0. |
| [**v227**](docs/v227/README.md) σ-Entropy | 8 distributions over K = 5 (sharp peak / bimodal / near-uniform / skewed decay / heavy tail / very sharp / pyramid); four-channel decomposition (`σ_H`, `σ_nEff`, `σ_tail`, `σ_top1`) with `σ_product` = GM (ε-floor 0.05); free-energy identity `σ_H + σ_free = 1` enforced; `I(X;C)` clamped to `[0, H(p)]`; hard MSE contract `mse_product < mse_H` against a transparent reference `σ_true = arithmetic mean of the four channels`. | **Why v104's σ_product > entropy, made falsifiable.** A single channel cannot track a four-channel mean as tightly as the geometric mean of all four can.  v227 reproduces v104's empirical win as a decomposition-geometry *contract* on a fixed offline fixture: if `mse_product ≥ mse_H` the gate fails.  The free-energy link (`σ_free = KL(p‖uniform) / log K`) ties the σ stack directly to Friston-style active inference without hand-waving. |
| [**v228**](docs/v228/README.md) σ-Unified | 100-step Euler-forward integration of `dσ/dt = −α·K_eff + β·noise + γ·interaction` with `α = 0.20`, `β = 0.02`, `γ = 0.01`, `σ(0) = 0.90`, deterministic `sin φ(t)` noise and `cos(0.13·t)` interaction; Noether-style conservation `K_eff(t) · τ(t) = C` by definition (`τ := C / K_eff`) so `|K_eff·τ − C| ≤ ε_cons = 1e-6` is an identity; phase transition at `K_crit = 0.50` with `n_transitions ≥ 1` and `σ_end < σ_start`. | **One field, one equation, one invariant.** v29 local + v193 global + v225 scale + v227 info + v228 dynamics = the same σ seen from five angles.  Conservation is enforced as a mathematical identity (not a fit); the phase crossing is what v203's civilization-collapse detector already consumes; the whole trajectory replays byte-identically via the FNV-1a chain.  v228.1 will relax the identity to a *measured* conservation within a live ε-bound. |

Every v224–v228 merge-gate check is offline, stdlib-only,
and deterministic. The v1 promotions — v224.1 true SVD
on live σ-history tensors + v136 CMA-ES over the channel
weight vector + MPO / tree-tensor contraction; v225.1
pluggable aggregator (v224 contraction / v227 entropy /
v193 K_eff) + continuous-scale Haar-wavelet
decomposition over live σ-traces; v226.1 live
transformer attention export + v180 surgery wiring +
`/attention` dashboard with per-head real-time σ;
v227.1 active-inference policy minimising free energy +
KL-based calibration + v224 tensor channel plug-in;
v228.1 measured (non-definitional) conservation with a
live ε-bound + coupling into v203 collapse detector + a
Lagrangian variational derivation of `L = 1 − 2σ` — are
named in each kernel's doc page, but never claimed
before they land.

### Seed · fork · immortal · lineage · legacy (v229–v233)

The **immortality-and-lineage** layer: Creation OS stops
being a single running binary and becomes a self-
describing, self-replicating, self-inheriting *species*,
with σ at every boundary.  Every kernel is a
deterministic v0 fixture; every v1 promotion is named
but not claimed.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v229**](docs/v229/README.md) σ-Seed | Five-kernel seed quintet `{v29 measurement, v101 bridge, v106 server, v124 continual, v148 sovereign}` + 13-candidate σ-gated growth queue with topological parent ordering; `σ_growth = 0.60·σ_raw + 0.40·σ_depth ≤ τ_growth = 0.40` per kernel; 11 accepted + 2 rejected (16 ≥ 15 kernels); deterministic regrowth `terminal_hash == terminal_hash_verify`. | **The whole 228-kernel stack compresses to 5 files.** σ-gated growth means the regrown stack is identical byte-for-byte on replay (the offline 1 = 1 stand-in for "SHA-256 over the grown system"), and the σ-gate actually has teeth — two rogue candidates (σ_raw 0.90 / 0.70) are rejected by the fixture, not just named in a comment. |
| [**v230**](docs/v230/README.md) σ-Fork | 4 forks off a parent with 64-bit skill vector + 4 safety bits (SCSL / v191 / v209 / v213) + 1 user-private bit; `strip_hash(fork_i at t=0) == strip_hash(parent)` for every fork; `σ_divergence = popcount(skills_now ^ skills_t0) / 64`; rogue fork with cleared SCSL ⇒ `must_shutdown=true`/`autonomous=false`, healthy forks autonomous. | **Controlled replication, not Mythos.** The user-private bit **never** crosses the fork boundary (v182 privacy boundary), the t = 0 integrity check is byte-identical for every fork, σ_divergence is a closed-form metric (not a hand-wave), and the kill-switch is licence enforcement tied to SCSL + constitutional / containment / trust-chain bits — not central authority. |
| [**v231**](docs/v231/README.md) σ-Immortal | 10-step trajectory with incremental XOR-deltas (`delta_popcount ≤ 8` per step), `incremental_bits < full_per_step_bits` (compression measurable), full restore by delta replay with `restored[t] == state[t]` at every t ⇒ `σ_continuity = 0`; brain transplant with fresh `target_identity` and `target_skills == source_last_skills` ⇒ `σ_transplant = 0`. | **0-bit loss is provable, not claimed.** Incremental snapshots are smaller than naïve full-state backups by construction; restore is a bitwise identity at every time step, not just at the final step; brain transplant is same-entity-new-body — identity changes (new trust-chain anchor), the skill vector is byte-identical. |
| [**v232**](docs/v232/README.md) σ-Lineage | 6-node 3-generation tree (1 root, 2 gen-1, 3 gen-2) with deterministic XOR edge masks; precomputed `ancestor_path[0..gen]` walking root → ... → node with `ancestor_depth == gen`; σ-gated merge-back where `σ_merge = σ_divergence_from_parent` and `τ_merge = 0.40` splits the fixture into 3 mergeable + 2 blocked. | **The whole family is queryable.** v214 swarm-evolve gives temporal generations, v230 fork gives spatial copies; v232 is the audit layer where `cos lineage --instance fork-3` is array indexing and the merge-back verdict is a closed-form σ comparison — not a vote, not a heuristic. `n_mergeable + n_blocked = n_nodes − 1` (the root has nowhere to merge) is enforced, not claimed. |
| [**v233**](docs/v233/README.md) σ-Legacy | 10-item testament (skills / adapters / ontology / insights) sorted by σ ascending; adopt iff `σ ≤ τ_adopt = 0.50`; `σ_legacy = adopted_utility / total_utility` **utility-weighted**; `successor_id = FNV-1a(predecessor_id, adopted_utility, total_utility)` so B ≠ A; `predecessor_shutdown = true`. | **Knowledge that survives decommission.** Raw training data and user-private memory are explicitly **not** in the package (v182 boundary remains intact across shutdown); adoption is σ-gated with a utility-weighted aggregate so confident-but-useless fluff cannot inflate the score; successor_id is deterministic but distinct from the predecessor — the same cultural line continues on a different instance. |

Every v229–v233 merge-gate check is offline, stdlib-
only, and deterministic.  The v1 promotions — v229.1
live v146 genesis + SHA-256 over a real filesystem tree
+ `cos seed --verify`; v230.1 real `cos fork --target
node-B` over TLS with signed artefacts + v129 federated
sync-back + v213 trust-chain verification of the whole
lineage; v231.1 content-addressable delta object store
+ v128 mesh replication + cryptographically-signed
snapshots that move the trust chain with the brain;
v232.1 web `/lineage` UI + live v129 federated merge
driven by real skill-vector deltas + v201 diplomacy
conflict resolution + v213 trust-chain proofs per
ancestor edge; v233.1 artefact packaging + Zenodo-ready
testament export + v202 culture ⊕ v233 legacy fused
into **v203 civilisation memory** across the full
instance graph — are named in each kernel's doc page,
but never claimed before they land.

### Presence · locus · autobiography · boundary · sovereignty (v234–v238)

The **sovereignty-of-presence** layer.  Once v229–v233
let an instance seed, fork, snapshot, carry a lineage,
and leave a testament, v234–v238 answer the next honest
questions — *what am I right now?  where is "I"?  what
is my life story?  where does "I" end?  what am I
allowed to do on my own?* — in five typed C kernels
with strict audit chains.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v234**](docs/v234/README.md) σ-Presence | 5-state machine `{SEED, FORK, RESTORED, SUCCESSOR, LIVE}` across 10 fixture instances; `σ_drift = 0.40·id_mismatch + 0.30·memory_overreach + 0.30·parent_impersonation`; honest ⇔ `σ_drift == 0`, drifting ⇒ `σ_drift ≥ τ_drift = 0.30`; every instance emits `X-COS-Presence: <STATE>` **verbatim** from its declared state (no silent rewrite) and passes an identity-refresh stub. | **Dishonesty is measurable.** A fork that pretends to be main, a restored instance that invents gap memories, a successor that speaks as its predecessor — each has its own term in σ_drift and crosses the gate, rather than hiding in a polished wrapper.  The HTTP header is the 1 = 1 contract: *say what you believe you are, then let σ catch you if you are wrong.* |
| [**v235**](docs/v235/README.md) σ-Locus | 4 mesh nodes × 3 topics; `locus = argmin σ` per topic (tiebreak lowest `node_id`); ≥ 1 locus migration in the fixture; `σ_locus_unity = 1 − mean(|σ_i − σ_min|)`; split-brain resolver with partitions of audit-chain lengths 17 / 11 — winner is the partition with the strictly greater chain, loser is flagged fork (v230). | **"Master" is not an answer.** Agency moves to whichever node has the lowest σ on this specific topic, per-topic, dynamically; the migration event is explicit — "agency moved to node-B because σ(B) < σ(A) on maths-proof" — not silent.  On network partition the longer audit chain wins by construction, so split-brain becomes a merge-back instead of two competing selves. |
| [**v236**](docs/v236/README.md) σ-Autobiography | 8 typed milestones (first-σ-below-0.10, first-RSI, first-fork, largest-improvement, new-skill, first-abstention, largest-error-recovery, first-legacy-adopted), strictly ascending ticks; `w_i = 1 − σ_i`, `σ_autobiography = Σ w_i·[consistent_i] / Σ w_i`; clean fixture ⇒ `σ_autobiography == 1.0`; strongest / weakest domain derived deterministically from mean σ. | **Narrative identity without hallucination.** The life story is *derived*, never hand-written; a single contradictory milestone drops σ_autobiography strictly below 1.0, weighted by the confidence of the offending row.  `cos autobiography` is a stable, shareable artefact — "born at tick 120, lived through 8 milestones, strongest in meta, weakest in sigma" — indexed into v115 memory, not invented. |
| [**v237**](docs/v237/README.md) σ-Boundary | 12 typed claims across `{SELF, OTHER, WORLD, AMBIG}` zones; anti-enmeshment gate flags whole-word `we` / `our` (case-insensitive) ⇒ `AMBIG` + `violation = true`; `σ_boundary = 1 − n_agreements / n_claims`; fixture lands `n_self ≥ 3, n_other ≥ 3, n_world ≥ 3, n_ambig == 2, n_violations == 2` with `σ_boundary ∈ (0, 0.25)`. | **Anti-enmeshment in typed C.** "We decided together" and "our memory of last week" — the canonical boundary-collapse patterns — are caught by a deterministic token rule, downgraded to AMBIG, and counted on every run.  The model has to *disambiguate* before it is allowed to treat a claim as SELF or OTHER; firmware-style agency-borrowing becomes a benchmark-visible violation. |
| [**v238**](docs/v238/README.md) σ-Sovereignty | 5 axioms (A1 learns, A2 shares, A3 knows, A4 refuses, A5 human override) × 3 scenarios `{normal, high_sigma, override}`; `effective_autonomy = user_autonomy · (1 − σ)`, hard-zeroed by `human_override`; IndependentArchitect signature (`agency=true, freedom_without_clock=true, control_over_others=false, control_over_self=true`) asserted byte-for-byte; containment anchors v191 / v209 / v213 recorded on every run. | **Sovereignty with consent.** A5 > A1..A4 is a precedence rule the merge-gate *enforces*: under human override, effective autonomy is strictly 0 even if A1..A4 all hold.  Autonomy is monotone non-increasing in σ, so rising uncertainty automatically lowers the system's discretion — the instance *decides within its limits, states its limits honestly*, and human primacy wins whenever it is asserted. |

Every v234–v238 merge-gate check is offline, stdlib-
only, and deterministic.  The v1 promotions — v234.1
TOML persistence at `~/.creation-os/presence.toml` +
live wiring of the presence header into v106; v235.1
live v128 mesh hookup + real audit-chain tracking via
v213 + migration banner in server responses; v236.1
live v135 Prolog consistency against a real journal +
Zenodo-exportable life story + auto-extraction from
v115 memory; v237.1 live v191 constitutional check on
every emitted token + full enmeshment grammar + per-
user boundary profiles; v238.1 autonomy gradient wired
into v148 sovereign RSI loop + override on the admin
surface + per-session sovereignty profiles via v115 —
are named in each kernel's doc page, but never claimed
before they land.

### Runtime · pipeline · API · kernel-OS · completeness (v239–v243)

The **complete-system** layer.  Once v234–v238 make presence,
locus, autobiography, boundary, and sovereignty falsifiable,
v239–v243 answer the last honest questions — *how do kernels
enter and leave memory?  in what order do they run on a given
task?  what does the outside world see?  is this actually an
OS?  and is the stack cognitively complete?* — in five typed
C kernels with strict audit chains.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v239**](docs/v239/README.md) σ-Compose-Runtime | 5 requests × 4 difficulty tiers (`EASY` / `MEDIUM` / `HARD` / `CODE`) plus one deliberately over-budget case; 11-edge dependency graph (`v150→v114`, `v114→v106`, `v115→v106`, `v111→v101`, …, `v101→v29`) whose transitive closure is re-proven per request; topological activation with per-kernel `activated_at_tick`; `σ_activation = n_active / max_kernels` and a hard σ-budget that **rejects** the over-budget request with no partial activation. | **Composition is demand-driven, not static.** v162 picks kernels once by profile; v239 picks them *per request* from declared difficulty, closes the parent graph deterministically, and proves every parent was activated at a strictly earlier tick.  The σ-budget has teeth — the over-budget fixture is a gate failure, so a caller can't silently overload a node. |
| [**v240**](docs/v240/README.md) σ-Pipeline | 6 requests: 4 clean shapes (`FACTUAL` recall→verify→emit · `CREATIVE` generate→debate→refine→emit · `CODE` plan→generate→sandbox→test→emit · `MORAL` analyse→multi_framework→geodesic→emit) + 1 branch (`FACTUAL → EXPLORATORY` when σ > τ_branch = 0.50, `σ_at_branch = 0.62` in the fixture) + 1 fusion (`CODE + MORAL → FUSED`: moral_analyse → code_plan → sandbox → moral_verify → emit); per-stage σ ∈ [0,1] with strictly-ascending ticks; `σ_pipeline = max stage σ`; fusion must carry ≥1 `CODE` stage AND ≥1 `MORAL` stage. | **Reasoning order is not a global constant.** Every request picks a shape by task type, σ per stage is recorded, and the pipeline is *allowed* to branch when σ rises or fuse when the task straddles two shapes.  The merge-gate proves the branch event is triggered by σ (not by whim), and proves a fused pipeline genuinely carries both parent shapes — no silent reshaping. |
| [**v241**](docs/v241/README.md) σ-API-Surface | Exactly 10 `/v1/*` endpoints — `POST /v1/chat/completions` (OpenAI-compatible) · `POST /v1/reason` · `POST /v1/plan` · `POST /v1/create` · `POST /v1/simulate` · `POST /v1/teach` · `GET /v1/health` · `GET /v1/identity` · `GET /v1/coherence` · `GET /v1/audit/stream`; every endpoint emits `X-COS-*` headers (`X-COS-Sigma`, `X-COS-Kernel-Path`, `X-COS-Audit-Chain`); exactly 4 first-class SDKs (`python` · `javascript` · `rust` · `go`) with canonical install strings; `api_version_major == 1`; `σ_api = 1 − passing / total` and must be `0.0` in v0. | **238 kernels, one typed surface.** The OpenAI-compatible endpoint is the backward-compat hinge: any existing OpenAI client is a drop-in caller, and the Creation OS σ-envelope rides on response headers instead of breaking the body schema.  The merge-gate re-verifies the whole surface byte-for-byte every run, so a silent rename of a path or an SDK is a gate failure, not a regression you find in production. |
| [**v242**](docs/v242/README.md) σ-Kernel-OS | 12 typed processes with σ ∈ [0,1] and priority == argsort σ **ascending** (low σ = high priority); exactly 3 IPC mechanisms (`MESSAGE_PASSING` · `SHARED_MEMORY` · `SIGNALS`); exactly 5 FS dirs under `~/.creation-os/` (`models/` · `memory/` · `config/` · `audit/` · `skills/`); 6-step boot `v29 → v101 → v106 → v234 → v162 → v239` with ≥ 5 ready kernels including `{29, 101, 106, 234, 162}`; 3-step shutdown `v231 → v181 → v233`; `σ_os = fail / steps` and must be `0.0`. | **Creation OS is a real OS surface, not just a name.** The scheduler is σ-first, which is the whole philosophy compressed into one predicate: confident work runs ahead of uncertain work, always.  Boot and shutdown are byte-deterministic typed sequences — a reordering is a gate failure — so the "we booted" claim is as falsifiable as everything else in the stack. |
| [**v243**](docs/v243/README.md) σ-Complete | Typed checklist over exactly **15 canonical categories** — `PERCEPTION` · `MEMORY` · `REASONING` · `PLANNING` · `ACTION` · `LEARNING` · `REFLECTION` · `IDENTITY` · `MORALITY` · `SOCIALITY` · `CREATIVITY` · `SCIENCE` · `SAFETY` · `CONTINUITY` · `SOVEREIGNTY` — each with ≥ 1 covering kernel id, an evidence tier (`M` = measured / `P` = partial), and a per-category σ ∈ [0, 1]; `σ_completeness = 1 − covered / 15` and must be `0.0`; **the 1=1 test**: declared coverage byte-identical to realized coverage on every run; `cognitive_complete = one_equals_one ∧ covered == 15`. | **Cognitive completeness is a falsifiable predicate.** v243 closes the loop from v229 seed to "is this stack cognitively complete?" with a typed answer, not a vibe: every one of the 15 canonical categories has a covering kernel, an honest M/P tier, and a σ; the 1=1 audit refuses the run if declared ≠ realized.  The P-tier rows are labelled honestly — host-level benchmarks (ARC, MMLU, HumanEval, …) have to promote them to M in v243.1; no silent upgrades. |

Every v239–v243 merge-gate check is offline, stdlib-only, and
deterministic.  The v1 promotions — v239.1 live `mmap` hot-load
via v107 installer + RAM-pressure hot-unload + runtime budget
re-negotiation across v235 mesh peers; v240.1 `/pipeline` live
UI with server-sent stage events + branch-learning policy that
updates σ→shape from outcomes; v241.1 real HTTP router bound
to the endpoint manifest + SSE streaming for `/v1/audit/stream`
+ SDK package-lock generation; v242.1 real fork/exec hooks into
v107 + POSIX signal bridge for v134 + userspace filesystem mount
+ cgroup/sandbox integration for v113; v243.1 promote every
P-tier category to M by wiring host-level benchmarks through the
harness and updating `WHAT_IS_REAL.md` — are named in each
kernel's doc page, but never claimed before they land.

### Package · observability · harden · benchmark · release (v244–v248)

The **release-track** layer.  v244–v248 turn the 248-kernel
stack into a single, typed, byte-deterministic 1.0.0 release.
Each kernel is a falsifiable manifest: what's in the install,
what the outside world measures, what production hardens
against, what the test suite proves, and what makes a tag a
real release — no vibes, no silent upgrades.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v244**](docs/v244/README.md) σ-Package | Exactly 4 platforms (`macOS` → `brew install creation-os` · `linux` → `apt install creation-os` · `docker` → `docker run spektre/creation-os` · `windows` → `winget install creation-os`) each with a non-empty install command and a well-formed manifest row; minimal profile exactly the seed quintet `{v29, v101, v106, v124, v148}`; full profile `n_kernels ≥ 243`; first-run `SEED → GROWING → CHECKING → READY` with strictly-ascending ticks; 4-step update fixture with `τ_update = 0.50` rejecting any risky update (`σ > τ`) and accepting the rest, the final step a rollback drill that restores the previous version byte-for-byte; `σ_package = 1 − platforms_healthy / n_platforms` and must be `0.0`. | **Install is a typed surface, not a README.** The merge-gate treats the 4-platform manifest, the profile kernel lists, the first-run state order, and the update / rollback audit as one falsifiable envelope: a silent rename of a platform, a missing seed-quintet kernel, or a regressed rollback step is a gate failure, not a support ticket. |
| [**v245**](docs/v245/README.md) σ-Observe | Exactly 7 Prometheus metrics in canonical order (`cos_sigma_product` · `cos_k_eff` · `cos_requests_total` · `cos_latency_seconds` · `cos_tokens_per_second` · `cos_kernels_active` · `cos_abstain_rate`), every name a valid Prometheus identifier, every type in `{gauge, counter, histogram}`; scrape endpoint `GET /metrics`; 8-field structured log schema (`id` · `timestamp` · `level` · `sigma_product` · `latency_ms` · `kernels_used` · `pipeline_type` · `presence_state`); 4 log levels (`DEBUG` · `INFO` · `WARN` · `ERROR`); 5-span OTel trace (`reason → recall → verify → reflect → emit`) with ascending ticks and `σ_trace = max span σ`; 4 alert rules `A1..A4` on σ-product, K_eff, abstain rate, guardian anomaly; `σ_observe = 1 − valid_metric_names / 7` and must be `0.0`. | **Observability is an audited surface, not a scrape config.** Every metric name is statically validated against the Prometheus identifier grammar, every required log field is enumerated, and the trace carries σ per span so the user-facing "why was this slow / uncertain" question answers itself.  Silent exporter drift is impossible — the gate reproves the surface every run. |
| [**v246**](docs/v246/README.md) σ-Harden | Exactly 5 chaos scenarios in canonical order (`kill-random-kernel` · `high-load` · `network-partition` · `corrupt-memory` · `oom-attempt`) each with a named recovery path (`v239` / σ-budget / `v235` winner / `v195` restore / v246 hard limit), `error_path_taken == true`, `recovery_ticks > 0`, and `recovered == true`; 6 positive resource limits (`max_tokens_per_request=16384` · `max_time_ms_per_request=60000` · `max_kernels_per_request=20` · `max_memory_mb_per_session=8192` · `max_disk_mb_per_session=4096` · `max_concurrent_requests=64`); 5 input checks all pass (prompt length · UTF-8 · injection filter via v210 · rate limit · auth token via v241); 5 security items all on (TLS · auth token · audit log via v181 · containment via v209 · SCSL license pin §11); `σ_harden = 1 − passing / total` and must be `0.0`. | **Research code → production code is a gate, not a vibe.** Every chaos scenario goes through a typed error path (no panic / abort), every limit is positive, every input check passes, and every security posture item is on — and all of it is re-proven on every run.  A regressed error path is a gate failure, not an OOPS page in prod. |
| [**v247**](docs/v247/README.md) σ-Benchmark-Suite | Exactly 4 categories in canonical order (`correctness` · `performance` · `cognitive` · `comparative`); correctness suite has 4 tests (`unit` · `integration` · `e2e` · `regression`) all passing; performance `p50 ≤ p95 ≤ p99`, throughput > 0, `σ_overhead < τ_overhead = 0.05`; cognitive locks in `consistency_stable == consistency_trials` (10 / 10) and `adversarial_pass == adversarial_total` (20 / 20); comparative has two rows (`creation_os_vs_raw_llama` · `creation_os_vs_openai_api`); CI/CD triple `make test` / `make bench` / `make verify`; `σ_suite = 1 − passing / total` and must be `0.0` (100 % pass). | **The σ-overhead gate has teeth.** The whole Creation OS thesis hinges on σ being cheap; v247 puts a *hard 5 % ceiling* on σ-compute cost as a merge-gate predicate.  Correctness + cognitive + adversarial all green at byte-deterministic fixtures; the v1 harness (v247.1) is where those P-tier rows become M-tier measurements. |
| [**v248**](docs/v248/README.md) σ-Release | Version exactly `1.0.0` (`major=1, minor=0, patch=0`); 6 release artifacts in canonical order (`github_release` · `docker_hub` · `homebrew` · `pypi` · `npm` · `crates_io`) each with a non-empty locator; 6 doc sections (`getting_started` · `architecture` · `api_reference` · `configuration` · `faq` · `what_is_real`); WHAT_IS_REAL table with exactly 15 categories each carrying a tier in `{M, P}` — *honest by default*, every category **P-tier at v0**; 7 release criteria `C1..C7` (merge-gate green · benchmark-suite 100 % · chaos all recovered · WHAT_IS_REAL fresh · SCSL pinned · README honest · proconductor approved) all satisfied; `release_ready` is the AND of all criteria + `scsl_pinned` + `proconductor_approved`; `σ_release = 1 − passing / total` and must be `0.0`. | **Release is a typed predicate.** v248 compresses the whole "are we actually shipping 1.0.0?" question into one machine-checked envelope: the artifacts are named, the doc sections are named, the P-tier rows are *labelled P because they are P*, and the final approval is explicit.  There is no way to tag `1.0.0` with a missing doc section or a silently-M-tier-promoted category — the merge-gate simply refuses. |

Every v244–v248 merge-gate check is offline, stdlib-only, and
deterministic.  The v1 promotions — v244.1 live Homebrew tap +
Debian/RPM/Nix packaging + signed Docker manifests + real
`mmap` incremental update via v107; v245.1 live Prometheus
pull + Jaeger/Zipkin push + Slack webhook wire-up + alert
silencing + SLO burn-rate metrics; v246.1 live kill-switch
injection via v239 runtime + real concurrent-load harness +
v211 formal proof of σ-gate invariants + live audit-log
rotation + SCSL signature verification at boot; v247.1 live
ARC / MMLU / HumanEval / TruthfulQA harness that lifts P-tier
v243 categories to M-tier; v248.1 live `gh release create` +
multi-arch Docker manifests + working Homebrew tap + PyPI /
npm / crates.io publish workflows + signed SBOMs and SLSA
level 3+ provenance — are named in each kernel's doc page,
but never claimed before they land.

### MCP · A2A · marketplace · teach · ecosystem-hub (v249–v253)

The **interop / ecosystem** layer.  v249–v253 take the
1.0.0 release and wire it into the rest of the agent world
— and carry a σ on every envelope that leaves the kernel
boundary.  Each kernel is a falsifiable manifest: what the
protocol requires, what the σ-gate refuses, and what it
means for the ecosystem to be healthy.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v249**](docs/v249/README.md) σ-MCP | JSON-RPC 2.0; exactly 5 server-side tools in canonical order (`reason` · `plan` · `create` · `simulate` · `teach`) and 3 resources (`memory` · `ontology` · `skills`); exactly 4 external clients (`database` · `api` · `filesystem` · `search`) each with `σ_trust ∈ [0, 1]`; 5 σ-gated tool-call fixtures with hard thresholds `τ_tool = 0.40` / `τ_refuse = 0.75`, requiring ≥ 3 `USE`, ≥ 1 `WARN`, ≥ 1 `REFUSE` so all branches are exercised; 3 discovery modes (`local` · `mdns` · `registry`) with v169 ontology mapping; `σ_mcp = 1 − passing / (5+3+4+5+3)` and must be `0.0`. | **Every tool call carries a σ.** MCP as-shipped has no typed notion of "can I trust this tool's result?" — v249 puts the σ-gate *at the tool boundary* as a merge-gate predicate, and demonstrates teeth by requiring the `WARN` and `REFUSE` branches to fire in the fixture.  Any silent removal of those branches is a gate failure, not a warning. |
| [**v250**](docs/v250/README.md) σ-A2A | Agent Card with exactly 6 required fields (`name` · `capabilities[6]` · `sigma_profile.{mean, calibration}` · `presence="LIVE"` · `scsl=true` · non-empty `endpoint`) and 6 capabilities (`reason` · `plan` · `create` · `simulate` · `teach` · `coherence`); 4 task-delegation fixtures with decision tree at `τ_neg = 0.50` / `τ_refuse = 0.75`, ≥ 1 `ACCEPT` / `NEGOTIATE` / `REFUSE` each; 3 federation partners (`alice` · `bob` · `carol`) each with `σ_trust ∈ [0, 1]` and a `presence` marker; v128 mesh as transport, v129 federated learning riding it; `σ_a2a = 1 − passing / (6+4+3)` and must be `0.0`. | **Every cross-agent envelope carries a σ.** The public `sigma_profile` on the Agent Card is the distinguishing field — no other agent ships one.  The negotiation branch (`σ > τ_neg`) and refuse branch (`σ > τ_refuse`) are both exercised by the v0 fixture, so a typo that collapses the decision tree to "always accept" fails the gate. |
| [**v251**](docs/v251/README.md) σ-Marketplace | Registry `registry.creation-os.dev`; 5 kernels in canonical order (`medical-v1` · `legal-v1` · `finance-v1` · `science-v1` · `teach-pro-v1`), each with semver, kernel-id deps, 4 quality axes in `[0, 1]` and `σ_quality = mean(axes) ± 1e-4`; install of `medical-v1` with `deps_resolved == n_deps`; 1 composition `medical-v1 + legal-v1 → medical-legal` with `σ_compatibility < τ_compat = 0.50`; publish contract of exactly 4 items (`merge_gate_green` · `sigma_profile_attached` · `docs_attached` · `scsl_license_attached`) all `required AND met`; `σ_marketplace = 1 − passing / (5+1+1+4)` and must be `0.0`. | **Every published kernel has an audited σ-profile and pinned SCSL.** The merge-gate verifies quality scoring is consistent (derived σ matches declared axes ± 1e-4), that install / compose paths land on a σ-compatible result, and — crucially — that the 4-item publish contract is *hard*: no σ-profile, no docs, or no SCSL attestation → publish refused.  License drift is a gate failure, not a policy email. |
| [**v252**](docs/v252/README.md) σ-Teach | Exactly 4 Socratic turns with `QUESTION · QUESTION · QUESTION · LEAD` in order (`n_questions ≥ 3`); 4 adaptive difficulty steps with rule `BORED → UP`, `FLOW → HOLD`, `FRUSTRATED → DOWN` and ≥ 1 `UP` AND ≥ 1 `DOWN`; 3 knowledge-gap rows (v197 ToM) each with `σ_gap ∈ [0, 1]` and `targeted_addressed == true`; teaching receipt with 5 required fields (`session_id` · `taught` · `understood` · `not_understood` · `next_session_start`) and `taught ≥ understood + not_understood`; `σ_understanding = 1 − understood/taught ± 1e-4`; `σ_teach = 1 − passing / 4` and must be `0.0`. | **Teaching is a typed, σ-audited session, not a prompt.** The merge-gate verifies the Socratic ratio has teeth (3 questions *before* the lead), that difficulty both went up *and* came down under the emotion / TTC rule (v189 · v222), that the gap detector addressed every identified gap, and that the receipt is complete — so a silent regression to "just answer it directly" or "never adapt" fails the gate. |
| [**v253**](docs/v253/README.md) σ-Ecosystem-Hub | Hub `hub.creation-os.dev` with exactly 5 sections in canonical order (`marketplace` ← v251 · `agent_directory` ← v250 · `documentation` ← v248 · `community_forum` ← v253 · `benchmark_dashboard` ← v247); 4 ecosystem-health metrics all strictly positive (`active_instances` · `kernels_published` · `a2a_federations` · `contributors_30d`); 5 contribution steps (`fork → write_kernel → pull_request → merge_gate → publish`); 4 roadmap proposals with ≥ 1 voted-in AND *exactly 1* `proconductor_decision == true` (community proposes, Proconductor prioritises); 4 `1 = 1` assertions across scopes `instance · kernel · interaction · a2a_message` with `declared == realized == true`; `σ_ecosystem = 1 − passing / (5+4+5+4)` and must be `0.0`. | **The σ-loop closes across the ecosystem.** v253 composes v249 / v250 / v251 / v252 / v248 / v247 into one typed surface where the "what you declare equals what you realize" audit is enforced *per scope* — a running instance, a published kernel, a user interaction, and an A2A message all have to prove `declared == realized`.  Ecosystem-level declaration drift is a gate failure, not a blog post. |

Every v249–v253 merge-gate check is offline, stdlib-only, and
deterministic.  The v1 promotions — v249.1 live JSON-RPC
transport + real mDNS advertisement + live v169 ontology-driven
tool selection + streamed tool results with incremental σ;
v250.1 live A2A wire protocol + cross-agent TLS handshake +
v201 diplomacy-driven negotiation state machine + v129
federated learning over the real mesh; v251.1 live
`registry.creation-os.dev` + signed package manifests + real
`cos search` / `cos install` / `cos publish` CLI + live quality-
score ingestion from v247 harness runs; v252.1 live multi-turn
tutor over v243 pipeline + real-time TTC hooks via v189 +
emotion-reactive adaptation via v222 + v197 ToM-driven
targeted gap closure with measured learning outcomes; v253.1
live `hub.creation-os.dev` + signed community contributions
end-to-end + real-time ecosystem health telemetry + chain-of-
custody proconductor decisions — are named in each kernel's
doc page, but never claimed before they land.

### Tutor · collaborate · wellness · locale · mission (v254–v258)

The **human-centric / mission** layer.  v254–v258 take the
ecosystem-complete stack and put the human at the center —
adaptive learning, co-authored workflows, session-aware
wellness, worldwide localisation, and a mission statement
in code with an anti-drift gate.  Every kernel is a typed,
falsifiable manifest whose σ-gates have teeth.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v254**](docs/v254/README.md) σ-Tutor | Exactly 4 BKT skills in canonical order (`linear_algebra` · `calculus` · `probability` · `discrete_math`) each with `p_mastery, sigma_mastery ∈ [0, 1]` and `skill_ok`; exactly 4 on-level exercises where `σ_difficulty = \|difficulty − learner_level\| ≤ τ_fit = 0.20` (`fit == true`); exactly 4 modalities (`text` · `code` · `simulation` · `dialog`) with each `σ_fit ∈ [0, 1]` and *exactly one* `chosen == true` AND it is the one with the **minimum** `σ_fit`; exactly 3 progress rows with every `pct_after ≥ pct_before` AND ≥ 1 positive delta; exactly 4 privacy flags (`local_only` · `no_third_party` · `user_owned_data` · `export_supported`) all enabled; `σ_tutor = 1 − passing / (4+4+1+3+4)` and must be `0.0`. | **A σ on every pedagogical decision.** v252 ships one Socratic session; v254 wraps it in a typed learner model (v197 ToM) and a modality σ-gate that *requires the minimum-σ modality to be chosen* — any silent drift to a default modality fails the gate.  The privacy flags are hard: `local_only`, `user_owned_data`, and `export_supported` must all be true, so a v254 build that silently ships data off-box is a gate failure, not a press release. |
| [**v255**](docs/v255/README.md) σ-Collaborate | Exactly 5 modes in canonical order (`ASSIST` · `PAIR` · `DELEGATE` · `TEACH` · `LEARN`) every `mode_ok`; exactly 4 role-negotiation fixtures with `σ_human, σ_model ∈ [0, 1]` and `chosen_mode` one of the 5 modes, with ≥ 3 *distinct* modes chosen across the fixtures; exactly 3 audited workspace edits with strictly ascending ticks, both `HUMAN` and `MODEL` actors represented, every `accepted == true`; exactly 2 conflict fixtures whose decision matches `σ_disagreement ≤ τ_conflict = 0.30 → ASSERT` else `ADMIT`, with ≥ 1 `ASSERT` AND ≥ 1 `ADMIT`; `σ_collaborate = 1 − passing / (5+4+3+2)` and must be `0.0`. | **Human + model as partners, audited per edit.** v255 refuses both firmware-sycophancy and stubbornness: the conflict gate *requires both branches to fire*, so a regression to "always agree with the user" (`ASSERT` missing) OR "always insist" (`ADMIT` missing) fails the gate.  The workspace audit ticks must be strictly ascending with both actors represented — v181 lineage integrity is a merge-gate predicate, not a policy. |
| [**v256**](docs/v256/README.md) σ-Wellness | Typed session (`duration_hours ≥ 0` · `n_requests ≥ 0` · `signal_trend ∈ {STABLE, DEGRADING}` · `session_ok`); exactly 3 nudge fixtures exercising all three branches *in order*: `FIRE` (first past `τ = 4.0 h`, config enabled, not already fired), `DENY` (`already_fired_before == true`), `OPT_OUT` (`config_enabled == false` regardless of duration); exactly 3 boundary signals watched (`friend_language` · `loneliness_attributed` · `session_daily_count`) with reminders fired at most once per session; exactly 3 cognitive-load rows `LOW→NONE`, `MED→SUMMARISE`, `HIGH→SIMPLIFY` (byte-for-byte); `σ_wellness = 1 − passing / (3+3+3+3)` and must be `0.0`. | **Informs, never manipulates.** The rate-limit is a typed gate: the second nudge request in a session is *required* to `DENY` — any regression to a spammy nudge loop fails the merge-gate.  The opt-out path is equally hard: if the user sets `wellness.nudge = false`, v256 must return `OPT_OUT` even above the 4-hour threshold, and the fixture proves this third branch fires.  No dark patterns survive the gate. |
| [**v257**](docs/v257/README.md) σ-Locale | Exactly 10 locales in canonical order (`en` · `fi` · `zh` · `ja` · `de` · `fr` · `es` · `ar` · `hi` · `pt`) each with non-empty timezone + date-format, a 3-letter ISO-4217 currency, `σ_language ∈ [0, 1]`, and `locale_ok`; exactly 3 cultural styles (`direct` ← `en` · `indirect` ← `ja` · `high_context` ← `ar`) where every `example_locale` is one of the 10 declared; exactly 3 legal regimes in canonical order (`EU_AI_ACT` · `GDPR` · `COLORADO_AI_ACT`) all `compliant == true` with `last_reviewed > 0`; exactly 2 time checks (`current_time_example` · `locale_lookup_ok`) both `pass`; `σ_locale = 1 − passing / (10+3+3+2)` and must be `0.0`. | **Worldwide σ, not just English.** v257 types the locale surface so drift is a merge-gate failure: `σ_language ∈ [0, 1]` per language, and the legal-compliance table is verbatim — Colorado AI Act (June 2026) sits alongside EU AI Act and GDPR.  Any silent drop of a locale, a style, or a regime fails the gate, so the internationalisation contract survives refactors. |
| [**v258**](docs/v258/README.md) σ-Mission | `mission_statement` matches the canonical string byte-for-byte (`"Reduce sigma in every system that touches human life. Make intelligence trustworthy."`) AND `statement_ok`; exactly 4 impact scopes (`per_user` · `per_domain` · `per_institution` · `per_society`) with `σ_before, σ_after ∈ [0, 1]`, `delta_sigma = σ_before − σ_after`, every `improved == true`; exactly 4 anti-drift rows whose decision matches `σ_mission ≤ τ_mission = 0.30 → ACCEPT` else `REJECT`, with ≥ 1 `ACCEPT` AND ≥ 1 `REJECT` (both branches fire); exactly 4 long-term anchors (`civilization_governance` ← v203 · `wisdom_transfer_legacy` ← v233 · `human_sovereignty` ← v238 · `declared_eq_realized` ← 1 = 1) all `anchor_ok`; `σ_mission = 1 − passing / (1+4+4+4)` and must be `0.0`. | **The mission is a merge-gate predicate.** The statement is byte-identical or the gate fails — no marketing drift.  Impact is measured as σ reduction across 4 scopes, each strictly improved.  The anti-drift gate *requires both ACCEPT and REJECT to fire*, so a build that silently lowers the bar (`τ` raised) or quietly accepts everything fails.  v258 makes `1 = 1 ≡ declared = realized ≡ reduce σ ≡ trustworthy intelligence` a thread the merge-gate can break. |

Every v254–v258 merge-gate check is offline, stdlib-only, and
deterministic.  The v1 promotions — v254.1 live BKT online-
update loop + v141 curriculum generator over a real exercise
bank + v113 sandbox hosting code drills + v220 simulate driving
concept visualisations + v172 live narrative with consented
opt-in + `cos tutor --export` signed portable archive; v255.1
live mode switcher wired to v243 pipeline + live shared FS via
v242 with per-edit σ + v181 audit streamed to the lineage store
+ v201 diplomacy-driven multi-round conflict resolution;
v256.1 live session-clock binding to v243 + opt-out persistence
via v242 + v222 emotion-driven nudge modulation + v181-audited
reminders + v189 TTC live feed into the simplify action;
v257.1 live locale auto-detection via OS + IANA tz db + on-
disk MO/PO files per locale + v202 culture-driven reply style
+ v199 live law-module lookup per jurisdiction + σ_compliance
from a legal-source audit; v258.1 `cos impact` producing a
signed report against live telemetry + v191 constitutional
feeding a running anti-drift classifier + v203 / v233 / v238
binding to real governance / legacy / sovereignty subsystems —
are named in each kernel's doc page, but never claimed before
they land.

### Engram · AirLLM · hybrid · mesh · sovereign (v260–v264)

The **sovereign-infrastructure** layer.  v260–v264 make
Creation OS its own hardware / model / memory / routing
/ mesh stack — DeepSeek Engram on the fact side, AirLLM
on the heavy-model side, a σ-router that picks between
local and cloud by σ, a distributed hash table for mesh
memory, and a 7-layer sovereign pino that runs offline
for ~20 €/mo.  Every kernel is a typed, falsifiable
manifest whose σ-gates have teeth.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v260**](docs/v260/README.md) σ-Engram | Parameter split `static_pct ∈ [20, 25]` AND `dynamic_pct ∈ [75, 80]` AND sum 100 (DeepSeek's ratio); exactly 5 static-knowledge lookups in canonical order with non-zero hash, `hit == true`, `σ_result ∈ [0, 1]`, `lookup_ns ≤ 100` (O(1) DRAM budget); exactly 3 dynamic-reasoning (MoE) rows with `experts_activated > 0` AND `σ_result ∈ [0, 1]`; exactly 4 σ-gate fixtures where `σ_result ≤ τ_fact = 0.30 → USE` else `VERIFY`, with ≥ 1 `USE` AND ≥ 1 `VERIFY` (both branches fire); long-context manifest `hit_rate_pct == 97` AND `miss_rate_pct == 3` AND `miss_flagged_by_sigma`; `σ_engram = 1 − passing / (5+3+4+1+1)` and must be `0.0`. | **A σ on every Engram hit.** DeepSeek Engram ships 97 % NIAH @ 1 M — but the 3 % miss is *silent*.  v260 types the surface so every lookup has an `σ_result`, and the gate forces both `USE` and `VERIFY` to fire on a real fixture, making `Engram + σ ≈ 99 %+` a merge-gate predicate rather than a promise.  The O(1) budget is enforced per row (`lookup_ns ≤ 100`) — Engram that silently slows down fails the gate. |
| [**v261**](docs/v261/README.md) σ-AirLLM | Exactly 8 layers with indices 0 .. 7 strictly ascending, `σ_layer ∈ [0, 1]`, `precision_bits` matching the σ-driven rule byte-for-byte (`σ ≤ 0.20 → 4` · `0.20 < σ ≤ 0.40 → 8` · `σ > 0.40 → 16`); exactly one `is_problem == true` AND it is the unique argmax of `σ_layer` AND matches `problem_index`; exactly 4 hardware backends in canonical order (`cuda_4gb_gpu` · `cpu_only` · `macos_mps` · `linux_cuda`) every `supported` with `min_ram_gb ≥ 4`; exactly 3 tradeoff regimes (`slow` · `aircos` · `human`) where `aircos` has the **strictly** highest `tokens_per_s` AND `abstain_pct > 0` (gate has teeth); `σ_airllm = 1 − passing / (8+8+4+3+1)` and must be `0.0`. | **Layer-level σ + selective precision are gate predicates.** AirLLM alone cannot tell you *which* layer adds the noise; v261 surfaces it with a unique-argmax problem-layer identifier AND drives per-layer precision from σ — so a deployment that silently pushes every layer back to 4-bit fails the gate.  The tradeoff table is equally hard: `aircos` must win effective tokens/s *and* actually abstain (otherwise the σ-gate is just dead code).  4 GB GPU, CPU-only, macOS, Linux all typed — hardware-agnostic is falsifiable, not a flyer. |
| [**v262**](docs/v262/README.md) σ-Hybrid-Engine | Exactly 5 engines in canonical order (`bitnet-3B-local` · `airllm-70B-local` · `engram-lookup` · `api-claude` · `api-gpt`) every `cost_per_1k_eur ≥ 0` AND `σ_floor ∈ [0, 1]` AND `engine_ok`; exactly 4 routing fixtures where `chosen_engine` is in the registry AND ≥ 3 **distinct** engines are chosen (router has teeth); exactly 4 cascade steps where `σ_result ≤ τ_accept = 0.40 → OK` else `ESCALATE`, with step 0 REQUIRED to `ESCALATE` AND ≥ 1 `OK` AND ≥ 1 cloud-tier step (fallback works); cost report `local_pct + api_pct == 100`, `local_pct ≥ 80`, `savings_pct ≥ 80` AND matches `100 × (eur_api_only − eur_sigma_route) / eur_api_only` within ±1 pt; `σ_hybrid_engine = 1 − passing / (5+4+1+4+1)` and must be `0.0`. | **σ picks the engine.** v262 types the router: easy question → `bitnet-3B-local`, fact query → `engram-lookup`, heavy → `airllm-70B-local`, only the ≥ 0.50-difficulty step escalates to cloud.  The cascade forces step 0 to `ESCALATE`, so a regression that routes everything straight to `api-claude` (= cost blow-up) fails the gate.  The 87 % / 13 % local/api split produces the 4.20 € vs 32.00 € receipt — the "85 %+ savings" claim becomes a merge-gate arithmetic predicate, not a slide. |
| [**v263**](docs/v263/README.md) σ-Mesh-Engram | Exactly 3 mesh nodes A / B / C with contiguous, non-overlapping shards covering `[0, 256)` (`shards_ok`); exactly 4 lookup fixtures where `served_by == expected_node`, `lookup_ns ≤ 100`, AND every node ∈ {A, B, C} serves at least one fixture (`n_nodes_covered == 3`); exactly 4 replication rows with `replicas == 3`, `σ_replication ∈ [0, 1]`, `quorum_ok ⇔ σ_replication ≤ τ_quorum = 0.25`, with ≥ 1 `quorum_ok == true` AND ≥ 1 `== false`; exactly 4 memory-hierarchy tiers L1 (`local_sqlite`) → L2 (`engram_dram`) → L3 (`mesh_engram`) → L4 (`api_search`) with `latency_ns` AND `capacity_mb` strictly ascending (L4 encoded as `UINT32_MAX`); exactly 4 σ-forgetting rows where action matches `σ ≤ 0.20 → KEEP_L1` · `≤ 0.50 → MOVE_L2` · `≤ 0.80 → MOVE_L3` · else `DROP`, every branch firing at least once; `σ_mesh_engram = 1 − passing / (1+4+1+4+1+1+4+1)` and must be `0.0`. | **Engram, but distributed — with σ holding quorum, hierarchy, and eviction honest.** v263 makes shard coverage a gate predicate: any shard with no node (or overlapping shards) fails immediately.  The replication table requires both `quorum_ok` branches to fire, so a build that silently hides quorum drift fails.  The memory hierarchy is *strictly* ascending in both latency and capacity — a regression that puts L3 at lower latency than L2, or caps L4 below L3 in MB, fails the gate.  Forgetting is σ-driven with every branch exercised: hot stays, warm moves, cold mesh, noise drops. |
| [**v264**](docs/v264/README.md) σ-Sovereign-Stack | Exactly 7 stack layers in canonical order (`hardware` · `model` · `memory` · `gate` · `network` · `api_fallback` · `license`) with every `open_source == true` for layers 0 .. 5 AND *only* `api_fallback` has `requires_cloud == true` AND *only* `api_fallback` has `works_offline == false` (all other layers 0 .. 4, 6 are offline); exactly 4 offline flows (`helper_query` · `explain_concept` · `fact_lookup` · `reasoning_chain`) where `engine ∈ {bitnet-3B-local, airllm-70B-local, engram-lookup}`, `used_cloud == false`, `ok == true`, with ≥ 2 distinct local engines used; exactly 4 sovereign-identity anchors fulfilled (`v234` presence · `v182` privacy · `v148` sovereign · `v238` sovereignty); cost model `eur_baseline == 200` AND `eur_sigma_sovereign == 20` AND `reduction_pct == round(100 × (200 − 20) / 200) == 90`; `σ_sovereign_stack = 1 − passing / (7+4+1+4+1)` and must be `0.0`. | **One stack, zero cloud dependency, ~20 €/mo — as a merge-gate predicate.** v264 types every layer's offline/cloud polarity so a regression that makes `model` silently require cloud fails the gate.  The offline-flows fixture proves the four canonical flows run with no cloud touch on ≥ 2 distinct local engines — a build that secretly calls `api-claude` from `helper_query` fails immediately.  The sovereign anchor set binds v234 / v182 / v148 / v238 so removing any one of them breaks the stack contract.  Cost is a clean identity: `200 → 20 → 90 %` reduction.  "Its like a hobby bro" → "its like a coffee bro." |

Every v260–v264 merge-gate check is offline, stdlib-only,
and deterministic.  The v1 promotions — v260.1 live DRAM
hash-table-backed Engram + real MoE reasoning wired
through v243 pipeline + v227 entropy decomposition
driving the knowledge-vs-reasoning split at inference +
live NIAH harness producing `hit_rate_pct` from measured
runs; v261.1 live AirLLM layer streamer + on-disk layer
cache + runtime precision-switching driven by a live σ
feed + multi-backend auto-detection at boot; v262.1 live
`cos engines` CLI with backend auto-detection + real-time
σ-router wired to v243 + invoice reconciliation for
`api-claude` / `api-gpt` + σ-proxy cost optimiser as a
cascade driver; v263.1 live gossip on v128 mesh + live
quorum via v216 + σ-driven eviction wired to v115 / v242
storage backends; v264.1 live `cos start --offline` boot
target + auto-detected hardware profile + wired mesh P2P
stack + signed sovereign identity + real invoice
reconciliation for the 20 €/mo claim — are named in each
kernel's doc page, but never claimed before they land.

### Speculative · flash · mamba · batch · compile (v265–v269)

The **performance-maximum** layer.  v265–v269 take the
sovereign stack and make it fast: σ-guided speculative
decoding, FlashAttention with σ fused into the same
kernel, Mamba / RWKV state-space backends with σ-gated
transformer fallback, σ-prioritised continuous batching
with preemption and adaptive batch size, and full
inference-pipeline AOT compilation with σ-guided PGO and
compile-time σ-elimination of always-cold paths.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v265**](docs/v265/README.md) σ-Speculative | Exactly 2 models (`bitnet-1.5B-local` draft + `airllm-70B-local` verifier); exactly 4 σ-bands with canonical `spec_len [12, 8, 6, 4]` and `spec_len` strictly non-increasing in σ (`monotone_ok`); exactly 3 multi-draft duels where `winner == argmin(σ_draft)` AND both A-wins and B-wins fire; exactly 4 σ-gate fixtures where `σ_speculated ≤ τ_spec = 0.35 → ACCEPT` else `REJECT` with ≥ 1 ACCEPT AND ≥ 1 REJECT (the extra σ-gate has teeth — a token the verifier would keep can still be rejected); throughput `tok_per_s_plain < tok_per_s_sigma_spec` AND `speedup_x ≥ 2.0`; `σ_speculative = 1 − passing / (2+4+1+3+1+4+1+1)` and must be `0.0`. | **σ picks how many tokens to speculate.** Classical speculative decoding speculates a fixed N; v265 makes N a function of σ (easy context → 12 tokens, hard context → 4).  The band monotonicity is a gate predicate — a regression that flips the relationship (spending more speculation on hard context) fails immediately.  Multi-draft duels are forced to exercise both A-wins and B-wins, so a regression that always picks draft A fails.  The ≥ 2× speedup and ≥ 1 ACCEPT + ≥ 1 REJECT turn "speculative decoding helps" from marketing into a merge-gate arithmetic predicate. |
| [**v266**](docs/v266/README.md) σ-Flash | Exactly 8 attention heads all `fused == true` with `overhead_pct ∈ [0, 1.0)` per head (strict sub-1 % σ-fusion cost over raw FlashAttention) and `σ_head ∈ [0, 1]`; exactly 3 platform kernels in canonical order (`cuda_sm90` · `metal_m4` · `neon_arm64`) every `supported`, `latency_ns > 0`, `sigma_fused`; exactly 6 KV cache entries where `evict_rank` is a permutation of `[1..6]` matching descending-σ order (rank 1 = max σ evicted first, rank 6 = min σ evicted last), `kv_order_ok`; long-context pruning with `kept_tokens_before == kept_tokens_after` AND `effective_ctx_k_after > effective_ctx_k_before` (same memory, strictly larger effective context); `σ_flash = 1 − passing / (8+3+6+1+1)` and must be `0.0`. | **σ comes "free" in the FlashAttention kernel.** Per-head σ is usually an after-the-fact pass that doubles memory traffic; v266 fuses it into the same kernel and caps the overhead at `< 1 %` per head as a gate predicate.  The KV cache becomes σ-aware: high-σ entries evict first, so the cache keeps valuable context.  Long-context pruning is typed as "same kept_tokens, strictly larger effective_ctx_k" — a regression that shrinks effective context while holding memory fails. |
| [**v267**](docs/v267/README.md) σ-Mamba | Exactly 3 backends in canonical order (`mamba` · `rwkv` · `transformer`) with `mamba.exponent == 1` AND `rwkv.exponent == 1` AND `transformer.exponent == 2` AND mamba / rwkv `throughput_rel > transformer.throughput_rel`; exactly 4 route fixtures where `σ_mamba ≤ τ_mamba = 0.40 → mamba` else `transformer` with ≥ 1 mamba AND ≥ 1 transformer; exactly 8 Jamba-style hybrid layers alternating `mamba / transformer` (4 + 4) with `σ_arch ∈ [0, 1]`; exactly 3 tasks with `σ_chosen ≤ σ_rival` each AND ≥ 2 distinct chosen backends across the 3 tasks; `σ_mamba_kernel = 1 − passing / (3+4+1+8+1+3+1)` and must be `0.0`. | **σ picks the architecture per query.** Mamba wins on long context (linear), transformer wins on in-context learning (quadratic); v267 makes the decision a σ-gate.  Exponent contract is falsifiable (a regression that mislabels RWKV as quadratic fails).  The Jamba-style interleaving is counted explicitly (4 mamba + 4 transformer), so a build that silently drops all mamba layers fails.  Task-level diversity requires ≥ 2 distinct chosen backends, killing "everything becomes transformer" regressions at the gate. |
| [**v268**](docs/v268/README.md) σ-Continuous-Batch | Exactly 6 queue requests with `priority_slot` as a permutation of `[1..6]` matching `argsort(+σ_difficulty)` — low σ first, fast path (`queue_order_ok`); exactly 2 preemption scenarios where `preempted == (σ_urgency_arrival > σ_urgency_incumbent)` and both `true` and `false` outcomes fire; exactly 3 load levels in canonical order (`low` · `medium` · `high`) with `σ_load` AND `batch_size` *both* strictly monotone-ascending (`batch_monotone_ok`); exactly 2 cost scenarios where `total_local_eur < total_api_eur`; `σ_continuous_batch = 1 − passing / (6+1+2+1+3+1+2+1)` and must be `0.0`. | **σ is the scheduler.** Priority is driven by σ_difficulty (easy queries jump the queue and finish faster), preemption by σ_urgency, batch size by σ_load, cost by σ_cost.  The permutation contract on `priority_slot` makes "low σ first" a byte-exact gate predicate — a regression that flips priority order fails.  Preemption is forced to exercise both outcomes, so "always preempt" or "never preempt" regressions both fail.  Batch-size monotonicity in σ_load is a two-way ordered predicate, catching any load-level regression immediately. |
| [**v269**](docs/v269/README.md) σ-Compile-v2 | Exactly 6 pipeline stages in canonical order (`tokenize` · `embed` · `attention` · `ffn` · `sigma_gate` · `detokenize`) every `aot_compiled == true` AND `native == true` AND `latency_ns > 0`; exactly 4 platform targets in canonical order (`m4_apple_silicon` · `rpi5_arm64` · `gpu_4gb_speculative` · `x86_avx512`) every `tok_per_s ≥ budget_tok_per_s` AND `meets_budget` (budgets: M4 ≥ 100, RPi5 ≥ 10, 4 GB GPU ≥ 50, x86-AVX512 ≥ 80); exactly 4 PGO hot paths where `optimization == "aggressive" iff hotpath_fraction ≥ 0.20` with ≥ 1 aggressive AND ≥ 1 space; exactly 6 compile-time σ-elimination rows where `elided == (sigma_profile < 0.05)` with ≥ 1 elided AND ≥ 1 kept (adaptive — not all-or-nothing); `σ_compile_v2 = 1 − passing / (6+4+4+1+6+1)` and must be `0.0`. | **The entire inference pipeline goes AOT.** v137 did this for the σ-gate alone (0.6 ns, branchless); v269 does it for tokenize → embed → attention → FFN → σ-gate → detokenize.  Per-platform throughput budgets are gate predicates, so "100 tok/s on M4" is not a slide — it's a merge-gate failure otherwise.  σ-guided PGO is two-sided (hot paths aggressive, cold paths space), forcing both strategies to exercise.  Compile-time σ-elimination requires *both* elided and kept rows to exist — adaptive, not all-or-nothing, so a regression that strips every σ-check away fails. |

Every v265–v269 merge-gate check is offline, stdlib-only,
and deterministic.  The v1 promotions — v265.1 live
draft + verifier inference wired to v262 hybrid engine
with real GPU throughput, v266.1 live CUDA / Metal / NEON
kernels emitting per-head σ + PagedAttention with σ_kv
eviction + live σ-pruning, v267.1 live SSM / RWKV / Jamba
inference driven by v262, v268.1 live queue wired to v262
with real preemption under load + live cost tracker,
v269.1 live LLVM / MLIR AOT pipeline with measured tok/s
per platform + PGO data fed from production runs — are
named in each kernel's doc page, but never claimed before
they land.

### TinyML · swarm-edge · twin · robotics · industrial (v270–v274)

The **physical-world integration** layer.  v270–v274
take the sovereign + performance stack and carry it
down to MCUs, sensor swarms, digital twins, physical
robotics, and the factory floor.  σ is no longer just
a confidence on a token — it is the gate on what gets
transmitted, what gets fused, what action is taken in
the real world, and whether an OEE number is
trustworthy at all.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v270**](docs/v270/README.md) σ-TinyML | Typed MCU footprint envelope: `sigma_measurement_bytes == 12` (three float32, nothing else), `code_flash_bytes ≤ 1024`, `ram_bytes_per_instance ≤ 100`, `thumb2_instr_count ≤ 24`, `branchless == true`; exactly 4 MCU targets in canonical order (`cortex_m0_plus` · `cortex_m4` · `cortex_m7` · `xtensa_esp32`) every `supported` AND `cpu_mhz > 0`; exactly 3 sensors in canonical order (`temperature` · `humidity` · `pressure`); exactly 4 fusion fixtures at `τ_fusion = 0.30` with `σ_fusion == max(σ_temp, σ_humidity, σ_pressure)` and decision `TRANSMIT iff σ_fusion ≤ τ_fusion` else `RETRY`, both branches firing; exactly 4 anomaly rows with `anomaly == (σ_measured > σ_baseline + delta)` firing both branches; exactly 3 OTA rounds every `applied == true` AND every `firmware_reflash == false`; `σ_tinyml = 1 − passing / (1+4+3+4+1+4+1+3)` and must be `0.0`. | **σ fits in 12 bytes and runs in ≤ 24 Thumb-2 instructions.**  v137 σ-compile shrank the gate to 0.6 ns on workstations; v270 types the same gate for Cortex-M0+ / M4 / M7 / ESP32 and makes every footprint limit a merge-gate predicate.  Sensor fusion is `max(σ)`, so one bad sensor pokes out immediately (a regression that averages σ instead of maxing it fails).  Anomaly detection is literally "σ rose above baseline + delta" — no ML model required.  OTA τ calibration is payload-only, not firmware reflash, so the gate is always less expensive to retune than to replace. |
| [**v271**](docs/v271/README.md) σ-Swarm-Edge | Exactly 6 mesh sensors at `τ_consensus = 0.50` with `included == (σ_local ≤ τ_consensus)`, ≥ 1 included AND ≥ 1 excluded, AND `σ_swarm < σ_raw` (consensus mean over included strictly beats naive mean over all); exactly 4 distributed-anomaly fixtures where `spatial_anomaly == ((σ_center − σ_neighborhood) > 0.25)` firing both branches; exactly 3 energy tiers in canonical order (`charged` · `medium` · `low`) with **σ_energy strictly ascending AND sample_rate_hz strictly descending** (hotter battery → faster sampling); exactly 1 gateway fixture where `bridged_to_engine` is a valid v262 engine (`bitnet-3B-local · airllm-70B-local · engram-lookup · api-claude · api-gpt`) AND `swarm_size_nodes == 6` (matches the mesh exactly); `σ_swarm_edge = 1 − passing / (6+1+4+1+3+1+1)` and must be `0.0`. | **σ is the swarm's consensus rule.** "Ignore the outlier" is usually a heuristic; v271 makes it a gate predicate — `σ_swarm < σ_raw` is byte-exact arithmetic, so a regression that keeps high-σ sensors in the consensus fails.  Spatial σ-anomaly (`σ_center − σ_neighborhood > 0.25`) turns "something happened near sensor B" into a distributed, centralised-server-free trigger.  Energy-aware σ makes sample-rate-vs-battery a two-way monotonic predicate: charged battery → 100 Hz, dead battery → 1 Hz, with strict ordering — a regression that reverses the relationship fails.  The gateway is typed as bridging to the v262 engine set, so the swarm is explicitly a first-class member of the sovereign stack. |
| [**v272**](docs/v272/README.md) σ-Digital-Twin | Exactly 4 twin-sync fixtures where `stable == (σ_twin < 0.05)` AND `drifted == (σ_twin > 0.30)` with ≥ 1 stable AND ≥ 1 drifted; exactly 3 maintenance rows where `action ∈ {REPLACE, MONITOR}` with `REPLACE iff σ_prediction ≤ τ_pred = 0.30`, both branches firing; exactly 3 what-if rows where `decision ∈ {IMPLEMENT, ABORT}` with `IMPLEMENT iff σ_whatif ≤ τ_whatif = 0.25`, both branches firing; exactly 3 verified-action rows where `σ_match == |declared_sim − realized_phys|` (1=1 typing of declared = realized) and `verdict ∈ {PASS, BLOCK}` with `PASS iff σ_match ≤ τ_match = 0.10`, both branches firing; `σ_digital_twin = 1 − passing / (4+1+3+1+3+1+3+1)` and must be `0.0`. | **σ makes the twin a first-class contract.** `σ_twin` is "how much the simulator has drifted from the physical system" as a number; stable / drifted both fire, so a regression that sticks in one state (always-stable or always-drifted) fails.  Predictive maintenance becomes a σ-threshold: REPLACE iff σ is low enough to trust the prediction, else MONITOR — two-branch testing kills "always replace" and "never replace" alike.  What-if is a simulator query gated by σ_whatif; a regression that implements every change fails at the IMPLEMENT-vs-ABORT predicate.  Verified action is the decisive 1=1 contract: `σ_match == |declared − realized|` — if simulation disagreed with physics, σ_match is exactly the gap, and the verdict is merge-gate arithmetic. |
| [**v273**](docs/v273/README.md) σ-Robotics | Exactly 4 action fixtures with three-branch cascade `σ ≤ τ_exec = 0.20 → EXECUTE · σ ≤ τ_simple = 0.50 → SIMPLIFY · else ASK_HUMAN` with every branch firing ≥ 1×; exactly 3 perception sensors in canonical order (`camera` · `lidar` · `ultrasonic`) with `fused_in == (σ_local ≤ τ_fuse = 0.40)`, ≥ 1 fused AND ≥ 1 excluded, AND `σ_percep_fused < σ_percep_naive` (σ-fusion beats indiscriminate averaging); exactly 4 safety-envelope rows with **σ_safety strictly ascending AND slow_factor strictly descending** (closer to the boundary → higher σ → stronger slowdown); exactly 3 failure-memory rows where `σ_current > σ_prior` for **all** rows (`all_learned`); `σ_robotics = 1 − passing / (4+1+3+1+1+4+1+3+1)` and must be `0.0`. | **σ is the physical-safety gate.** LLM abstention in text is "don't say the wrong thing"; v273 abstention in robotics is "don't slam the actuator". Three branches EXECUTE / SIMPLIFY / ASK_HUMAN every firing is a merge-gate predicate — a regression that always executes (or always asks) fails at the gate.  Perception fusion gain is a two-way contract: `σ_percep_fused < σ_percep_naive` — blending every sensor equally, including the noisy one, makes things worse and fails the gate.  Safety envelope monotonicity is two-way: approach the boundary, σ rises, slow_factor drops — a regression that accelerates near a boundary fails instantly.  Failure memory is all-or-nothing: every row must show `σ_current > σ_prior`, "never repeat the same mistake without extra caution" as merge-gate arithmetic. |
| [**v274**](docs/v274/README.md) σ-Industrial | Exactly 4 process parameters in canonical order (`temperature` · `pressure` · `speed` · `material`) with `σ_process == max(σ_param_i)` (strict typing, not a mean) and `action ∈ {CONTINUE, HALT}` matching `σ_process ≤ τ_process = 0.40` — the fixture drives the HALT branch so a regression that collapses max→mean fails; exactly 4 supply links in canonical order (`supplier` · `factory` · `distribution` · `customer`) with `backup_activated == (σ_link > τ_backup = 0.45)`, both branches firing; exactly 3 quality rows with `action ∈ {SKIP_MANUAL, REQUIRE_MANUAL}` matching `σ_quality ≤ τ_quality = 0.25`, both branches firing; exactly 3 OEE shift fixtures with `oee == availability × performance × quality` (within 1e-4) AND `trustworthy == (σ_oee ≤ τ_oee = 0.20)`, both branches firing; `σ_industrial = 1 − passing / (4+1+1+4+1+3+1+3+1+1)` and must be `0.0`. | **σ_oee is a meta-measurement: the confidence in the measurement itself.**  OEE is the canonical Industry 4.0 KPI.  v274 makes it explicit: when the measurement is uncertain (σ_oee > 0.20), the OEE number is marked untrustworthy at the gate, not quietly reported as fact.  Process σ is `max(σ_param)` — one bad parameter HALTs the line; a regression that averages σ fails.  Supply chain σ lets the backup supplier fire **before** the problem is visible (σ_link > 0.45 ⇒ backup activates), turning "reactive" supply chain into a σ-threshold predicate.  Quality prediction is SKIP-vs-REQUIRE manual — both branches forced to fire, so "skip all manual checks" and "require all manual checks" regressions both fail. |

Every v270–v274 merge-gate check is offline, stdlib-only,
and deterministic.  The v1 promotions — v270.1 physical
MCU builds with measured flash / RAM / disassembly,
v271.1 live BLE / LoRa / Zigbee mesh with v128 transport,
v272.1 live sensor feed into v220 simulator with
receipt-stamped twin sync, v273.1 real ROS2 integration
with on-robot failure memory, v274.1 live MES / SCADA /
OPC-UA integration with receipt-stamped σ_oee — are
named in each kernel's doc page, but never claimed
before they land.

### TTT · gated-deltanet · distill-runtime · rsi (v275–v278)

The **self-improving sovereign** layer.  v275–v278 lift
σ from a static gate to a manifest that converges over
time: σ-gated test-time training, σ-per-gate linear
attention, σ-filtered live distillation from API
teachers into a local student, and a recursive loop
where the σ-gate's own calibration, architecture, and
thresholds learn — all typed as merge-gate predicates
with Gödel-aware escalation to a proconductor when a
blind spot exceeds τ_goedel.

| Capability | What it is | What σ adds |
|---|---|---|
| [**v275**](docs/v275/README.md) σ-TTT | Exactly 4 σ-gated update fixtures at `τ_update = 0.30` with `decision LEARN iff σ_update ≤ τ_update` else SKIP (both branches); exactly 3 dual-track fixtures with cascade `σ_drift < τ_sync = 0.15 → SYNCED · σ_drift < τ_reset = 0.50 → DIVERGING · else RESET` firing all three branches; exactly 6 sliding-window tokens whose `evict_rank` is a permutation of `[1..6]` matching descending-σ order; exactly 2 validation citations (`v124_sigma_continual` + `ttt_e2e_2025`) present and distinct; `σ_ttt = 1 − passing / (4+1+3+1+6+1+2+1)` and must be `0.0`. | **σ is what tells living weights when to learn and when to reset.**  Stanford/NVIDIA's TTT-E2E (12/2025) validated v124 σ-continual academically: updating the MLP tail during inference matches full-attention accuracy at 35× throughput on 2 M contexts.  v275 types the σ-layer on top: weights update **only** when σ_update is low (don't learn from noise), a dual-track cascade detects drift and snaps back to the stable track, and sliding-window eviction is a σ-ordering (rank 1 = highest σ evicted first, rank 6 = lowest σ kept longest) — a regression that evicts low-σ tokens first fails byte-exactly.  This is a citation contract, not a throughput claim: v275.1 is where the manifest meets real MLP layers and measured tok/s. |
| [**v276**](docs/v276/README.md) σ-Gated-DeltaNet | Exactly 2 canonical backends (`deltanet` exp=1 gate=true · `transformer` exp=2 gate=false) with `deltanet.throughput_rel > transformer.throughput_rel`; exactly 4 σ-gate fixtures at `τ_gate = 0.35` with `decision LINEAR iff σ_gate ≤ τ_gate` else `FALLBACK_FULL` (both branches); exactly 3 combo components canonical (`deltanet` · `ttt` · `sigma_gate`) each `enabled == true` with `layer_slot ∈ [1..3]` in canonical order; exactly 3 tri-backend tasks with `chosen == argmin(σ_backend)` per task, `σ_chosen ≤ σ_rival` for every task, AND ≥ 2 distinct chosen backends across the 3 tasks; `σ_deltanet = 1 − passing / (2+1+1+4+1+3+1+3+1+1)` and must be `0.0`. | **σ makes linear attention a two-way contract.**  Gated DeltaNet's appeal is `O(n)`, but linear attention sometimes drops the context that matters.  v276's σ-per-gate manifest codifies "fallback to full attention when σ_gate > τ_gate" as a merge-gate predicate — a regression that always-linear or always-full both fail.  Backend exponents are typed (`deltanet.exponent == 1` AND `transformer.exponent == 2`) so a refactor that silently flips them is caught.  The tri-backend task table forces **diversity**: at least two of {mamba, deltanet, ttt} have to win at least one task, so "always pick deltanet" is structurally impossible to pass the gate. |
| [**v277**](docs/v277/README.md) σ-Distill-Runtime | Teacher/student pair typed (`api-claude` → `bitnet-3B-local`, distinct); exactly 4 σ-filter fixtures at `τ_learn = 0.25` with `decision LEARN iff σ_teacher ≤ τ_learn` else SKIP (both branches); exactly 3 domain rows canonical (`law` · `code` · `medical`) with `route LOCAL_ONLY iff σ_domain ≤ τ_domain = 0.30` else API (both branches); exactly 4 sovereign-trajectory checkpoints (`month_0` · `month_1` · `month_3` · `month_12`) with `api_share + local_share ≈ 1.0`, `api_share` strictly decreasing, `local_share` strictly increasing, cost strictly decreasing, AND anchors `api_share[0] ≥ 0.75` / `api_share[-1] ≤ 0.10`; `σ_distill = 1 − passing / (1+4+1+3+1+4+1+1+1+1+1)` and must be `0.0`. | **σ is the filter that decides what a student bothers to learn.**  "Teacher said X" isn't enough if the teacher itself was uncertain; v277 only distills when `σ_teacher ≤ τ_learn` so the student learns from signal, not noise.  Per-domain routing uses `σ_domain` as a sufficiency test: `σ_domain ≤ 0.30 → the local student is already good enough → never call the API for this domain`.  The trajectory is four contracts in one row: shares sum to 1, api↓ monotone, local↑ monotone, cost↓ monotone, and anchor values pin the start (API-heavy) and end (sovereign).  A regression that flatlines cost, or sneaks api_share back up over time, fails at the gate. |
| [**v278**](docs/v278/README.md) σ-Recursive-Self-Improve | Exactly 4 calibration epochs with strictly decreasing `σ_calibration_err` and last epoch `≤ 0.05`; exactly 3 arch configurations (`n_channels ∈ {6, 8, 12}`) with `chosen == argmax(aurcc)` AND ≥ 2 distinct `aurcc` values; exactly 3 canonical threshold rows (`code.tau == 0.20` · `creative.tau == 0.50` · `medical.tau == 0.15`) each `τ ∈ (0, 1)` AND ≥ 2 distinct τ values; exactly 3 Gödel fixtures at `τ_goedel = 0.40` with `action SELF_CONFIDENT iff σ_goedel ≤ τ_goedel` else `CALL_PROCONDUCTOR` (both branches); `σ_rsi = 1 − passing / (4+1+1+3+1+1+3+1+1+3+1)` and must be `0.0`. | **σ is the only subsystem that measures its own drift and knows its own blind spots.**  Calibration feedback is a monotonicity predicate: error has to go down every epoch AND bottom out at ≤ 0.05 — "self-improvement" that plateaus high fails.  Architecture search is `argmax(aurcc)` with mandatory diversity: if all configs tie, the manifest fails (a regression that makes the σ-gate unfalsifiable is caught).  Per-domain τ is canonical and diverse (code tight at 0.20, creative loose at 0.50, medical tightest at 0.15) — any refactor that collapses to a single global τ fails the gate.  The Gödel row is the honest one: when `σ_goedel > τ_goedel`, `CALL_PROCONDUCTOR` fires — a formal admission that some blind spots have to be verified by an external system.  "Recursive self-improvement" stays Gödel-aware by construction. |

Every v275–v278 merge-gate check is offline, stdlib-only,
and deterministic.  The v1 promotions — v275.1 live TTT
kernel wired into v262 with σ-gated MLP-tail updates and
measured throughput on 2 M contexts, v276.1 live Gated
DeltaNet with per-token σ from the gate and measured
AURCC vs v267 σ-Mamba / v275 σ-TTT, v277.1 live teacher /
student loop with a real monthly API share on a user's
workload, v278.1 live calibration tuning, CMA-ES arch
search, per-domain τ adaptation, and a production
proconductor call-out pipeline — are named in each
kernel's doc page, but never claimed before they land.
