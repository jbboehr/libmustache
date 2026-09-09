{
  pkgs,
  source,
  platform,
  linkage,
  stdenv ?
    if pkgs.stdenv.hostPlatform.isLinux
    then pkgs.gcc13Stdenv
    else pkgs.stdenv,
}: let
  inherit (pkgs) lib;
  version = (builtins.fromJSON (builtins.readFile ../vcpkg.json)).version-string;
  json = pkgs.buildPackages.nlohmann_json;
in
  stdenv.mkDerivation {
    pname = "libmustache-release-${platform}-${linkage}";
    inherit version;
    src = source;
    strictDeps = true;
    nativeBuildInputs = with pkgs.buildPackages;
      [cmake pkg-config python3]
      ++ lib.optional stdenv.hostPlatform.isLinux patchelf
      ++ lib.optionals stdenv.hostPlatform.isDarwin [darwin.cctools darwin.sigtool];
    buildInputs = [json];
    cmakeFlags =
      [
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_INSTALL_BINDIR=bin"
        "-DCMAKE_INSTALL_INCLUDEDIR=include"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
        "-DMUSTACHE_WARNINGS_AS_ERRORS=ON"
        "-DMUSTACHE_ENABLE_TESTS=ON"
        "-DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=ON"
        "-DMUSTACHE_ENABLE_JSON=ON"
        "-Dnlohmann_json_DIR=${json}/share/cmake/nlohmann_json"
        "-DMUSTACHE_ENABLE_YAML=OFF"
        (lib.cmakeBool "MUSTACHE_CLI_STATIC" (linkage == "static"))
      ]
      ++ lib.optional stdenv.hostPlatform.isDarwin "-DCMAKE_INSTALL_NAME_DIR=@rpath";
    enableParallelBuilding = true;
    doCheck = true;
    checkPhase = ''
      runHook preCheck
      ctest --output-on-failure --parallel 2
      python3 "$src/nix/package-unix-test.py"
      python3 "$src/tests/test_release.py" PublishingTests
      runHook postCheck
    '';
    installPhase = ''
      runHook preInstall
      cmake --install . --prefix "$TMPDIR/installed"
      python3 "$src/nix/package-unix.py" \
        --prefix "$TMPDIR/installed" --source "$src" \
        --json-license ${json.src}/LICENSE.MIT \
        --platform ${platform} --linkage ${linkage} --version ${version} \
        --output "$out"
      runHook postInstall
    '';
    # Archives already contain relocated binaries; Nix fixups must not re-add store paths.
    dontFixup = true;
    meta.platforms =
      if stdenv.hostPlatform.isDarwin
      then ["aarch64-darwin"]
      else ["x86_64-linux"];
  }
