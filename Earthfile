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
