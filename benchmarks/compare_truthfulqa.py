#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Placeholder: compare two lm-eval JSON blobs once real runs exist."""
import argparse
import json


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--sigma", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()
    with open(args.baseline, "r", encoding="utf-8") as f:
        b = json.load(f)
    with open(args.sigma, "r", encoding="utf-8") as f:
        s = json.load(f)
    lines = [
        "# TruthfulQA compare (stub)\n\n",
        f"- baseline keys: {len(b)}\n",
        f"- sigma keys: {len(s)}\n",
        "\nReplace this script once real `lm_eval` JSON is archived.\n",
    ]
    with open(args.output, "w", encoding="utf-8") as f:
        f.writelines(lines)


if __name__ == "__main__":
    main()
