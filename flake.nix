{
  description = "libmustache";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/23.05";
    flake-utils.url = "github:numtide/flake-utils";
    mustache_spec.url = "github:jbboehr/mustache-spec";
    gitignore = {
      url = "github:hercules-ci/gitignore.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    mustache_spec,
    gitignore,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
      in rec {
        packages = flake-utils.lib.flattenTree rec {
          libmustache = pkgs.callPackage ./default.nix {
            mustache_spec = mustache_spec.packages.${system}.mustache-spec;
            inherit (gitignore.lib) gitignoreFilterWith;
            cmakeSupport = false;
          };
          libmustache-cmake = pkgs.callPackage ./default.nix {
            mustache_spec = mustache_spec.packages.${system}.mustache-spec;
            inherit (gitignore.lib) gitignoreFilterWith;
            cmakeSupport = true;
          };
          default = libmustache;
        };

        checks = packages;

        formatter = pkgs.alejandra;
      }
    );
}
