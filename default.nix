{
  pkgs ? import <nixpkgs> {},
  libmustacheVersion ? null,
  libmustacheSrc ? ./.,
  libmustacheSha256 ? null,
  mustache_spec ? pkgs.callPackage (import (fetchTarball {
    url = https://github.com/jbboehr/mustache-spec/archive/5b85c1b58309e241a6f7c09fa57bd1c7b16fa9be.tar.gz;
    sha256 = "1h9zsnj4h8qdnzji5l9f9zmdy1nyxnf8by9869plyn7qlk71gdyv";
  })) {},
}:

pkgs.callPackage ./derivation.nix {
  inherit mustache_spec libmustacheVersion libmustacheSrc libmustacheSha256;
}

