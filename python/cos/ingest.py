# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-ingest — document → chunks → triple candidates → σ-gated graph writes.

Optional: ``pymupdf`` (``fitz``) for PDF, ``python-docx`` for DOCX, LLM client for rich
extraction. Core path uses stdlib + regex only.
"""
from __future__ import annotations

import io
import json
import re
import zipfile
from html.parser import HTMLParser
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from cos.graph import SigmaGraph

__all__ = [
    "SigmaIngest",
    "chunk_text",
    "extract_triples_regex",
    "ingest",
    "load_document_text",
]


class _StripHTML(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self._chunks: List[str] = []

    def handle_data(self, data: str) -> None:
        t = data.strip()
        if t:
            self._chunks.append(t)

    def text(self) -> str:
        return "\n".join(self._chunks)


def load_document_text(path: str | Path) -> str:
    """Load plain text from supported document types (best-effort, optional heavy libs)."""
    p = Path(path).expanduser().resolve()
    if not p.is_file():
        raise FileNotFoundError(str(p))
    suf = p.suffix.lower()
    if suf in (".txt", ".md"):
        return p.read_text(encoding="utf-8", errors="replace")
    if suf in (".html", ".htm"):
        raw = p.read_text(encoding="utf-8", errors="replace")
        parser = _StripHTML()
        parser.feed(raw)
        return parser.text() or raw
    if suf == ".docx":
        return _docx_to_text(p)
    if suf == ".pdf":
        return _pdf_to_text(p)
    if suf == ".epub":
        return _epub_to_text(p)
    return p.read_text(encoding="utf-8", errors="replace")


def _docx_to_text(p: Path) -> str:
    try:
        from docx import Document  # type: ignore[import-not-found]
    except ImportError:
        Document = None  # type: ignore[misc, assignment]
    else:
        doc = Document(str(p))
        return "\n".join(paragraph.text for paragraph in doc.paragraphs if paragraph.text.strip())
    out: List[str] = []
    try:
        with zipfile.ZipFile(p) as zf:
            xml = zf.read("word/document.xml").decode("utf-8", errors="replace")
    except (KeyError, OSError, zipfile.BadZipFile):
        return ""
    for m in re.finditer(r"<w:t[^>]*>([^<]*)</w:t>", xml):
        t = m.group(1).strip()
        if t:
            out.append(t)
    return "\n".join(out)


def _pdf_to_text(p: Path) -> str:
    try:
        import fitz  # type: ignore[import-not-found]
    except ImportError:
        fitz = None  # type: ignore[misc, assignment]
    if fitz is not None:
        doc = fitz.open(str(p))
        try:
            return "\n".join(page.get_text() for page in doc)
        finally:
            doc.close()
    try:
        from pypdf import PdfReader  # type: ignore[import-not-found]
    except ImportError:
        data = p.read_bytes()
        strings = re.findall(rb"[\x20-\x7e\r\n]{4,}", data)
        return b"\n".join(strings).decode("ascii", errors="ignore")
    reader = PdfReader(io.BytesIO(p.read_bytes()))
    parts: List[str] = []
    for page in reader.pages:
        try:
            parts.append(page.extract_text() or "")
        except Exception:  # pragma: no cover
            continue
    return "\n".join(parts)


def _epub_to_text(p: Path) -> str:
    out: List[str] = []
    try:
        with zipfile.ZipFile(p) as zf:
            names = [n for n in zf.namelist() if n.endswith((".html", ".xhtml", ".htm"))]
            for name in sorted(names):
                raw = zf.read(name).decode("utf-8", errors="replace")
                parser = _StripHTML()
                parser.feed(raw)
                t = parser.text()
                if t:
                    out.append(t)
    except (OSError, zipfile.BadZipFile):
        return ""
    return "\n".join(out)


def chunk_text(text: str, *, max_chars: int = 800, overlap: int = 50) -> List[str]:
    """Sliding windows over ``text`` (character-based, lab chunker)."""
    t = str(text).strip()
    if not t:
        return []
    max_chars = max(64, int(max_chars))
    overlap = max(0, min(int(overlap), max_chars // 2))
    step = max(1, max_chars - overlap)
    out: List[str] = []
    for i in range(0, len(t), step):
        chunk = t[i : i + max_chars]
        if chunk:
            out.append(chunk)
    return out


def chunk_words(text: str, *, size: int = 1000) -> List[str]:
    """Fixed word windows (used by :class:`SigmaIngest`)."""
    words = str(text).split()
    size = max(1, int(size))
    return [" ".join(words[i : i + size]) for i in range(0, len(words), size)]


def extract_triples_regex(text: str) -> List[Tuple[str, str, str]]:
    """Naive line splitter: first token / second token / remainder (legacy tests)."""
    out: List[Tuple[str, str, str]] = []
    for raw in text.replace(".", ".\n").split("\n"):
        sent = raw.strip()
        if not sent:
            continue
        words = sent.split()
        if len(words) >= 3:
            subj = words[0]
            rel = words[1]
            obj = " ".join(words[2:]).rstrip(".")
            if subj and rel and obj:
                out.append((subj, rel, obj))
    return out


def _verdict_str(verdict: object) -> str:
    if hasattr(verdict, "name"):
        return str(getattr(verdict, "name"))
    raw = str(verdict)
    if "." in raw:
        return raw.rsplit(".", 1)[-1]
    return raw


class SigmaIngest:
    """File → text → triple extraction → σ-gate per triple (**ABSTAIN** never stored)."""

    SUPPORTED = {".txt", ".md", ".html", ".htm", ".pdf", ".docx", ".epub"}

    ENTITY_PATTERN = re.compile(r"\b([A-Z][a-z]+(?:\s[A-Z][a-z]+)*)\b")
    RELATION_PATTERNS: List[Tuple[str, str]] = [
        (r"(\w+)\s+(?:is|was)\s+(?:a|an|the)\s+(\w+)", "is_a"),
        (r"(\w+)\s+(?:created|invented|developed|built)\s+(\w+)", "created"),
        (r"(\w+)\s+(?:works at|employed by|joined)\s+(\w+)", "works_at"),
        (r"(\w+)\s+(?:located in|based in|from)\s+(\w+)", "located_in"),
        (r"(\w+)\s+(?:uses|utilizes|requires)\s+(\w+)", "uses"),
    ]

    def __init__(self, graph: SigmaGraph, gate: Any = None, llm_client: Any = None) -> None:
        self.graph = graph
        self.gate = gate if gate is not None else graph.gate
        self.llm = llm_client

    def ingest(self, file_path: str | Path, chunk_size: int = 1000) -> Dict[str, Any]:
        path = Path(file_path).expanduser().resolve()
        if path.suffix.lower() not in self.SUPPORTED:
            raise ValueError(f"Unsupported format: {path.suffix}")

        text = load_document_text(path)
        chunks = chunk_words(text, size=chunk_size)
        report: Dict[str, Any] = {
            "file": str(path),
            "chunks": len(chunks),
            "accepted": 0,
            "rejected": 0,
            "triples": [],
        }

        for chunk in chunks:
            for subj, rel, obj in self._extract(str(chunk)):
                prompt = f"Is it true that {subj} {rel} {obj}?"
                response = f"{subj} {rel} {obj}"
                sigma, verdict = self.gate.score(prompt, response)
                vn = _verdict_str(verdict)

                row = {
                    "subject": subj,
                    "relation": rel,
                    "object": obj,
                    "sigma": round(float(sigma), 4),
                    "verdict": vn,
                }
                report["triples"].append(row)

                if vn == "ABSTAIN":
                    report["rejected"] += 1
                    continue
                res = self.graph.add(subj, rel, obj, sigma=float(sigma), source=str(path))
                if res.get("added"):
                    report["accepted"] += 1
                else:
                    report["rejected"] += 1

        return report

    def _extract(self, chunk: str) -> List[Tuple[str, str, str]]:
        if self.llm is not None:
            return self._extract_llm(chunk)
        return self._extract_regex(chunk)

    def _extract_regex(self, chunk: str) -> List[Tuple[str, str, str]]:
        triples: List[Tuple[str, str, str]] = []
        for pattern, rel_type in self.RELATION_PATTERNS:
            for match in re.finditer(pattern, chunk, re.IGNORECASE):
                subj = match.group(1).strip()
                obj = match.group(2).strip()
                if subj and obj and subj.lower() != obj.lower():
                    triples.append((subj, rel_type, obj))
        return triples

    def _extract_llm(self, chunk: str) -> List[Tuple[str, str, str]]:
        model = getattr(self.llm, "model", None) or getattr(self.llm, "model_name", "gpt-4o-mini")
        prompt = (
            "Extract subject-predicate-object triples from this text. "
            "Return as JSON list of {subject, predicate, object}.\n\n"
            f"Text: {chunk[:2000]}"
        )
        try:
            resp = self.llm.chat.completions.create(
                model=model,
                messages=[{"role": "user", "content": prompt}],
                max_tokens=1000,
            )
            text = resp.choices[0].message.content or ""
            text = re.sub(r"```json|```", "", text).strip()
            triples_raw = json.loads(text)
            out: List[Tuple[str, str, str]] = []
            for t in triples_raw:
                if not isinstance(t, dict):
                    continue
                if not all(k in t for k in ("subject", "predicate", "object")):
                    continue
                out.append((str(t["subject"]), str(t["predicate"]), str(t["object"])))
            return out
        except Exception:
            return self._extract_regex(chunk)


def ingest(
    file_path: str | Path,
    gate: Any = None,
    graph: Optional[SigmaGraph] = None,
    *,
    max_chars: int = 800,
    overlap: int = 50,
) -> Dict[str, Any]:
    """Backward-compatible ingest: character chunks + naive triple lines, σ-gated.

    Prefer :class:`SigmaIngest` for relation-pattern extraction and document types.
    """
    g_inst = graph if graph is not None else SigmaGraph(gate=gate)
    gate_eff = gate if gate is not None else g_inst.gate
    path = Path(file_path).expanduser().resolve()
    text = load_document_text(path)
    chunks = chunk_text(text, max_chars=max_chars, overlap=overlap)
    sigma_rows: List[Dict[str, Any]] = []
    added = 0
    skipped_abstain = 0
    for ch in chunks:
        for subj, rel, obj in extract_triples_regex(ch):
            statement = f"{subj} {rel} {obj}"
            sigma, verdict = gate_eff.score(str(path), statement)
            vn = _verdict_str(verdict)
            sigma_rows.append(
                {
                    "subject": subj,
                    "relation": rel,
                    "object": obj,
                    "sigma": round(float(sigma), 4),
                    "verdict": vn,
                    "added": False,
                }
            )
            if vn == "ABSTAIN":
                skipped_abstain += 1
                continue
            res = g_inst.add(subj, rel, obj, sigma=float(sigma), source=str(path))
            sigma_rows[-1]["added"] = bool(res.get("added"))
            if res.get("added"):
                added += 1
    st = g_inst.stats()
    return {
        "path": str(path),
        "chunks": len(chunks),
        "triples_seen": len(sigma_rows),
        "triples_added": added,
        "skipped_abstain": skipped_abstain,
        "sigma_per_triple": sigma_rows,
        "graph_stats": st,
    }
