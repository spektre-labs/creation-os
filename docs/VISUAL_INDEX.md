# Visual index — diagrams & figures (Creation OS)

All graphics in this tree are **original SVG** (vector, scalable) — **no stock photos** and **no unlicensed web bitmaps**. Safe for PDF export, slides, and dark/light GitHub themes (some figures use fixed palettes; preview in both modes if presenting).

| Asset | Use case |
|-------|----------|
| [assets/architecture-stack.svg](assets/architecture-stack.svg) | README / decks — single-file kernel over Hypercube, Oracle, World model → BSC core → Soul, Proconductor, Cognition |
| [assets/bsc-primitives.svg](assets/bsc-primitives.svg) | Teaching — XOR → MAJ → POPCNT / σ pipeline |
| [assets/gemm-vs-bsc-memory-ops.svg](assets/gemm-vs-bsc-memory-ops.svg) | Benchmark story — **32×** memory and **192×** op-proxy bars (same definitions as README table) |
| [assets/evidence-ladder.svg](assets/evidence-ladder.svg) | CLAIM_DISCIPLINE talks — one headline number, one evidence class |
| [assets/planes-abc.svg](assets/planes-abc.svg) | ANALYSIS / AGI map — Planes A–B–C at a glance |

## Embedding in Markdown (GitHub)

```markdown
![Architecture](docs/assets/architecture-stack.svg)
```

Paths are relative to the **repository root** of the `creation-os` checkout.

## Mermaid

The root [README.md](../README.md) includes a **Mermaid** flowchart (GitHub-native render). Edit that block if the parity story changes.

## Editing SVGs

Use any text editor or Inkscape / Figma import. Keep **fonts** as generic `ui-sans-serif, system-ui, sans-serif` or `ui-monospace` so CI and headless renders do not depend on custom font installs.

---

*Spektre Labs · Creation OS · 2026*
