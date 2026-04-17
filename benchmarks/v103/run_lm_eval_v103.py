# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v103 launcher: imports `creation_os_v103_backend` before invoking
lm_eval.cli_evaluate so that the `creation_os_v103` model name is
registered in lm_eval's registry at the moment argparse resolves
`--model creation_os_v103`.

Accepts all the usual `python -m lm_eval` flags.  The backend's σ sidecar
path is controlled via the env var COS_V103_SIGMA_SIDECAR (set by
`run_sigma_log.sh` per task).
"""

from __future__ import annotations

import sys


def main() -> int:
    import creation_os_v103_backend  # noqa: F401  side-effect registration
    from lm_eval.__main__ import cli_evaluate

    cli_evaluate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
