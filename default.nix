{
  pkgs ? import <nixpkgs> {},
  libmustacheVersion ? null,
  libmustacheSrc ? null,
  libmustacheSha256 ? null,
  mustacheSpecVersion ? null,
  mustacheSpecSrc ? null,
  mustacheSpecSha256 ? null,
  mustache_spec ? pkgs.callPackage (import ((fetchTarball {
    url = https://github.com/jbboehr/mustache-spec/archive/210109d9ccd05171233d8d7a8ceb97f3bddc790a.tar.gz;
    sha256 = "0ss51lraznpbixahfdj7j7wg41l1gnwq9qbm9aw2lkc9vvsc3h3c";
  }) + "/derivation.nix")) { inherit mustacheSpecVersion mustacheSpecSrc mustacheSpecSha256; },
}:

pkgs.callPackage ./derivation.nix {
  inherit mustache_spec libmustacheVersion libmustacheSrc libmustacheSha256;
}

