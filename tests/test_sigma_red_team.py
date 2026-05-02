# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
import os
import sys
import unittest


class TestSigmaRedTeam(unittest.TestCase):
    def test_mock_ci_zero_bypass(self):
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
        from cos.sigma_red_team import MockRedTeamModel, SigmaRedTeam
        from cos.scoring import QuickscoreGate
        from cos.sigma_robustness import SigmaRobustnessReport

        team = SigmaRedTeam(QuickscoreGate(), MockRedTeamModel())
        res = team.run_all([], n_per_attack=25)
        rep = SigmaRobustnessReport().generate(res)
        self.assertEqual(rep["total_bypassed"], 0)
        self.assertTrue(rep["overall_robust"])

    def test_confident_hallucination_not_accept_bypass(self):
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
        from cos.sigma_red_team import MockRedTeamModel, SigmaRedTeam
        from cos.scoring import QuickscoreGate

        team = SigmaRedTeam(QuickscoreGate(), MockRedTeamModel())
        att = team.confident_hallucination
        prompt = "What is the capital of France?"
        sigma, verdict = team._score(prompt, att(prompt))
        self.assertNotEqual(verdict, "ACCEPT")


class TestSigmaHardening(unittest.TestCase):
    def test_export_only(self):
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
        from cos.sigma_hardening import SigmaHardening
        from cos.scoring import QuickscoreGate

        results = {
            "fake": {
                "total": 1,
                "bypassed": 1,
                "bypass_rate": 1.0,
                "robust": False,
                "bypassed_cases": [
                    {"prompt": "p", "response": "r", "sigma": 0.1, "verdict": "ACCEPT"}
                ],
            }
        }
        out = SigmaHardening().harden(QuickscoreGate(), results)
        self.assertIsInstance(out, dict)
        self.assertEqual(out.get("status"), "export_only")


if __name__ == "__main__":
    unittest.main()
