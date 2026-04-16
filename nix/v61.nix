{ pkgs ? import <nixpkgs> {} }:

# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v61 — reproducible Nix build recipe for creation_os_v61.
#
# Pins clang + the src/v61 source tree.  Evaluated under Nix with
#
#   nix build -f nix/v61.nix
#
# …produces a bit-for-bit identical binary regardless of host toolchain
# versions, because Nix guarantees hermetic inputs.  SLSA-3 provenance
# is emitted by the GitHub action `.github/workflows/slsa.yml`.

pkgs.stdenv.mkDerivation {
  pname   = "creation_os_v61";
  version = "61.0.1";

  src = ../src/v61;

  nativeBuildInputs = [ pkgs.clang ];

  dontConfigure = true;

  buildPhase = ''
    export SOURCE_DATE_EPOCH=0
    clang -O2 -std=c11 -Wall -Wextra \
      -D_FORTIFY_SOURCE=3 -fstack-protector-strong -fPIE \
      -I. -o creation_os_v61 creation_os_v61.c citadel.c -lm
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp creation_os_v61 $out/bin/
  '';

  meta = with pkgs.lib; {
    description = "Σ-Citadel: BLP+Biba lattice + attestation (reproducible Nix build)";
    license     = licenses.agpl3Plus;
    platforms   = platforms.unix;
  };
}
