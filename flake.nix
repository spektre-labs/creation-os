{
  description = "Creation OS — reproducible formal + portable C toolchain (Nix)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        formalPkgs = with pkgs; [
          clang
          gnumake
          pkg-config
          yosys
          verilator
          symbiyosys
          rustc
          cargo
          rustfmt
          clippy
          jdk17
          sbt
          git
          cacert
        ];
        omnigateRunners = with pkgs; [
          go
          julia
          deno
          nushell
          python3
          zig
          gawk
          perl
          rakudo
        ];
        cosShell = pkgs.mkShell {
          name = "creation-os";
          packages = formalPkgs;
          shellHook = ''
            echo "creation-os devShell: clang/make + yosys/verilator/symbiyosys + rust + jdk17/sbt"
            echo "  make merge-gate"
            echo "  just merge-gate   (same, via Just)"
          '';
        };
        omnigateShell = pkgs.mkShell {
          name = "creation-os-omnigate";
          packages = formalPkgs ++ omnigateRunners;
          shellHook = ''
            echo "OMNIGATE devShell: Go, Julia, Deno, Nushell, Zig, Awk, Perl, Raku + full creation-os toolchain"
            echo "  just omnigate-all"
            echo "  (see omnigate/README.md)"
          '';
        };
      in
      {
        devShells.default = cosShell;
        devShells.omnigate = omnigateShell;
        packages.default = pkgs.writeShellScriptBin "cos-merge-gate" ''
          set -euo pipefail
          root="''${CREATION_OS_ROOT:-.}"
          cd "$root"
          exec ${pkgs.gnumake}/bin/make merge-gate
        '';
      });
}
