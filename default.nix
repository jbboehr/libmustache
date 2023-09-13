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
    checkSupport ? true,
    cmakeSupport ? false,
    debugSupport ? false,
  }:
    pkgs.callPackage ./nix/derivation.nix {
      inherit stdenv;
      inherit mustache_spec;
      inherit checkSupport cmakeSupport debugSupport;
      inherit gitignoreFilterWith;
    }
