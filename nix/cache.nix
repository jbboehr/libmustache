{
  pkgs,
  checks,
  packages,
}: let
  # Only include release packages that CI builds on this host.
  releases = pkgs.lib.filterAttrs (name: package:
    pkgs.lib.hasPrefix "release-" name
    && pkgs.lib.meta.availableOn pkgs.stdenv.hostPlatform package)
  packages;
in
  pkgs.linkFarm "libmustache-ci-dependencies" (pkgs.lib.mapAttrsToList (name: package: {
      inherit name;
      # Keeps all build-time references, including SDKs embedded in toolchain files.
      path = package.inputDerivation;
    })
    (checks // releases))
