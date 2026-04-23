#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Legacy name — delegates to sigma_separation.sh (same CSV + chart).
exec "$(dirname "$0")/sigma_separation.sh"
