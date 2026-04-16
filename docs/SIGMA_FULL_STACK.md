# σ Full Stack — from paper to physics (Creation OS v39)

One equation family, many substrates:

\[
K_{\mathrm{eff}} = (1-\sigma_{\mathrm{total}})\cdot K
\]

At the software layers, \(\sigma_{\mathrm{total}}\) is often instantiated as **aleatoric + epistemic** (Dirichlet / evidence-style decomposition). At v39, the lab adds an explicit **hardware** term:

\[
\sigma_{\mathrm{total}} \approx \sigma_{\mathrm{aleatoric}} + \sigma_{\mathrm{epistemic}} + \sigma_{\mathrm{hardware}}
\]

This is **not** a claim that these three numbers are always statistically independent observables; it is a **bookkeeping interface** so analog substrate noise can be carried in the same σ ledger as semantic uncertainty.

## Layer map (engineering pointers)

| Layer | σ-component | What it measures (intent) | Where it lives |
|---|---|---|---|
| Paper | \(K_{\mathrm{eff}}=(1-\sigma)\cdot K\) | Coherence invariant | External corpus / Zenodo (see `docs/REPRO_BUNDLE_TEMPLATE.md`) |
| Algorithm | σ_aleatoric | “World is ambiguous” (Dirichlet mass / spread) | `src/sigma/decompose.c` |
| Algorithm | σ_epistemic | “Model doesn’t know” (Dirichlet / evidence coupling) | `src/sigma/decompose.c` |
| Algorithm | σ_hardware | Memristor / analog array non-ideality remainder (lab scalar) | `src/sigma/decompose_v39.c` |
| Software | σ_logit_entropy | Token prediction uncertainty (scalar channel) | `src/sigma/channels.c` |
| Software | σ_top_margin | Decision margin (scalar channel) | `src/sigma/channels.c` |
| Inference | σ_spec_draft | Speculative decoding confidence hooks | `src/v35/spec_decode.c` |
| Protocol | σ MCP tools | Exposed to MCP clients | `src/mcp/sigma_server.c` |
| FPGA | σ_pipeline | Cross-head variance gate | `hdl/v37/sigma_pipeline.sv` |
| ASIC | σ_tile | Tiny-tile abstention / SPI σ interface | `hdl/asic/sigma_tile.sv` |
| Physics (toy) | σ_hw column counter | Digital noise stand-in for analog MAC columns | `hdl/neuromorphic/crossbar_sim.sv` |

## Neuromorphic / memristor substrate note

See `docs/neuromorphic/memristor_mapping.md` for the BitNet↔conductance story **as positioning**, not measured silicon.

## P-tier collaboration vectors (do not treat as shipped)

These are **planning contacts / directions**, not active partnerships:

1. **IHP (Leibniz Institute, Frankfurt Oder)** — 130 nm open PDK ecosystem; Tiny Tapeout moved here; natural ASIC partner lane for `hdl/asic/`.
2. **University of Manchester (SpiNNaker-2 group)** — large European neuromorphic research footprint; a credible place for “σ as a spike routing / abstention policy” conversations—*if* there is mutual interest and data.
3. **BrainChip (Akida)** — commercial neuromorphic SKU line; any “σ in SDK” story is **product/partnering**, not an in-repo claim.
4. **IMEC** — advanced semiconductor research; memristive crossbar research exists publicly; any collaboration is **P** until contracts + artifacts exist.

Rule: **do not** cold-contact from this repo alone; treat as roadmap until FPGA/ASIC lab artifacts and paper-grade measurements exist.

## Working paper title (only when data exists)

**“σ Across Substrates: A Unified Uncertainty Framework from Logits to Memristors”**

Thesis sentence: σ is **substrate-invariant in role** (a gate on coherence / continuation), even though its **instantiation** differs (semantic entropy vs device noise). The invariant story is **K_eff = (1−σ_total)·K**; the measurable part is what you archive per substrate.
