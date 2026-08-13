let
  lock = builtins.fromJSON (builtins.readFile ./flake.lock);
in
  {
    pkgs ? import <nixpkgs> {},
    stdenv ? pkgs.stdenv,
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
    debugSupport ? false,
    sanitizerSupport ? false,
    fuzzSupport ? false,
  }:
    pkgs.callPackage ./nix/derivation.nix {
      inherit stdenv mustache_spec libmustacheSrc;
      inherit checkSupport cmakeSupport debugSupport sanitizerSupport fuzzSupport;
      inherit gitignoreFilterWith;
    }
