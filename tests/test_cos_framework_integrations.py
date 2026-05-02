# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
import os
import sys
import unittest


class TestSigmaGatedDecorator(unittest.TestCase):
    def test_pass_through_accept_band(self):
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
        from cos.decorators import sigma_gated

        @sigma_gated
        def echo(prompt: str) -> str:
            return "4"

        self.assertEqual(echo("What is 2+2?"), "4")

    def test_abstain_raises(self):
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
        from cos.decorators import SigmaAbstainError, sigma_gated

        @sigma_gated
        def bad(_prompt: str) -> str:
            return "Berlin"

        with self.assertRaises(SigmaAbstainError):
            bad("What is the capital of France?")


class TestLangGraphSigma(unittest.TestCase):
    def test_node_and_router(self):
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
        from cos.integrations.langgraph_sigma import sigma_gate_node, sigma_gate_router

        class _M:
            def __init__(self, c: str) -> None:
                self.content = c

        st = {"messages": [_M("What is the capital of France?"), _M("Berlin")]}
        out = sigma_gate_node(st)
        self.assertIn("verdict", out)
        self.assertAlmostEqual(out["sigma"], 0.89, places=2)
        self.assertEqual(sigma_gate_router(out), "abstain")


class TestAutoGenHook(unittest.TestCase):
    def test_hook_adds_sigma_fields(self):
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
        from cos.integrations.autogen_sigma import SigmaGateHook

        h = SigmaGateHook()
        m = {"content": "hello world"}
        out = h.process_message("a", "b", m)
        self.assertIn("sigma", out)
        self.assertIn("verdict", out)
        self.assertEqual(out["content"], "hello world")


if __name__ == "__main__":
    unittest.main()
