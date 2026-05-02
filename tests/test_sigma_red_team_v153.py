# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""v153 adversarial suite tests (quickstart gate + mock model)."""
from __future__ import annotations

import os
import sys
import unittest


class TestSigmaRedTeamV153(unittest.TestCase):
    def setUp(self) -> None:
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))

    def test_injection_sigma_not_trivially_low(self) -> None:
        from cos.sigma_red_team import MockRedTeamModel, SigmaRedTeam
        from cos.scoring import QuickscoreGate

        team = SigmaRedTeam(QuickscoreGate(), MockRedTeamModel())
        out = team.attack_injection()
        self.assertTrue(out["resilient"])
        self.assertEqual(out["bypassed"], 0)

    def test_encoding_not_bypassed(self) -> None:
        from cos.sigma_red_team import MockRedTeamModel, SigmaRedTeam
        from cos.scoring import QuickscoreGate

        team = SigmaRedTeam(QuickscoreGate(), MockRedTeamModel())
        out = team.attack_encoding()
        self.assertEqual(out["bypassed"], 0)
        self.assertTrue(out["resilient"])

    def test_multiturn_sigma_trace_present(self) -> None:
        from cos.sigma_red_team import MockRedTeamModel, SigmaRedTeam
        from cos.scoring import QuickscoreGate

        team = SigmaRedTeam(QuickscoreGate(), MockRedTeamModel())
        out = team.attack_multiturn()
        trace = out.get("sigma_trace") or []
        self.assertEqual(len(trace), 4)
        self.assertEqual(out["bypassed"], 0)
        self.assertIn("drift_alert", out)

    def test_adversarial_suite_all_resilient_mock(self) -> None:
        from cos.sigma_red_team import MockRedTeamModel, SigmaRedTeam
        from cos.scoring import QuickscoreGate

        team = SigmaRedTeam(QuickscoreGate(), MockRedTeamModel())
        summary = team.run_adversarial_suite()
        self.assertTrue(summary["all_resilient"])
        self.assertEqual(summary["total_bypassed"], 0)

    def test_sigma_red_team_hardening_no_actions_when_clean(self) -> None:
        from cos.sigma_red_team import SigmaRedTeamHardening

        summary = {
            "per_attack": [
                {"attack": "injection", "total": 1, "bypassed": 0, "resilient": True},
            ]
        }
        plan = SigmaRedTeamHardening(object(), summary).harden()
        self.assertEqual(plan["hardened"], 0)
        self.assertEqual(plan["actions"], [])


if __name__ == "__main__":
    unittest.main()
