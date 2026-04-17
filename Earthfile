# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Commercial:    licensing@spektre-labs.com
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Earthly — containerized merge-gate (same bar as CI; needs Docker + earthly).
# Run from this directory: earthly +merge-gate
VERSION 0.8

merge-gate:
    FROM nixos/nix:2.21.1
    RUN mkdir -p /etc/nix && echo "experimental-features = nix-command flakes" >> /etc/nix/nix.conf
    WORKDIR /cos
    COPY . .
    RUN nix flake lock --accept-flake-config
    RUN nix develop --accept-flake-config -c make merge-gate
