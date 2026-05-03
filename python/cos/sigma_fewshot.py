# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v183 σ-fewshot: prototypical embeddings + in-context generation + σ transfer check (lab).

**Not MAML/Open-MAML:** centroid + ICL is a lightweight stand-in for meta-learning.
See ``docs/CLAIM_DISCIPLINE.md`` — no few-shot accuracy claims without a harness.
"""
from __future__ import annotations

import hashlib
import math
from typing import Any, Dict, List, Mapping, Sequence, Tuple

Pair = Tuple[str, str]


def _verdict_str(verdict: Any) -> str:
    return str(getattr(verdict, "name", verdict))


class HashEmbeddingEncoder:
    """Deterministic L2-normalized bag-of-shingles encoder (no trainable weights)."""

    def __init__(self, *, dim: int = 32, ngram: int = 3) -> None:
        self.dim = max(4, int(dim))
        self.ngram = max(1, int(ngram))

    def encode(self, text: str) -> List[float]:
        t = str(text).lower().strip()
        if not t:
            return [0.0] * self.dim
        grams: List[str] = []
        if len(t) < self.ngram:
            grams = [t]
        else:
            for i in range(len(t) - self.ngram + 1):
                grams.append(t[i : i + self.ngram])
        vec = [0.0] * self.dim
        for g in grams:
            h = hashlib.sha256(g.encode("utf-8")).digest()
            idx = int.from_bytes(h[:2], "big") % self.dim
            vec[idx] += float(int.from_bytes(h[2:4], "big")) / float(1 << 16)
        norm = math.sqrt(sum(x * x for x in vec)) or 1.0
        return [x / norm for x in vec]


def _lab_generate(model: Any, prompt: str) -> str:
    try:
        return str(model.generate(prompt))
    except Exception:
        return str(model.generate(prompt[:4000]))


class SigmaFewShot:
    """Prototype centroids over encoder space; σ gates support coherence and transfer."""

    def __init__(self, gate: Any, model: Any, encoder: Any) -> None:
        self.gate = gate
        self.model = model
        self.encoder = encoder
        self.prototypes: Dict[str, Dict[str, Any]] = {}

    def learn(self, task_name: str, support_set: Sequence[Pair]) -> Dict[str, Any]:
        if len(support_set) < 1:
            return {"learned": False, "reason": "need at least 1 example"}
        rows = [(str(a), str(b)) for a, b in support_set]
        enc_fn = getattr(self.encoder, "encode", None)
        if not callable(enc_fn):
            return {"learned": False, "reason": "encoder.encode missing"}
        embeddings = [list(enc_fn(inp)) for inp, _ in rows]
        prototype = self.compute_centroid(embeddings)
        coherence: List[float] = []
        for inp, out in rows:
            s, _ = self.gate.score(inp, out)
            coherence.append(float(s))
        avg_sigma = sum(coherence) / max(len(coherence), 1)
        if avg_sigma > 0.5:
            return {
                "learned": False,
                "reason": f"support set incoherent (avg_σ={avg_sigma:.3f})",
                "avg_sigma": avg_sigma,
            }
        self.prototypes[str(task_name)] = {
            "centroid": prototype,
            "support_set": rows,
            "n_examples": len(rows),
            "avg_sigma": avg_sigma,
        }
        return {
            "learned": True,
            "task": str(task_name),
            "n_examples": len(rows),
            "avg_sigma": avg_sigma,
        }

    def predict(self, task_name: str, query: str) -> Dict[str, Any]:
        tid = str(task_name)
        if tid not in self.prototypes:
            return {"error": f"task {tid} not learned"}
        proto = self.prototypes[tid]
        prompt = self.build_icl_prompt(proto["support_set"], str(query))
        response = _lab_generate(self.model, prompt)
        sigma, verdict = self.gate.score(str(query), response)
        enc_fn = getattr(self.encoder, "encode", None)
        if not callable(enc_fn):
            return {"error": "encoder.encode missing"}
        query_emb = list(enc_fn(str(query)))
        centroid = list(proto["centroid"])
        distance = self.euclidean_distance(query_emb, centroid)
        transfer_confidence = max(0.0, 1.0 - float(sigma) - float(distance) * 0.1)
        return {
            "response": response,
            "sigma": float(sigma),
            "verdict": _verdict_str(verdict),
            "distance_to_prototype": distance,
            "transfer_confidence": transfer_confidence,
            "n_examples_used": int(proto["n_examples"]),
        }

    def adapt(self, task_name: str, new_example: Pair) -> Dict[str, Any]:
        tid = str(task_name)
        ne = (str(new_example[0]), str(new_example[1]))
        if tid not in self.prototypes:
            return self.learn(tid, [ne])
        proto = self.prototypes[tid]
        ss: List[Pair] = list(proto["support_set"])
        ss.append(ne)
        enc_fn = getattr(self.encoder, "encode", None)
        if not callable(enc_fn):
            return {"adapted": False, "reason": "encoder.encode missing"}
        embeddings = [list(enc_fn(inp)) for inp, _ in ss]
        proto["support_set"] = ss
        proto["centroid"] = self.compute_centroid(embeddings)
        proto["n_examples"] = len(ss)
        coh: List[float] = []
        for inp, out in ss:
            s, _ = self.gate.score(inp, out)
            coh.append(float(s))
        proto["avg_sigma"] = sum(coh) / max(len(coh), 1)
        return {"adapted": True, "n_examples": proto["n_examples"], "avg_sigma": proto["avg_sigma"]}

    def build_icl_prompt(self, support_set: Sequence[Pair], query: str) -> str:
        examples = "\n".join(f"Input: {inp}\nOutput: {out}" for inp, out in support_set)
        return f"{examples}\n\nInput: {query}\nOutput:"

    def compute_centroid(self, embeddings: Sequence[Sequence[float]]) -> List[float]:
        if not embeddings:
            return []
        dim = len(embeddings[0])
        acc = [0.0] * dim
        for emb in embeddings:
            for i in range(dim):
                acc[i] += float(emb[i])
        n = float(len(embeddings))
        return [x / n for x in acc]

    @staticmethod
    def euclidean_distance(a: Sequence[float], b: Sequence[float]) -> float:
        return float(sum((float(a[i]) - float(b[i])) ** 2 for i in range(len(a))) ** 0.5)

    def export_state(self) -> Dict[str, Any]:
        return {"prototypes": dict(self.prototypes)}

    def import_state(self, blob: Mapping[str, Any]) -> None:
        raw = blob.get("prototypes")
        if isinstance(raw, dict):
            self.prototypes = {str(k): dict(v) for k, v in raw.items() if isinstance(v, dict)}

    def hdc_support_bundle_similarity(self, task_name: str, query: str, *, dim: int = 256) -> Dict[str, Any]:
        """Hypervector similarity between bundled support triples and a query triple (NumPy required)."""
        try:
            from cos.hypervector import HDCodebook, HyperVector
        except ImportError as exc:  # pragma: no cover
            return {"ok": False, "error": str(exc)}
        tid = str(task_name)
        proto = self.prototypes.get(tid)
        if not proto:
            return {"ok": False, "error": "task not learned"}
        book = HDCodebook(int(dim), seed=abs(hash(tid)) % (2**31))
        support: List[Pair] = list(proto.get("support_set") or [])
        if not support:
            return {"ok": False, "error": "empty support"}
        triple_hvs = []
        for inp, out in support:
            s = book[str(inp)[:80]]
            r = book["rel"]
            o = book[str(out)[:80]]
            triple_hvs.append(HyperVector.bind(s, HyperVector.bind(r, HyperVector.permute(o, 1))))
        bundle = HyperVector.bundle(triple_hvs)
        q_in = str(query)[:80]
        q = HyperVector.bind(
            book[q_in],
            HyperVector.bind(book["rel"], HyperVector.permute(book[q_in], 1)),
        )
        sim = HyperVector.similarity(bundle, q)
        return {"ok": True, "similarity": round(float(sim), 4), "dim": int(dim)}
