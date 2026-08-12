{
  lib,
  stdenv,
  pkg-config,
  libtool,
  m4,
  autoconf,
  automake,
  autoreconfHook,
  cmake ? null,
  json_c,
  libyaml,
  mustache_spec,
  gitignoreFilterWith,
  libmustacheSrc ? ../.,
  checkSupport ? true,
  cmakeSupport ? false,
  debugSupport ? false,
  sanitizerSupport ? false,
}:
stdenv.mkDerivation (finalAttrs: {
  pname =
    "libmustache"
    + lib.optionalString cmakeSupport "-cmake"
    + lib.optionalString sanitizerSupport "-sanitized";
  version = "0.5.0";

  src =
    if builtins.isPath libmustacheSrc
    then
      lib.cleanSourceWith {
        name = "libmustache-source";
        src = libmustacheSrc;
        filter = gitignoreFilterWith {
          basePath = libmustacheSrc;
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
      }
    else libmustacheSrc;

  outputs = ["out" "dev" "lib"];
  strictDeps = true;
  enableParallelBuilding = true;
  separateDebugInfo = debugSupport;
  dontStrip = sanitizerSupport;

  buildInputs = [json_c libyaml];
  propagatedBuildInputs = [json_c libyaml];
  nativeBuildInputs =
    lib.optional checkSupport mustache_spec
    ++ lib.optionals cmakeSupport [cmake]
    ++ lib.optionals (!cmakeSupport) [autoreconfHook libtool m4 autoconf automake]
    ++ [pkg-config];

  doCheck = checkSupport;

  configureFlags =
    [
      "--libdir=${placeholder "lib"}/lib"
      "--includedir=${placeholder "dev"}/include"
    ]
    ++ lib.optional checkSupport "--with-mustache-spec=${mustache_spec}/share/mustache-spec/specs"
    ++ lib.optional sanitizerSupport "--enable-sanitizers";

  cmakeFlags =
    [
      (lib.cmakeBool "MUSTACHE_ENABLE_TESTS" checkSupport)
      (lib.cmakeBool "MUSTACHE_ENABLE_HARDENING" true)
      (lib.cmakeBool "MUSTACHE_ENABLE_ASSERTIONS" true)
      (lib.cmakeBool "MUSTACHE_ENABLE_SANITIZERS" sanitizerSupport)
      "-DCMAKE_BUILD_TYPE=${
        if debugSupport || sanitizerSupport
        then "Debug"
        else "Release"
      }"
    ]
    ++ lib.optional checkSupport "-DMUSTACHE_SPEC_DIR=${mustache_spec}/share/mustache-spec/specs";

  postInstall = lib.optionalString stdenv.hostPlatform.isLinux ''
    patchelf --add-rpath "$lib/lib" "$out/bin/mustachec"
  '';

  doInstallCheck = true;
  nativeInstallCheckInputs = lib.optional cmakeSupport cmake ++ [pkg-config];
  installCheckPhase =
    ''
      runHook preInstallCheck

      "$out/bin/mustachec" -v
      printf '%s\n' '{{name}}' > install-check.mustache
      printf '%s\n' '{"name":"secure"}' > install-check.json
      rendered=$("$out/bin/mustachec" -t install-check.mustache -d install-check.json)
      test "$rendered" = "secure"

      PKG_CONFIG_PATH="$dev/lib/pkgconfig''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
        pkg-config --exact-version="${finalAttrs.version}" mustache
    ''
    + lib.optionalString cmakeSupport ''
      consumer_cmake_flags=()
      ${lib.optionalString sanitizerSupport ''
        consumer_cmake_flags+=(
          '-DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all'
          '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all'
        )
      ''}
      cmake -S "$src/tests/cmake-consumer" -B consumer-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$dev;$lib" \
        "''${consumer_cmake_flags[@]}"
        cmake --build consumer-build --parallel
        consumer-build/mustache_consumer
    ''
    + ''
      runHook postInstallCheck
    '';

  meta = {
    description = "C++ implementation of Mustache templates";
    homepage = "https://github.com/jbboehr/libmustache";
    license = lib.licenses.mit;
    mainProgram = "mustachec";
    platforms = lib.platforms.linux;
  };
})
