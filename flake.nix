{
  description = "C++ implementation of Mustache templates";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-26.05";
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
    nix-github-actions,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        src = gitignore.lib.gitignoreSource ./.;
        packageSrc = pkgs.lib.cleanSourceWith {
          name = "libmustache-source";
          inherit src;
          filter = gitignore.lib.gitignoreFilterWith {
            basePath = ./.;
            extraRules = ''
              .clang-format
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

        makePackage = args:
          pkgs.callPackage ./default.nix ({
              mustache_spec = mustache_spec.packages.${system}.mustache-spec;
              libmustacheSrc = packageSrc;
              inherit (gitignore.lib) gitignoreFilterWith;
            }
            // args);

        pre-commit-check = pre-commit-hooks.lib.${system}.run {
          inherit src;
          hooks = {
            actionlint.enable = true;
            alejandra.enable = true;
            clang-format = {
              enable = true;
              excludes = ["^vendor/"];
              types_or = pkgs.lib.mkForce ["c" "c++"];
            };
            shellcheck.enable = true;
            version-consistency = {
              enable = true;
              name = "version consistency";
              entry = "./scripts/check-version-consistency.sh";
              pass_filenames = false;
            };
          };
        };
      in rec {
        packages = rec {
          libmustache = makePackage {cmakeSupport = false;};
          libmustache-cmake = makePackage {cmakeSupport = true;};
          default = libmustache;
        };

        checks = {
          inherit (packages) libmustache libmustache-cmake;
          libmustache-static-only = makePackage {
            cmakeSupport = false;
            staticOnlySupport = true;
          };
          libmustache-sanitized = makePackage {
            cmakeSupport = true;
            debugSupport = true;
            sanitizerSupport = true;
          };
          libmustache-fuzz = makePackage {
            stdenv = pkgs.llvmPackages.stdenv;
            cmakeSupport = true;
            debugSupport = true;
            sanitizerSupport = true;
            fuzzSupport = true;
            archivedTemplateSupport = true;
          };
          libmustache-clang-tidy = makePackage {
            stdenv = pkgs.llvmPackages.stdenv;
            clang-tools = pkgs.llvmPackages.clang-tools;
            cmakeSupport = true;
            clangTidySupport = true;
          };
          libmustache-no-json = makePackage {
            cmakeSupport = false;
            nlohmann_json = null;
          };
          libmustache-minimal = makePackage {
            cmakeSupport = false;
            nlohmann_json = null;
            libyaml = null;
          };
          libmustache-cmake-no-yaml = makePackage {
            cmakeSupport = true;
            libyaml = null;
          };
          libmustache-cmake-minimal = makePackage {
            cmakeSupport = true;
            nlohmann_json = null;
            libyaml = null;
          };
          libmustache-cista-benchmark = makePackage {
            cmakeSupport = true;
            cista = pkgs.cista;
            xxhash = pkgs.xxhash;
            zlib = pkgs.zlib;
            debugSupport = true;
            sanitizerSupport = true;
            archivedTemplateSupport = true;
            useSystemCista = true;
            useSystemXxhash = true;
            cistaBenchmarkSupport = true;
            cistaBuiltinXxh3Support = true;
          };
          libmustache-cista-benchmark-vendored = makePackage {
            cmakeSupport = true;
            cista = null;
            xxhash = null;
            zlib = pkgs.zlib;
            cistaBenchmarkSupport = true;
            cistaBuiltinXxh3Support = true;
          };
          libmustache-archived-vendored-cmake = makePackage {
            cmakeSupport = true;
            cista = null;
            xxhash = null;
            zlib = pkgs.zlib;
            archivedTemplateSupport = true;
          };
          libmustache-archived-vendored-autotools = makePackage {
            cmakeSupport = false;
            cista = null;
            xxhash = null;
            zlib = pkgs.zlib;
            archivedTemplateSupport = true;
          };
          libmustache-archived-system-autotools = makePackage {
            cmakeSupport = false;
            cista = pkgs.cista;
            xxhash = pkgs.xxhash;
            zlib = pkgs.zlib;
            archivedTemplateSupport = true;
            useSystemCista = true;
            useSystemXxhash = true;
          };
          libmustache-archived-system-cista-vendored-xxhash-cmake = makePackage {
            cmakeSupport = true;
            cista = pkgs.cista;
            xxhash = null;
            zlib = pkgs.zlib;
            archivedTemplateSupport = true;
            useSystemCista = true;
          };
          libmustache-archived-vendored-cista-system-xxhash-cmake = makePackage {
            cmakeSupport = true;
            cista = null;
            xxhash = pkgs.xxhash;
            zlib = pkgs.zlib;
            archivedTemplateSupport = true;
            useSystemXxhash = true;
          };
          libmustache-archived-system-cista-vendored-xxhash-autotools = makePackage {
            cmakeSupport = false;
            cista = pkgs.cista;
            xxhash = null;
            zlib = pkgs.zlib;
            archivedTemplateSupport = true;
            useSystemCista = true;
          };
          libmustache-archived-vendored-cista-system-xxhash-autotools = makePackage {
            cmakeSupport = false;
            cista = null;
            xxhash = pkgs.xxhash;
            zlib = pkgs.zlib;
            archivedTemplateSupport = true;
            useSystemXxhash = true;
          };
          pre-commit = pre-commit-check;
        };

        apps = rec {
          mustachec =
            (flake-utils.lib.mkApp {
              drv = packages.libmustache;
              exePath = "/bin/mustachec";
            })
            // {inherit (packages.libmustache) meta;};
          default = mustachec;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [packages.libmustache packages.libmustache-cmake];
          packages =
            pre-commit-check.enabledPackages
            ++ [pkgs.cista pkgs.llvmPackages.clang-tools pkgs.xxhash pkgs.zlib];
          shellHook = pre-commit-check.shellHook;
        };

        formatter = pkgs.alejandra;
      }
    )
    // {
      githubActions.matrix.include = let
        cleanName = value:
          value
          // {
            name =
              builtins.replaceStrings
              ["githubActions." "checks." "x86_64-linux." "\""]
              ["" "" "" ""]
              value.attr;
          };
      in
        builtins.map cleanName
        (nix-github-actions.lib.mkGithubMatrix {
          attrPrefix = "checks";
          checks = nixpkgs.lib.getAttrs ["x86_64-linux"] self.checks;
        })
        .matrix
        .include;
    };
}
