# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-report — markdown/HTML report helpers from **operator-supplied** numbers.

Do **not** treat generated tables as harness truth. Every export carries a
``CLAIM_DISCIPLINE`` footer. PDF requires an external renderer."""
from __future__ import annotations

import html
import json
from typing import Any, List, Mapping, Sequence

__all__ = ["SigmaReport"]


class SigmaReport:
    """M-tier tables, evidence ladders, r/MachineLearning-style drafts, compliance shells."""

    FOOTER = (
        "_All metrics are operator-supplied placeholders unless cited to archived harness JSON "
        "(see docs/CLAIM_DISCIPLINE.md). Do not merge microbench with leaderboard rows in one headline._"
    )

    def mtier_table_markdown(self, rows: Sequence[Mapping[str, Any]]) -> str:
        """Canonical multi-benchmark table (M-tier v2): metric, score, status, saturation."""
        lines = [
            "| Benchmark | Metric | Score | Status | Abstain% | smECE | SNR | Saturation |",
            "|-------------|--------|-------|--------|---------|-------|-----|------------|",
        ]

        def _fmt_cell(x: Any) -> str:
            if x is None:
                return "—"
            if isinstance(x, (float, int)):
                return str(round(float(x), 4)) if isinstance(x, float) else str(x)
            return str(x)

        for r in rows:
            lines.append(
                "| {b} | {m} | {sc} | {st} | {ab} | {ce} | {sn} | {sat} |".format(
                    b=r.get("benchmark", "—"),
                    m=r.get("metric", "—"),
                    sc=r.get("score_display", r.get("score", "—")),
                    st=r.get("status", "—"),
                    ab=_fmt_cell(r.get("abstention_rate")),
                    ce=_fmt_cell(r.get("calibration_gap_smECE")),
                    sn=_fmt_cell(r.get("SNR")),
                    sat=r.get("saturation_note", "—"),
                )
            )
        lines.extend(
            [
                "",
                "_σ-gate is a **detector**, not the generative model; negatives stay in the ladder._",
                "",
                self.FOOTER,
            ]
        )
        return "\n".join(lines)

    def generate_mtier_table(self, bench_results: Mapping[str, Any]) -> str:
        """M-tier markdown: v2 ``rows`` list, empty/placeholder map → canonical defaults, else legacy."""
        from cos.bench import default_mtier_rows, merge_mtier_metrics

        raw: dict[str, Any] = dict(bench_results or {})
        rows = raw.get("rows")
        if isinstance(rows, list) and rows:
            first = rows[0]
            if isinstance(first, Mapping) and ("benchmark" in first or "id" in first):
                return self.mtier_table_markdown(rows)
        if not raw:
            return self.mtier_table_markdown(default_mtier_rows())
        vals = list(raw.values())
        if vals and all(isinstance(v, Mapping) for v in vals) and not any(v for v in vals):
            return self.mtier_table_markdown(default_mtier_rows())
        if raw.keys() <= {"version", "rows", "not_agi_achieved", "claim_discipline"}:
            merged = merge_mtier_metrics(by_id={})
            return self.mtier_table_markdown(merged)

        lines = [
            "| Benchmark | AUROC | smECE | SNR | Abstain% |",
            "|-----------|-------|-------|-----|----------|",
        ]
        for name, row in raw.items():
            if name in ("version", "rows", "not_agi_achieved", "claim_discipline"):
                continue
            if not isinstance(row, Mapping):
                row = {}
            lines.append(
                "| {name} | {auroc} | {smece} | {snr} | {abst} |".format(
                    name=str(name),
                    auroc=row.get("auroc", row.get("AUROC", "—")),
                    smece=row.get("smece", row.get("smECE", "—")),
                    snr=row.get("snr", row.get("SNR", "—")),
                    abst=row.get("abstain_pct", row.get("Abstain%", "—")),
                )
            )
        lines.append("")
        lines.append(self.FOOTER)
        return "\n".join(lines)

    def generate_evidence_ladder(self, results: Mapping[str, Any]) -> str:
        pos = results.get("positives") or results.get("supporting") or []
        neg = results.get("negatives") or results.get("limitations") or []
        if not isinstance(pos, list):
            pos = [pos]
        if not isinstance(neg, list):
            neg = [neg]

        def _bullets(title: str, xs: List[Any]) -> List[str]:
            out = [f"### {title}", ""]
            for x in xs:
                out.append(f"- {x}")
            out.append("")
            return out

        lines = ["## Evidence ladder", ""]
        lines.extend(_bullets("Supporting (bind each row to an evidence class)", pos))
        lines.extend(_bullets("Negative / falsifiers / unknowns", neg))
        lines.append(self.FOOTER)
        return "\n".join(lines)

    def generate_reddit_post(
        self,
        mtier_markdown: str,
        evidence_markdown: str,
        repo_url: str,
    ) -> str:
        """r/MachineLearning-style self-post draft (table + honesty boilerplate)."""
        body = [
            "**Title (edit):** `creation-os` — σ-gate detector + **multi-benchmark** M-tier (not a single-AUROC claim)",
            "",
            "Hi r/MachineLearning —",
            "",
            "We do **not** lead with one TruthfulQA AUROC: that benchmark is **saturated** in 2026; we "
            "publish the **full table** including **negative** rows (HaluEval) and **pending** rows "
            "(SimpleQA, FACTS). See docs/CLAIM_DISCIPLINE.md + docs/REDDIT_MACHINELEARNING_STRATEGY.md.",
            "",
            "### M-tier table",
            "",
            mtier_markdown,
            "",
            "### Evidence / limitations",
            "",
            evidence_markdown,
            "",
            f"**Repo:** {repo_url}",
            "",
            self.FOOTER,
        ]
        return "\n".join(body)

    def generate_compliance_report(self, compliance_data: Mapping[str, Any]) -> str:
        """EU AI Act–style **outline** — legal review required; not legal advice."""
        lines = [
            "# Conformity assessment outline (draft)",
            "",
            "## System context",
            json.dumps(compliance_data.get("system", {}), indent=2, default=str),
            "",
            "## Risk class & mitigations (operator fills)",
            json.dumps(compliance_data.get("mitigations", {}), indent=2, default=str),
            "",
            "## Data governance (operator fills)",
            json.dumps(compliance_data.get("data_governance", {}), indent=2, default=str),
            "",
            "_This scaffold is not legal advice. Engage qualified counsel for EU AI Act filings._",
            "",
            self.FOOTER,
        ]
        return "\n".join(lines)

    def generate_changelog(self, from_version: str, to_version: str) -> str:
        return "\n".join(
            [
                f"# Changelog {from_version} → {to_version}",
                "",
                "- Auto stub: replace with `git log {from_version}..{to_version}` in CI.",
                "- Bump `pyproject` / package `__version__` with release notes.",
                "",
                self.FOOTER,
            ]
        )

    @staticmethod
    def _markdown_to_simple_html(md: str) -> str:
        esc = html.escape(md)
        return (
            "<!DOCTYPE html><html><head><meta charset='utf-8'/><title>creation-os report</title>"
            "</head><body><pre style='white-space:pre-wrap;font-family:monospace'>" + esc + "</pre></body></html>"
        )

    def format_body(self, markdown_body: str, format: str) -> str:
        fmt = str(format).lower().strip()
        if fmt == "markdown":
            return markdown_body
        if fmt == "html":
            return self._markdown_to_simple_html(markdown_body)
        if fmt == "pdf":
            raise ValueError(
                "PDF export is not bundled; render HTML with wkhtmltopdf/pandoc/weasyprint "
                "or install a PDF toolchain in your CI.",
            )
        raise ValueError(f"unknown format {format!r} (use markdown|html|pdf)")
