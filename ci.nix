let
  generateLibmustacheTestsForPlatform = { pkgs, stdenv }:
    pkgs.callPackage ./derivation.nix {
      inherit stdenv;

      libmustacheSrc = builtins.filterSource
        (path: type: baseNameOf path != ".idea" && baseNameOf path != ".git" && baseNameOf path != "ci.nix")
        ./.;

      mustache_spec = pkgs.callPackage (import ((fetchTarball {
        url = https://github.com/jbboehr/mustache-spec/archive/5b85c1b58309e241a6f7c09fa57bd1c7b16fa9be.tar.gz;
        sha256 = "1h9zsnj4h8qdnzji5l9f9zmdy1nyxnf8by9869plyn7qlk71gdyv";
      }))) {};
    };

  generateLibmustacheTestsForPlatform2 = { pkgs }:
    pkgs.recurseIntoAttrs {
      gcc = generateLibmustacheTestsForPlatform { inherit pkgs; stdenv = pkgs.stdenv; };
      clang = generateLibmustacheTestsForPlatform { inherit pkgs; stdenv = pkgs.clangStdenv; };
    };
in
builtins.mapAttrs (k: _v:
  let
    path = builtins.fetchTarball {
       url = https://github.com/NixOS/nixpkgs/archive/release-19.03.tar.gz;
       name = "nixpkgs-19.03";
    };
    pkgs = import (path) { system = k; };
  in
  pkgs.recurseIntoAttrs {
    n1809 = let
        path = builtins.fetchTarball {
           url = https://github.com/NixOS/nixpkgs/archive/release-18.09.tar.gz;
           name = "nixpkgs-18.09";
        };
        pkgs = import (path) { system = k; };
    in generateLibmustacheTestsForPlatform2  {
      inherit pkgs;
    };

    n1903 = let
        path = builtins.fetchTarball {
           url = https://github.com/NixOS/nixpkgs/archive/release-19.03.tar.gz;
           name = "nixpkgs-19.03";
        };
        pkgs = import (path) { system = k; };
    in generateLibmustacheTestsForPlatform2  {
      inherit pkgs;
    };

    n1909 = let
        path = builtins.fetchTarball {
           url = https://github.com/NixOS/nixpkgs/archive/release-19.09.tar.gz;
           name = "nixpkgs-19.09";
        };
        pkgs = import (path) { system = k; };
    in generateLibmustacheTestsForPlatform2 {
      inherit pkgs;
    };

    unstable = let
       path = builtins.fetchTarball {
          url = https://github.com/NixOS/nixpkgs/archive/master.tar.gz;
          name = "nixpkgs-unstable";
       };
       pkgs = import (path) { system = k; };
    in generateLibmustacheTestsForPlatform2 {
     inherit pkgs;
    };
  }
) {
  x86_64-linux = {};
  # Uncomment to test build on macOS too
  # x86_64-darwin = {};
}
