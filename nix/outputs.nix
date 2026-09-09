{
  self,
  nixpkgs,
  agent-badge,
  flake-utils,
  mustache_spec,
  gitignore,
  pre-commit-hooks,
  nix-github-actions,
  ...
}:
nixpkgs.lib.recursiveUpdate (flake-utils.lib.eachDefaultSystem (
  system: let
    pkgs = nixpkgs.legacyPackages.${system};
    windowsPkgs = import nixpkgs {
      inherit system;
      config.allowUnfreePredicate = pkg:
        builtins.elem (pkgs.lib.getName pkg) ["win-sdk" "xwin-fetch-msvc"];
      config.microsoftVisualStudioLicenseAccepted = true;
    };
    windowsPackages = pkgs.lib.optionalAttrs (system == "x86_64-linux") (
      pkgs.lib.listToAttrs (map (linkage: {
        name = "libmustache-windows-x64-xwin-${linkage}";
        value = windowsPkgs.callPackage ./windows.nix {
          sdk = windowsPkgs.windows.sdk;
          source = packageSrc;
          inherit linkage;
        };
      }) ["static" "shared"])
    );
    src = gitignore.lib.gitignoreSource ../.;
    packageSrc = pkgs.lib.cleanSourceWith {
      name = "libmustache-source";
      inherit src;
      filter = gitignore.lib.gitignoreFilterWith {
        basePath = ../.;
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
      pkgs.callPackage ../default.nix ({
          inherit mustache_spec;
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
    packages =
      rec {
        libmustache = makePackage {cmakeSupport = false;};
        libmustache-cmake = makePackage {cmakeSupport = true;};
        default = libmustache;
        release-linux-x64-musl-static = pkgs.pkgsCross.musl64.callPackage ./release.nix {
          source = src;
          platform = "linux-x64-musl";
          linkage = "static";
          stdenv = pkgs.pkgsCross.musl64.stdenv;
        };
      }
      // windowsPackages;

    checks =
      windowsPackages
      // (import ./checks.nix {inherit pkgs makePackage;})
      // {
        inherit (packages) libmustache libmustache-cmake;
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
        ++ [agent-badge.packages.${system}.default pkgs.cista pkgs.llvmPackages.clang-tools pkgs.xxhash pkgs.zlib];
      shellHook = pre-commit-check.shellHook;
    };

    formatter = pkgs.alejandra;
  }
)) (flake-utils.lib.eachSystem ["x86_64-linux" "aarch64-darwin"] (system: let
  pkgs = nixpkgs.legacyPackages.${system};
  platform =
    if system == "x86_64-linux"
    then "linux-x64-glibc"
    else "macos-aarch64";
in {
  packages = pkgs.lib.listToAttrs (map (linkage: {
    name = "release-${platform}-${linkage}";
    value = import ./release.nix {
      inherit pkgs;
      source = gitignore.lib.gitignoreSource ../.;
      inherit platform linkage;
    };
  }) ["static" "shared"]);
}))
// {
  githubActions.matrix =
    (nix-github-actions.lib.mkGithubMatrix {
      attrPrefix = "checks";
      inherit (self) checks;
    })
      .matrix;
}
