# Visual index — diagrams & figures (Creation OS)

All graphics in this tree are **original SVG** (vector, scalable) — **no stock photos** and **no unlicensed web bitmaps**. Safe for PDF export, slides, and GitHub light theme; dark-theme figures are labeled below.

## Asset catalog

| Asset | Theme | Use case |
|-------|--------|----------|
| [assets/kernel-lineage-evidence.svg](assets/kernel-lineage-evidence.svg) | Light | README doctoral path, thesis — **v2 portable proof vs v6–v26 lab demo (C)** |
| [assets/architecture-stack.svg](assets/architecture-stack.svg) | Dark | README / decks — single-file kernel over Hypercube, Oracle, World model → BSC core → Soul, Proconductor, Cognition |
| [assets/bsc-primitives.svg](assets/bsc-primitives.svg) | Light | Teaching — XOR → MAJ → POPCNT / σ definition strip |
| [assets/gemm-vs-bsc-memory-ops.svg](assets/gemm-vs-bsc-memory-ops.svg) | Light | Benchmark story — **32×** memory and **192×** op-proxy (README definitions) + legend |
| [assets/evidence-ladder.svg](assets/evidence-ladder.svg) | Dark | CLAIM_DISCIPLINE / committee — Arithmetic → Measured → Harness + lab-demo band |
| [assets/planes-abc.svg](assets/planes-abc.svg) | Light | ANALYSIS / AGI map — Planes A–B–C with flow arrows |
| [assets/reddit-hook-banner.svg](assets/reddit-hook-banner.svg) | Dark | README **above-the-fold** — drive-by hook + merge-gate / **184** checks @ v26 CTA (lab demo class) |
| [assets/readme-scan-map.svg](assets/readme-scan-map.svg) | **Light + dark** (`prefers-color-scheme` inside SVG) | README **Contents** — inverted pyramid / L1–L3 scan map (FIG 09) |
| [Monorepo landing hero](../../docs/assets/landing-hero.svg) | Dark | Parent **spektre-protocol** README banner (FIG 01 / archive display) |

## Figure register (stable IDs)

Use these IDs when citing figures in slides or thesis text so captions stay stable across edits:

| FIG ID | File |
|--------|------|
| **01** | [`../../docs/assets/landing-hero.svg`](../../docs/assets/landing-hero.svg) (monorepo root) |
| **02** | [`assets/reddit-hook-banner.svg`](assets/reddit-hook-banner.svg) |
| **03** | [`assets/evidence-ladder.svg`](assets/evidence-ladder.svg) |
| **04** | [`assets/kernel-lineage-evidence.svg`](assets/kernel-lineage-evidence.svg) |
| **05** | [`assets/planes-abc.svg`](assets/planes-abc.svg) |
| **06** | [`assets/bsc-primitives.svg`](assets/bsc-primitives.svg) |
| **07** | [`assets/gemm-vs-bsc-memory-ops.svg`](assets/gemm-vs-bsc-memory-ops.svg) |
| **08** | [`assets/architecture-stack.svg`](assets/architecture-stack.svg) |
| **09** | [`assets/readme-scan-map.svg`](assets/readme-scan-map.svg) |

## Flight-ops display language (mission-control inspired)

Figures use a **quiet engineering grid**, **corner index brackets**, **`FIG nn` plates** (monospace), **L1 / L2 / …** or **BAY A / BAY B** labels where a diagram has sequential or parallel bays, and a **telemetry strip** (mono footer: `MODE=…`, `VERIFY=…`, `ASSET=…`, `LOCKED` / `DISPLAY OK`). This is **visual discipline only**: **not** affiliated with any space agency, **not** a certification or flight-software claim, **not** a substitute for [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

## Design system (editorial)

- **Spektral tokens (dark):** background stack `#020617` → `#0f172a` → `#0c2444`; accent spine `#22d3ee` → `#38bdf8` → `#6366f1`; body `#f1f5f9` / muted `#94a3b8` / code `#7dd3fc`.
- **Spektral tokens (light):** surface `#ffffff` on wash `#f8fafc` → `#e2e8f0`; ink `#0f172a`; secondary `#64748b`; proof accent `#059669` / lab demo `#ea580c` (see kernel-lineage figure).
- **Typography:** `ui-sans-serif, system-ui, sans-serif` for prose labels; `ui-monospace, monospace` for code-like tokens (`creation_os_v2.c`, `make check-v26`). Avoid custom webfonts so CI and headless renders match GitHub.
- **Light figures** (`#fafafa` / `#f8fafc` backgrounds): print-first, keynote-friendly. **Drop shadow** via `feDropShadow` at low opacity (~0.06–0.08) for card lift — not heavy skeuomorphism.
- **Dark figures** (`#0f172a`–`#020617` backgrounds): deck contrast on projectors; top **accent hairline** (cyan–indigo gradient) as brand spine.
- **Accessibility:** every SVG ships `<title>` + `<desc>` and `role="img"` + `aria-labelledby` for screen readers and PDF export.
- **README on GitHub:** hero and inline figures use `decoding="async"`, optional `loading="lazy"` below the fold, `max-width: min(…px, 100%)`, `height: auto`, `border-radius`, and a **soft neutral shadow** (`box-shadow: 0 2px 14px rgba(15,23,42,0.09)`) so SVGs read as *cards* on the white canvas; clients that strip inline CSS still get `width="96%"` / `width="100%"` fallbacks. Badges are centered in a **two-row table** for alignment on narrow viewports. **FIG 09** is the primary light/dark adaptive asset on the landing README.
- **ASCII in `<text>`** when possible (use `/` not exotic punctuation) — avoids replacement glyphs and invalid UTF-8 in some rasterizers. See [COMMON_MISREADINGS.md](COMMON_MISREADINGS.md) for diagram vs policy.

## Embedding in Markdown (GitHub)

```markdown
![Architecture](docs/assets/architecture-stack.svg)
```

Paths are relative to the **repository root** of the `creation-os` checkout.

## Mermaid

The [README.md](../README.md) flowcharts use `%%{init: {'theme':'neutral'}}%%` so **GitHub light/dark** renders with similar contrast (avoid default “hot” Mermaid colors beside Spektral SVGs). The conceptual monorepo uses the same pattern on its landing page: [spektre-protocol README](https://github.com/spektre-labs/spektre-protocol/blob/main/README.md) (when that checkout includes this tree as `creation-os/`).

## Citation hygiene for figures

1. **Cite committed Markdown or SVG** at a specific revision; raster excerpts are not canonical for footnotes or small numerals.
2. **Evidence ladder** ([evidence-ladder.svg](assets/evidence-ladder.svg)) applies to **numbers** too: microbench rows do not graduate to harness claims without the matching protocol row.
3. **Lineage figure** ([kernel-lineage-evidence.svg](assets/kernel-lineage-evidence.svg)) is a **prose guardrail** — not a substitute for reading [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) and [RESEARCH_AND_THESIS_ARCHITECTURE.md](RESEARCH_AND_THESIS_ARCHITECTURE.md).
4. **Revision drift:** diff the cited commit against `main` here before attributing text that does not match the current tree.

## Editing SVGs

Use any text editor or Inkscape / Figma import. After visual edits, re-check **viewBox** clipping (titles and footers) and **contrast** (WCAG-ish: avoid gray-on-gray below ~4.5:1 for body labels).

---

*Spektre Labs · Creation OS · 2026*
