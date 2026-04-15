# Visual index — diagrams & figures (Creation OS)

All graphics in this tree are **original SVG** (vector, scalable) — **no stock photos** and **no unlicensed web bitmaps**. Safe for PDF export, slides, and GitHub light theme; dark-theme figures are labeled below.

## Asset catalog

| Asset | Theme | Use case |
|-------|--------|----------|
| [assets/kernel-lineage-evidence.svg](assets/kernel-lineage-evidence.svg) | Light | README doctoral path, thesis — **v2 portable proof vs v6–v12 lab demo (C)** |
| [assets/architecture-stack.svg](assets/architecture-stack.svg) | Dark | README / decks — single-file kernel over Hypercube, Oracle, World model → BSC core → Soul, Proconductor, Cognition |
| [assets/bsc-primitives.svg](assets/bsc-primitives.svg) | Light | Teaching — XOR → MAJ → POPCNT / σ definition strip |
| [assets/gemm-vs-bsc-memory-ops.svg](assets/gemm-vs-bsc-memory-ops.svg) | Light | Benchmark story — **32×** memory and **192×** op-proxy (README definitions) + legend |
| [assets/evidence-ladder.svg](assets/evidence-ladder.svg) | Dark | CLAIM_DISCIPLINE / committee — Arithmetic → Measured → Harness + lab-demo band |
| [assets/planes-abc.svg](assets/planes-abc.svg) | Light | ANALYSIS / AGI map — Planes A–B–C with flow arrows |

## Design system (editorial)

- **Typography:** `ui-sans-serif, system-ui, sans-serif` for prose labels; `ui-monospace, monospace` for code-like tokens (`creation_os_v2.c`, `make check-v12`). Avoid custom webfonts so CI and headless renders match GitHub.
- **Light figures** (`#fafafa` / `#f8fafc` backgrounds): print-first, keynote-friendly. **Drop shadow** via `feDropShadow` at low opacity (~0.06–0.08) for card lift — not heavy skeuomorphism.
- **Dark figures** (`#0f172a`–`#020617` backgrounds): deck contrast on projectors; top **accent hairline** (cyan–indigo gradient) as brand spine.
- **Accessibility:** every SVG ships `<title>` + `<desc>` and `role="img"` + `aria-labelledby` for screen readers and PDF export.
- **ASCII in `<text>`** when possible (use `/` not exotic punctuation) — avoids replacement glyphs and invalid UTF-8 in some rasterizers. See [COMMON_MISREADINGS.md](COMMON_MISREADINGS.md) for diagram vs policy.

## Embedding in Markdown (GitHub)

```markdown
![Architecture](docs/assets/architecture-stack.svg)
```

Paths are relative to the **repository root** of the `creation-os` checkout.

## Mermaid

The root [README.md](../README.md) includes a **Mermaid** flowchart (GitHub-native render). Edit that block if the parity story changes.

## Citation hygiene for figures

1. **Cite committed Markdown or SVG** at a specific revision; raster excerpts are not canonical for footnotes or small numerals.
2. **Evidence ladder** ([evidence-ladder.svg](assets/evidence-ladder.svg)) applies to **numbers** too: microbench rows do not graduate to harness claims without the matching protocol row.
3. **Lineage figure** ([kernel-lineage-evidence.svg](assets/kernel-lineage-evidence.svg)) is a **prose guardrail** — not a substitute for reading [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) and [RESEARCH_AND_THESIS_ARCHITECTURE.md](RESEARCH_AND_THESIS_ARCHITECTURE.md).
4. **Revision drift:** diff the cited commit against `main` here before attributing text that does not match the current tree.

## Editing SVGs

Use any text editor or Inkscape / Figma import. After visual edits, re-check **viewBox** clipping (titles and footers) and **contrast** (WCAG-ish: avoid gray-on-gray below ~4.5:1 for body labels).

---

*Spektre Labs · Creation OS · 2026*
