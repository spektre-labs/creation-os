#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Read one `cos chat --verbose` transcript from stdin; print sigma\\taction\\troute."""
import re
import sys

def main() -> None:
    t = sys.stdin.read()
    if not t:
        print("N/A\tN/A\tN/A")
        return
    sig = ""
    sigma = "\u03c3"
    for pat in (
        sigma + r"_combined=([0-9.]+)",
        r"sigma_combined=([0-9.]+)",
        r"SIGMA_COMBINED=([0-9.]+)",
    ):
        m = re.search(pat, t)
        if m:
            sig = m.group(1)
            break
    if not sig:
        m = re.search(r"\[" + sigma + r"=([0-9.]+)", t)
        if m:
            sig = m.group(1)

    act = "N/A"
    m = re.search(r"action=(ACCEPT|RETHINK|ABSTAIN)\b", t)
    if m:
        act = m.group(1)
    else:
        m = re.search(r"\[" + sigma + r"=[0-9.]+\s*\|\s*([A-Za-z_]+)", t)
        if m:
            act = m.group(1).upper()

    rte = "N/A"
    m = re.search(r"route=(LOCAL|ESCALATE)\b", t)
    if m:
        rte = m.group(1)
    else:
        m = re.search(r"route=([A-Z0-9_()]+)", t)
        if m:
            rte = m.group(1)

    print(sig or "N/A", act, rte, sep="\t")


if __name__ == "__main__":
    main()
