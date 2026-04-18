# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""v142 σ-Interop merge-gate tests (stdlib only, no network)."""
from __future__ import annotations

import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
PKG  = os.path.abspath(os.path.join(HERE, ".."))
if PKG not in sys.path:
    sys.path.insert(0, PKG)

from creation_os import COS, ChatMessage, ChatResponse  # noqa: E402
from creation_os.openai_compat import (                  # noqa: E402
    check_openai_drop_in,
    sample_openai_headers,
    sample_openai_response,
)


class SDKShapeTest(unittest.TestCase):
    def test_openai_response_parses(self) -> None:
        """v106 OpenAI-compat response → ChatResponse with σ + specialist."""
        cos = COS()
        resp: ChatResponse = cos.parse_openai_response(
            sample_openai_response(), sample_openai_headers()
        )
        self.assertEqual(resp.text, "Hello from Creation OS.")
        self.assertEqual(resp.role, "assistant")
        self.assertIsNotNone(resp.sigma_product)
        self.assertAlmostEqual(resp.sigma_product or 0.0, 0.231847, places=4)
        self.assertEqual(resp.specialist, "bitnet-2b-reasoning")
        self.assertTrue(resp.emitted)

    def test_headers_are_lowercased(self) -> None:
        cos = COS()
        resp = cos.parse_openai_response(
            sample_openai_response(), sample_openai_headers()
        )
        self.assertIn("x-cos-sigma-product", resp.headers)
        self.assertIn("x-cos-specialist", resp.headers)

    def test_abstain_response(self) -> None:
        abstain = sample_openai_response()
        abstain["choices"][0]["message"]["content"] = ""
        abstain["choices"][0]["finish_reason"] = "abstain"
        cos = COS()
        resp = cos.parse_openai_response(abstain, sample_openai_headers())
        self.assertEqual(resp.text, "")
        self.assertFalse(resp.emitted)

    def test_messages_normalisation(self) -> None:
        cos = COS()
        msg = ChatMessage(role="user", content="hi")
        self.assertEqual(cos._normalize_messages("hi"), [{"role": "user", "content": "hi"}])
        self.assertEqual(cos._normalize_messages([msg]), [{"role": "user", "content": "hi"}])
        self.assertEqual(
            cos._normalize_messages([{"role": "system", "content": "s"}]),
            [{"role": "system", "content": "s"}],
        )


class AdapterImportTest(unittest.TestCase):
    def test_langchain_adapter_imports(self) -> None:
        """LangChain adapter must import even without langchain installed."""
        import creation_os.langchain_adapter as la  # noqa: F401
        self.assertIn("LANGCHAIN_AVAILABLE", dir(la))

    def test_llamaindex_adapter_imports(self) -> None:
        import creation_os.llamaindex_adapter as li  # noqa: F401
        self.assertIn("LLAMAINDEX_AVAILABLE", dir(li))

    def test_llamaindex_query_engine_usable_without_framework(self) -> None:
        """CreationOSQueryEngine must be constructable without LlamaIndex."""
        from creation_os.llamaindex_adapter import CreationOSQueryEngine
        eng = CreationOSQueryEngine()
        self.assertIsNotNone(eng)

    def test_openai_drop_in_probe_is_robust(self) -> None:
        """openai drop-in probe must return (False, reason), not raise."""
        ok, reason = check_openai_drop_in()
        self.assertIsInstance(ok, bool)
        self.assertIsInstance(reason, str)
        self.assertGreater(len(reason), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
