let
  generateTestsForPlatform = { pkgs, ... }@args:
    pkgs.recurseIntoAttrs {
      gcc = pkgs.callPackage ../default.nix (args // {
        stdenv = pkgs.stdenv;
      });
      clang = pkgs.callPackage ../default.nix (args // {
        stdenv = pkgs.clangStdenv;
      });
    };
in
builtins.mapAttrs (k: _v:
  let
    path = builtins.fetchTarball {
      url = https://github.com/NixOS/nixpkgs-channels/archive/nixos-20.03.tar.gz;
      name = "nixos-20.03";
    };
    pkgs = import (path) { system = k; };
  in
  pkgs.recurseIntoAttrs {
    autotools = generateTestsForPlatform {
      inherit pkgs;
      cmakeSupport = false;
    };

    cmake = generateTestsForPlatform {
      inherit pkgs;
      cmakeSupport = true;
    };
  }
) {
  x86_64-linux = {};
  # Uncomment to test build on macOS too
  # x86_64-darwin = {};
}
