# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""LangChain callback: score every LLM call (requires optional dependency)."""

from __future__ import annotations

# Uncomment when langchain-core is installed:
#
# from langchain_core.callbacks import BaseCallbackHandler
# from cos.integrations.langchain import SigmaGateCallback
#
# handler = SigmaGateCallback(raise_on_abstain=False)
# chain.invoke({"input": "…"}, config={"callbacks": [handler]})

print("Optional: pip install 'creation-os[langchain]' langchain-core")
print("Then wire SigmaGateCallback from cos.integrations.langchain (see source).")
