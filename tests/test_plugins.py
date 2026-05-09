# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.plugins` entry-point registry (no network)."""
from __future__ import annotations

import importlib.metadata
from unittest.mock import MagicMock, patch

from cos.plugins import PluginRegistry


def test_discover_returns_list() -> None:
    fake_ep = importlib.metadata.EntryPoint(
        name="demo",
        value="math:sqrt",
        group="cos.probes",
    )
    mock_eps = MagicMock()
    mock_eps.select = MagicMock(return_value=[fake_ep])
    with patch("cos.plugins.importlib.metadata.entry_points", return_value=mock_eps):
        reg = PluginRegistry()
        found = reg.discover("cos.probes")
    assert isinstance(found, list)
    assert len(found) == 1
    assert found[0]["name"] == "demo"
    assert found[0]["module"] == "math:sqrt"
    assert found[0]["group"] == "cos.probes"
    mock_eps.select.assert_called_once_with(group="cos.probes")


def test_load_handles_missing() -> None:
    fake_ep = importlib.metadata.EntryPoint(
        name="broken",
        value="no_such_module_for_plugins_xyz:Klass",
        group="cos.probes",
    )
    mock_eps = MagicMock()
    mock_eps.select = MagicMock(return_value=[fake_ep])
    with patch("cos.plugins.importlib.metadata.entry_points", return_value=mock_eps):
        reg = PluginRegistry()
        bucket = reg.load("cos.probes")
    assert "broken" in bucket
    assert "error" in bucket["broken"]
    assert reg.get_probe("broken") == bucket["broken"]


def test_list_all_groups() -> None:
    mock_eps = MagicMock()
    mock_eps.select = MagicMock(return_value=[])
    with patch("cos.plugins.importlib.metadata.entry_points", return_value=mock_eps):
        reg = PluginRegistry()
        all_plugins = reg.list_all()
    assert set(all_plugins.keys()) == set(PluginRegistry.GROUPS.keys())
    for _g, info in all_plugins.items():
        assert info["count"] == 0
        assert info["plugins"] == []
        assert "description" in info
    called = {c.kwargs.get("group") for c in mock_eps.select.call_args_list}
    assert called == set(PluginRegistry.GROUPS.keys())
