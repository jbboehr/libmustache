{
  description = "libmustache";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-23.11";
    systems.url = "github:nix-systems/default-linux";
    flake-utils = {
      url = "github:numtide/flake-utils";
      inputs.systems.follows = "systems";
    };
    gitignore = {
      url = "github:hercules-ci/gitignore.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    pre-commit-hooks = {
      url = "github:cachix/pre-commit-hooks.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    nix-github-actions = {
      url = "github:nix-community/nix-github-actions";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    mustache_spec.url = "github:jbboehr/mustache-spec";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    mustache_spec,
    gitignore,
    pre-commit-hooks,
    systems,
    nix-github-actions,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = pkgs.lib;
        src' = gitignore.lib.gitignoreSource ./.;

        src = pkgs.lib.cleanSourceWith {
          name = "libmustache-source";
          src = src';
          filter = gitignore.lib.gitignoreFilterWith {
            basePath = ./.;
            extraRules = ''
              .clang-format
              composer.json
              composer.lock
              .editorconfig
              .envrc
              .gitattributes
              .github
              .gitignore
              *.md
              *.nix
              flake.*
            '';
          };
        };

        pre-commit-check = pre-commit-hooks.lib.${system}.run {
          src = src';
          hooks = {
            actionlint.enable = true;
            alejandra.enable = true;
            shellcheck.enable = true;
          };
        };
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

        devShells.default = pkgs.mkShell {
          inputsFrom = [packages.default];
          shellHook = ''
            ${pre-commit-check.shellHook}
          '';
        };

        formatter = pkgs.alejandra;
      }
    )
    // {
      # prolly gonna break at some point
      githubActions.matrix.include = let
        cleanFn = v: v // {name = builtins.replaceStrings ["githubActions." "checks." "x86_64-linux."] ["" "" ""] v.attr;};
      in
        builtins.map cleanFn
        (nix-github-actions.lib.mkGithubMatrix {
          attrPrefix = "checks";
          checks = nixpkgs.lib.getAttrs ["x86_64-linux"] self.checks;
        })
        .matrix
        .include;
    };
}
