# Spektre Corpus — theory behind the code

The **Spektre Corpus** (~80 papers, **CC BY 4.0**) is the published theory layer for Creation OS. The canonical **Git-tracked paper pack** lives in the sibling repository:

**https://github.com/spektre-labs/corpus**

## Using the corpus with this repo

1. **Browse / clone** the corpus repo for PDFs, BibTeX, and Zenodo DOIs.
2. **Optional submodule** (advanced): add the corpus as a git submodule under `third_party/spektre-corpus` or `data/corpus/papers/` if you want a single checkout that includes both kernel and PDFs. Large PDF sets may require **Git LFS**; configure `.gitattributes` locally if you enable LFS.
3. **Zenodo** community uploads remain the long-term archive; see the root README **Theoretical foundation** section for the community URL.

## Index table

A full numbered table (`| # | Title | DOI | Pages |`) should be generated from the corpus repository’s own `README` or Zenodo metadata — **do not hand-duplicate** DOIs here (drift risk). Treat `github.com/spektre-labs/corpus` as the **source of truth** for the catalogue until a `make corpus-index` exporter exists.

---

*Spektre Labs · Creation OS · 2026*
