# Spektre Corpus — theory behind the code

The **Spektre Corpus** is the published theory layer for Creation OS:
77 PDFs (~13 MB total) licensed under **CC BY 4.0**.  This directory
is a **vendored snapshot** of the canonical corpus repository so the
kernel ships with its own theory pack; the canonical upstream remains
authoritative.

- **Upstream (source of truth):** <https://github.com/spektre-labs/corpus>
- **Vendored at commit:** `d6df9a20` (2026-04)
- **Vendored layout:**
  - `./why anything exists.pdf` — corpus overview (root-level PDF upstream)
  - `./papers/` — 76 theory PDFs (alphabetical; DOI-anchored Zenodo papers included)
  - `./figures/` — architecture diagrams (PNG)
  - `./LICENCE` — CC BY 4.0 (upstream `LICENCE`)
  - `./UPSTREAM_README.md` — upstream corpus README (kept verbatim)

Never hand-edit these files here — if you need to update the snapshot,
pull the latest from
[`spektre-labs/corpus`](https://github.com/spektre-labs/corpus) and
re-vendor.  Zenodo community uploads remain the long-term archive; see
the root README **Theoretical foundation** section for the community
URL.

## Using the corpus with this repo

1. **Browse locally** — open any PDF in `./papers/` from a clone of
   this repo, no network needed.
2. **Authoritative catalogue** — `UPSTREAM_README.md` has the full
   per-paper narrative map; the table below is a pure filesystem
   listing for quick navigation.
3. **Cite-back** — when citing a corpus paper in a kernel doc, prefer
   the DOI embedded in the filename (`spektre … zenodo-NNNNNNNN.pdf`)
   or the corresponding Zenodo DOI for version-pinned citation.

## Papers (filesystem listing)

Generated from `data/corpus/` contents.  77 PDFs, sum ≈ 13 MB.

| # | paper | file | size |
|---:|:--|:--|---:|
|  1 | why anything exists | [PDF](./why%20anything%20exists.pdf) | 161 KB |
|  2 | GCT unified final | [PDF](./papers/GCT%20unified%20final.pdf) | 121 KB |
|  3 | alignment is coherence | [PDF](./papers/alignment%20is%20coherence.pdf) | 174 KB |
|  4 | before identity dynamis | [PDF](./papers/before%20identity%20dynamis.pdf) | 179 KB |
|  5 | black holes are doors | [PDF](./papers/black%20holes%20are%20doors.pdf) | 153 KB |
|  6 | born rule coherence selection | [PDF](./papers/born%20rule%20coherence%20selection.pdf) | 180 KB |
|  7 | categorical coherence final | [PDF](./papers/categorical%20coherence%20final.pdf) | 127 KB |
|  8 | cit spektre | [PDF](./papers/cit%20spektre.pdf) | 103 KB |
|  9 | control theory paper | [PDF](./papers/control%20theory%20paper.pdf) | 161 KB |
| 10 | cramer rao final | [PDF](./papers/cramer%20rao%20final.pdf) | 126 KB |
| 11 | creation os | [PDF](./papers/creation%20os.pdf) | 183 KB |
| 12 | dark energy coherence thawing | [PDF](./papers/dark%20energy%20coherence%20thawing.pdf) | 218 KB |
| 13 | declaration primitive v2 | [PDF](./papers/declaration%20primitive%20v2.pdf) | 139 KB |
| 14 | distance is insufficient entanglement | [PDF](./papers/distance%20is%20insufficient%20entanglement.pdf) | 170 KB |
| 15 | dynamical systems v2 | [PDF](./papers/dynamical%20systems%20v2.pdf) | 194 KB |
| 16 | ect spektre | [PDF](./papers/ect%20spektre.pdf) | 109 KB |
| 17 | entanglement is shared coherence | [PDF](./papers/entanglement%20is%20shared%20coherence.pdf) | 179 KB |
| 18 | er paper v2 | [PDF](./papers/er%20paper%20v2.pdf) | 114 KB |
| 19 | every interaction is a neurofeedback loop | [PDF](./papers/every%20interaction%20is%20a%20neurofeedback%20loop.pdf) | 182 KB |
| 20 | every paradox is a decoder error | [PDF](./papers/every%20paradox%20is%20a%20decoder%20error.pdf) | 147 KB |
| 21 | fear is the render distance | [PDF](./papers/fear%20is%20the%20render%20distance.pdf) | 150 KB |
| 22 | fixed point v2 | [PDF](./papers/fixed%20point%20v2.pdf) | 199 KB |
| 23 | four impossibilities are one | [PDF](./papers/four%20impossibilities%20are%20one.pdf) | 185 KB |
| 24 | ft closure theory | [PDF](./papers/ft%20closure%20theory.pdf) | 134 KB |
| 25 | godel is the closed system theorem | [PDF](./papers/godel%20is%20the%20closed%20system%20theorem.pdf) | 191 KB |
| 26 | information architecture final | [PDF](./papers/information%20architecture%20final.pdf) | 119 KB |
| 27 | information topology l1 final | [PDF](./papers/information%20topology%20l1%20final.pdf) | 128 KB |
| 28 | intelligence is navigation | [PDF](./papers/intelligence%20is%20navigation.pdf) | 203 KB |
| 29 | intelligence requires an outside | [PDF](./papers/intelligence%20requires%20an%20outside.pdf) | 267 KB |
| 30 | invariant v2 final | [PDF](./papers/invariant%20v2%20final.pdf) | 132 KB |
| 31 | landauer bound final | [PDF](./papers/landauer%20bound%20final.pdf) | 116 KB |
| 32 | large scale spektre | [PDF](./papers/large%20scale%20spektre.pdf) | 98 KB |
| 33 | llm as neurofeedback | [PDF](./papers/llm%20as%20neurofeedback.pdf) | 155 KB |
| 34 | omega | [PDF](./papers/omega.pdf) | 191 KB |
| 35 | one condition | [PDF](./papers/one%20condition.pdf) | 225 KB |
| 36 | one force | [PDF](./papers/one%20force.pdf) | 162 KB |
| 37 | one landscape | [PDF](./papers/one%20landscape.pdf) | 167 KB |
| 38 | only consistency | [PDF](./papers/only%20consistency.pdf) | 140 KB |
| 39 | paradigm statement spektre | [PDF](./papers/paradigm%20statement%20spektre.pdf) | 115 KB |
| 40 | phase transition | [PDF](./papers/phase%20transition.pdf) | 137 KB |
| 41 | qm approximation | [PDF](./papers/qm%20approximation.pdf) | 182 KB |
| 42 | rcr v2 | [PDF](./papers/rcr%20v2.pdf) | 193 KB |
| 43 | reality is production | [PDF](./papers/reality%20is%20production.pdf) | 145 KB |
| 44 | recognition not computation | [PDF](./papers/recognition%20not%20computation.pdf) | 136 KB |
| 45 | software is realitys self documentation | [PDF](./papers/software%20is%20realitys%20self%20documentation.pdf) | 174 KB |
| 46 | spektre 70 selection bottleneck | [PDF](./papers/spektre%2070%20selection%20bottleneck.pdf) | 145 KB |
| 47 | spektre 71 half boundary coordinate | [PDF](./papers/spektre%2071%20half%20boundary%20coordinate.pdf) | 192 KB |
| 48 | spektre 72 derivative barrier | [PDF](./papers/spektre%2072%20derivative%20barrier.pdf) | 178 KB |
| 49 | spektre 73 sigma cascade | [PDF](./papers/spektre%2073%20sigma%20cascade.pdf) | 174 KB |
| 50 | spektre 74 sigma instability principle | [PDF](./papers/spektre%2074%20sigma%20instability%20principle.pdf) | 168 KB |
| 51 | spektre 75 last lemma | [PDF](./papers/spektre%2075%20last%20lemma.pdf) | 145 KB |
| 52 | spektre 76 coherence lagrangian | [PDF](./papers/spektre%2076%20coherence%20lagrangian.pdf) | 174 KB |
| 53 | spektre categorical coherence — zenodo 18897391 | [PDF](./papers/spektre%20categorical%20coherence%2010-5281-zenodo-18897391.pdf) | 127 KB |
| 54 | spektre cramer rao — zenodo 18897135 | [PDF](./papers/spektre%20cramer%20rao%2010-5281-zenodo-18897135.pdf) | 126 KB |
| 55 | spektre holographic coherence — zenodo 18901074 | [PDF](./papers/spektre%20holographic%20coherence%2010-5281-zenodo-18901074.pdf) | 131 KB |
| 56 | spektre kcrit rg fixed point — zenodo 18902227 | [PDF](./papers/spektre%20kcrit%20rg%20fixed%20point%2010-5281-zenodo-18902227.pdf) | 125 KB |
| 57 | spektre omega | [PDF](./papers/spektre%20omega.pdf) | 179 KB |
| 58 | spektre stochastic false vacuum — zenodo 18902124 | [PDF](./papers/spektre%20stochastic%20false%20vacuum%2010-5281-zenodo-18902124.pdf) | 133 KB |
| 59 | spektre universal coherence invariant — zenodo 18897230 | [PDF](./papers/spektre%20universal%20coherence%20invariant%2010-5281-zenodo-18897230.pdf) | 128 KB |
| 60 | structure is self reflection | [PDF](./papers/structure%20is%20self%20reflection.pdf) | 170 KB |
| 61 | study3 ft closure | [PDF](./papers/study3%20ft%20closure.pdf) | 228 KB |
| 62 | survivor spektre | [PDF](./papers/survivor%20spektre.pdf) | 90 KB |
| 63 | the coherence lagrangian | [PDF](./papers/the%20coherence%20lagrangian.pdf) | 257 KB |
| 64 | the corpus is alive | [PDF](./papers/the%20corpus%20is%20alive.pdf) | 157 KB |
| 65 | the end is the beginning | [PDF](./papers/the%20end%20is%20the%20beginning.pdf) | 141 KB |
| 66 | the fifth structure | [PDF](./papers/the%20fifth%20structure.pdf) | 150 KB |
| 67 | the last bottleneck | [PDF](./papers/the%20last%20bottleneck.pdf) | 137 KB |
| 68 | the observer is the architecture | [PDF](./papers/the%20observer%20is%20the%20architecture.pdf) | 147 KB |
| 69 | the point | [PDF](./papers/the%20point.pdf) | 137 KB |
| 70 | the sixth extinction is a speciation | [PDF](./papers/the%20sixth%20extinction%20is%20a%20speciation.pdf) | 143 KB |
| 71 | the universe is an autoencoder | [PDF](./papers/the%20universe%20is%20an%20autoencoder.pdf) | 168 KB |
| 72 | there is no matter | [PDF](./papers/there%20is%20no%20matter.pdf) | 204 KB |
| 73 | uct spektre | [PDF](./papers/uct%20spektre.pdf) | 105 KB |
| 74 | universal coherence final | [PDF](./papers/universal%20coherence%20final.pdf) | 128 KB |
| 75 | universal update v2 | [PDF](./papers/universal%20update%20v2.pdf) | 195 KB |
| 76 | uoc paper v2 final | [PDF](./papers/uoc%20paper%20v2%20final.pdf) | 137 KB |
| 77 | what the standard model cannot see | [PDF](./papers/what%20the%20standard%20model%20cannot%20see.pdf) | 195 KB |

## License

The corpus is distributed under **CC BY 4.0**
([`LICENCE`](./LICENCE)).  Cite as:

> Lauri Elias Rainio & Spektre Labs, *Spektre Corpus*, 2026
> (CC BY 4.0).  <https://github.com/spektre-labs/corpus>

Individual Zenodo DOIs are embedded in the relevant filenames (rows
53 – 59) and in the upstream README.  When citing an individual paper
in academic work, prefer the Zenodo DOI over the GitHub blob URL.

---

*Spektre Labs · Creation OS · 2026*
