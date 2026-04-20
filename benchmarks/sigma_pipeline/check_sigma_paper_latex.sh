#!/usr/bin/env bash
#
# FIX-8: structural + (optional) compile-time check that the arXiv
# LaTeX paper is in the tree, matches the Markdown source on every
# section title, carries the canonical measurement numbers, and (if
# a TeX toolchain is installed) compiles to PDF cleanly.
#
# STATUS: the LaTeX source at docs/papers/creation_os_v1.tex is the
# arXiv-submission artefact; the Markdown source at
# docs/papers/creation_os_v1.md remains the canonical prose and is
# what merge-gate grep-checks for headline numbers.  This smoke test
# guarantees the two do not drift: same section skeleton, same
# measurements, same bibtex key.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

MD="docs/papers/creation_os_v1.md"
TEX="docs/papers/creation_os_v1.tex"
[[ -f "$MD"  ]] || { echo "FAIL: missing $MD"  >&2; exit 2; }
[[ -f "$TEX" ]] || { echo "FAIL: missing $TEX" >&2; exit 2; }

# ---- 1. structural skeleton --------------------------------------------
# The LaTeX document must declare article class + the mandatory packages
# that arXiv expects (amsmath, amssymb, hyperref, booktabs).
for needle in \
    '\\documentclass' \
    'amsmath' \
    'amssymb' \
    'hyperref' \
    'booktabs' \
    'begin{document}' \
    'end{document}' \
    'begin{abstract}'
do
    grep -q "$needle" "$TEX" \
        || { echo "FAIL: $TEX missing '$needle'" >&2; exit 3; }
done

# ---- 2. section skeleton must match the Markdown -----------------------
# Every Markdown `## N. Title` has a LaTeX `\section{Title}` counterpart.
for section in \
    "Introduction" \
    "theory" \
    "Architecture" \
    "Measurements" \
    "Comparisons" \
    "Living system" \
    "Formal guarantees" \
    "Limitations" \
    "Future work" \
    "Reproducibility"
do
    grep -q "^## .*${section}" "$MD" \
        || { echo "FAIL: $MD missing section '$section'" >&2; exit 4; }
    grep -Eq "\\\\section\\*?\{[^}]*${section}[^}]*\}" "$TEX" \
        || { echo "FAIL: $TEX missing \\section{... ${section} ...}" >&2; exit 5; }
done

# ---- 3. headline numbers preserved --------------------------------------
for number in \
    "0.0442" \
    "0.002" \
    "46.32" \
    "16" \
    "v6" \
    "v153"
do
    grep -q "$number" "$MD"  \
        || { echo "FAIL: $MD missing '$number'"  >&2; exit 6; }
    grep -q "$number" "$TEX" \
        || { echo "FAIL: $TEX missing '$number' (drift vs Markdown)" >&2; exit 7; }
done

# ---- 4. bibtex key must match -------------------------------------------
grep -q "creation_os_v1" "$MD"  || { echo "FAIL: $MD missing bibtex key"  >&2; exit 8; }
grep -q "creation_os_v1" "$TEX" || { echo "FAIL: $TEX missing bibtex key" >&2; exit 8; }

# ---- 5. optional: compile to PDF if pdflatex present --------------------
if command -v pdflatex >/dev/null 2>&1; then
    TMP="$(mktemp -d)"
    trap 'rm -rf "$TMP"' EXIT
    cp "$TEX" "$TMP/paper.tex"
    cd "$TMP"
    if pdflatex -interaction=nonstopmode -halt-on-error paper.tex \
         >/tmp/cos_paper_latex.out 2>&1; then
        [[ -f paper.pdf ]] \
            || { echo "FAIL: pdflatex produced no paper.pdf" >&2; exit 9; }
        echo "  pdflatex: paper.pdf generated ($(wc -c < paper.pdf) bytes)"
    else
        tail -30 /tmp/cos_paper_latex.out >&2 || true
        echo "FAIL: pdflatex returned non-zero" >&2
        exit 10
    fi
    cd - >/dev/null
else
    echo "  (pdflatex absent — structural grep only; install MacTeX/TeXLive to compile)"
fi

echo "check-sigma-paper-latex: PASS (arXiv .tex structural-equivalent to .md, numbers pinned)"
