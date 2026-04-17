# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v102 σ-Eval-Harness — thin launcher for `python -m lm_eval` that pre-imports
the `creation_os_backend` module so the `creation_os` model name is
registered in lm_eval's model registry *before* the CLI resolves
`--model creation_os`.

Without this launcher, `python -m lm_eval --model creation_os ...` fails
with `no model for this name found` because PYTHONPATH alone does not
trigger module execution / decorator registration.
"""

from __future__ import annotations

import sys


def main() -> int:
    import creation_os_backend  # noqa: F401  side-effect: registers "creation_os"
    from lm_eval.__main__ import cli_evaluate

    cli_evaluate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
