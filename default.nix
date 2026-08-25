let
  lock = builtins.fromJSON (builtins.readFile ./flake.lock);
in
  {
    pkgs ? import <nixpkgs> {},
    stdenv ? pkgs.stdenv,
    clang-tools ? pkgs.llvmPackages.clang-tools,
    cista ? pkgs.cista,
    gitignoreFilterWith ?
      (import (
        fetchTarball {
          url = "https://github.com/hercules-ci/gitignore/archive/${lock.nodes.gitignore.locked.rev}.tar.gz";
          sha256 = lock.nodes.gitignore.locked.narHash;
        }
      ) {inherit (pkgs) lib;})
      .gitignoreFilterWith,
    mustache_spec ?
      pkgs.callPackage (import (fetchTarball {
        url = "https://github.com/jbboehr/mustache-spec/archive/${lock.nodes.mustache_spec.locked.rev}.tar.gz";
        sha256 = lock.nodes.mustache_spec.locked.narHash;
      })) {},
    libmustacheSrc ? ./.,
    checkSupport ? true,
    cmakeSupport ? false,
    staticOnlySupport ? false,
    debugSupport ? false,
    sanitizerSupport ? false,
    fuzzSupport ? false,
    clangTidySupport ? false,
    cistaBenchmarkSupport ? false,
  }:
    pkgs.callPackage ./nix/derivation.nix {
      inherit stdenv clang-tools cista mustache_spec libmustacheSrc;
      inherit checkSupport cmakeSupport staticOnlySupport debugSupport sanitizerSupport fuzzSupport clangTidySupport;
      inherit cistaBenchmarkSupport;
      inherit gitignoreFilterWith;
    }
