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
  clang-tools ? null,
  cista ? null,
  xxhash ? null,
  zlib ? null,
  nlohmann_json ? null,
  libyaml ? null,
  mustache_spec,
  gitignoreFilterWith,
  libmustacheSrc ? ../.,
  checkSupport ? true,
  cmakeSupport ? false,
  staticOnlySupport ? false,
  debugSupport ? false,
  sanitizerSupport ? false,
  threadSanitizerSupport ? false,
  fuzzSupport ? false,
  clangTidySupport ? false,
  archivedTemplateSupport ? null,
  useSystemCista ? false,
  useSystemXxhash ? false,
  cistaBenchmarkSupport ? false,
  cistaBuiltinXxh3Support ? false,
}:
stdenv.mkDerivation (finalAttrs: {
  pname =
    "libmustache"
    + lib.optionalString cmakeSupport "-cmake"
    + lib.optionalString staticOnlySupport "-static-only"
    + lib.optionalString sanitizerSupport "-sanitized"
    + lib.optionalString threadSanitizerSupport "-thread-sanitized"
    + lib.optionalString fuzzSupport "-fuzz"
    + lib.optionalString clangTidySupport "-clang-tidy"
    + lib.optionalString (archivedTemplateSupport == true) "-archived-templates"
    + lib.optionalString useSystemCista "-system-cista"
    + lib.optionalString useSystemXxhash "-system-xxhash"
    + lib.optionalString cistaBenchmarkSupport "-cista-benchmark"
    + lib.optionalString cistaBuiltinXxh3Support "-builtin-xxh3";
  version = "0.6.0";

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
  enableParallelBuilding = !threadSanitizerSupport;
  enableParallelChecking = !threadSanitizerSupport;
  separateDebugInfo = debugSupport;
  dontStrip = sanitizerSupport || threadSanitizerSupport || fuzzSupport;
  TSAN_OPTIONS = lib.optionalString threadSanitizerSupport "halt_on_error=1:second_deadlock_stack=1";

  buildInputs = assert archivedTemplateSupport == null || builtins.isBool archivedTemplateSupport;
  assert !sanitizerSupport || !threadSanitizerSupport;
  assert !threadSanitizerSupport || !fuzzSupport;
  assert !cistaBenchmarkSupport || cmakeSupport;
  assert !useSystemCista || archivedTemplateSupport != false || (cmakeSupport && cistaBenchmarkSupport);
  assert !useSystemCista || cista != null;
  assert !useSystemXxhash || archivedTemplateSupport != false || (cmakeSupport && cistaBenchmarkSupport);
  assert !useSystemXxhash || xxhash != null;
  assert !cistaBenchmarkSupport || zlib != null;
    lib.optional (nlohmann_json != null) nlohmann_json
    ++ lib.optional (useSystemCista && (archivedTemplateSupport != false || cistaBenchmarkSupport)) cista
    ++ lib.optional (useSystemXxhash && (archivedTemplateSupport != false || cistaBenchmarkSupport)) xxhash
    ++ lib.optional cistaBenchmarkSupport zlib;
  propagatedBuildInputs = lib.optional (libyaml != null) libyaml;
  nativeBuildInputs =
    lib.optional checkSupport mustache_spec
    ++ lib.optionals cmakeSupport [cmake]
    ++ lib.optional clangTidySupport clang-tools
    ++ lib.optionals (!cmakeSupport) [autoreconfHook libtool m4 autoconf automake]
    ++ [pkg-config];

  doCheck = checkSupport;

  # The Nix source filter intentionally excludes packaging metadata such as
  # *.nix. Repository-level checks remain strict; packaged-source checks skip
  # only manifests that are absent from that filtered source.
  preCheck = lib.optionalString (!cmakeSupport) ''
    export MUSTACHE_VERSION_CHECK_ALLOW_MISSING=1
  '';

  configureFlags =
    [
      "--libdir=${placeholder "lib"}/lib"
      "--includedir=${placeholder "dev"}/include"
    ]
    ++ lib.optional checkSupport "--with-mustache-spec=${mustache_spec}/share/mustache-spec/specs"
    ++ lib.optional checkSupport "--enable-warnings-as-errors"
    ++ lib.optionals staticOnlySupport ["--disable-shared" "--enable-static"]
    ++ lib.optional sanitizerSupport "--enable-sanitizers"
    ++ lib.optional threadSanitizerSupport "--enable-thread-sanitizer"
    ++ lib.optional (archivedTemplateSupport == true) "--enable-archived-templates"
    ++ lib.optional (archivedTemplateSupport == false) "--disable-archived-templates"
    ++ lib.optional useSystemCista "--with-system-cista"
    ++ lib.optional useSystemXxhash "--with-system-xxhash"
    ++ lib.optional (nlohmann_json == null) "--without-json"
    ++ lib.optional (libyaml == null) "--without-yaml";

  cmakeFlags = assert !cistaBuiltinXxh3Support || cistaBenchmarkSupport;
    [
      (lib.cmakeBool "MUSTACHE_ENABLE_TESTS" checkSupport)
      (lib.cmakeBool "MUSTACHE_ENABLE_HARDENING" true)
      (lib.cmakeBool "MUSTACHE_ENABLE_ASSERTIONS" true)
      (lib.cmakeBool "MUSTACHE_WARNINGS_AS_ERRORS" checkSupport)
      (lib.cmakeBool "MUSTACHE_ENABLE_SANITIZERS" sanitizerSupport)
      (lib.cmakeBool "MUSTACHE_ENABLE_THREAD_SANITIZER" threadSanitizerSupport)
      (lib.cmakeBool "MUSTACHE_ENABLE_FUZZING" fuzzSupport)
      (lib.cmakeBool "MUSTACHE_ENABLE_CLANG_TIDY" clangTidySupport)
      "-DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=${
        if archivedTemplateSupport == null
        then "AUTO"
        else if archivedTemplateSupport
        then "ON"
        else "OFF"
      }"
      (lib.cmakeBool "MUSTACHE_USE_SYSTEM_CISTA" useSystemCista)
      (lib.cmakeBool "MUSTACHE_USE_SYSTEM_XXHASH" useSystemXxhash)
      (lib.cmakeBool "MUSTACHE_ENABLE_CISTA_BENCHMARK" cistaBenchmarkSupport)
      (lib.cmakeBool "MUSTACHE_CISTA_BENCHMARK_BUILTIN_XXH3" cistaBuiltinXxh3Support)
      "-DMUSTACHE_ENABLE_JSON=${
        if nlohmann_json == null
        then "OFF"
        else "AUTO"
      }"
      "-DMUSTACHE_ENABLE_YAML=${
        if libyaml == null
        then "OFF"
        else "AUTO"
      }"
      "-DCMAKE_BUILD_TYPE=${
        if debugSupport || sanitizerSupport || threadSanitizerSupport
        then "Debug"
        else "Release"
      }"
    ]
    ++ lib.optional checkSupport "-DMUSTACHE_SPEC_DIR=${mustache_spec}/share/mustache-spec/specs";

  postBuild =
    lib.optionalString (
      cmakeSupport && archivedTemplateSupport != false && !cistaBenchmarkSupport
    ) ''
      if test -e benchmarks/mustache_ast_cache_vs_source; then
        echo "archive-only CMake build unexpectedly included the AST benchmark" >&2
        exit 1
      fi
    '';

  postInstall = lib.optionalString stdenv.hostPlatform.isLinux ''
    patchelf --add-rpath "$lib/lib" "$out/bin/mustachec"
  '';

  doInstallCheck = true;
  nativeInstallCheckInputs = lib.optional cmakeSupport cmake ++ [pkg-config];
  installCheckPhase =
    ''
      runHook preInstallCheck

      "$out/bin/mustachec" -v > install-check-version.txt
      grep -F '${
        if nlohmann_json != null
        then "JSON support: nlohmann/json"
        else "JSON support: none"
      }' \
        install-check-version.txt
      if ${
        if archivedTemplateSupport != false
        then "true"
        else "false"
      }; then
        grep -F '#define MUSTACHE_HAVE_ARCHIVED_TEMPLATES 1' \
          "$dev/include/mustache/mustache_config.h"
      elif grep -q -F '#define MUSTACHE_HAVE_ARCHIVED_TEMPLATES 1' \
          "$dev/include/mustache/mustache_config.h"; then
        echo "archived-template support was enabled unexpectedly" >&2
        exit 1
      fi
      grep -F '${
        if libyaml != null
        then "YAML support: libyaml"
        else "YAML support: none"
      }' \
        install-check-version.txt
      ${
        if nlohmann_json != null
        then ''
          printf '%s\n' '{{name}}' > install-check.mustache
          printf '%s\n' '{"name":"secure"}' > install-check.json
          rendered=$("$out/bin/mustachec" -t install-check.mustache -d install-check.json)
        ''
        else if libyaml != null
        then ''
          printf '%s\n' '{{name}}' > install-check.mustache
          printf '%s\n' 'name: secure' > install-check.yaml
          rendered=$("$out/bin/mustachec" -t install-check.mustache -d install-check.yaml)
        ''
        else ''
          printf '%s' 'secure' > install-check.mustache
          rendered=$("$out/bin/mustachec" -t install-check.mustache)
        ''
      }
      test "$rendered" = "secure"

      export PKG_CONFIG_PATH="$dev/lib/pkgconfig''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
      pkg-config --exact-version="${finalAttrs.version}" mustache
      read -r -a pkg_config_flags <<< "$(pkg-config ${lib.optionalString staticOnlySupport "--static"} --cflags --libs mustache)"
      pkg_config_consumer_flags=()
      ${lib.optionalString staticOnlySupport ''
        pkg_config_consumer_flags+=(
          -DMUSTACHE_EXPECT_STATIC_DEFINE
        )
      ''}
      ${lib.optionalString sanitizerSupport ''
        pkg_config_consumer_flags+=(
          -fno-omit-frame-pointer
          -fsanitize=address,undefined
          -fno-sanitize-recover=all
        )
      ''}
      ${lib.optionalString threadSanitizerSupport ''
        pkg_config_consumer_flags+=(
          -fno-omit-frame-pointer
          -fsanitize=thread
        )
      ''}
      "$CXX" "$src/tests/cmake-consumer/main.cpp" \
        -std=c++11 \
        -DMUSTACHE_EXPECTED_VERSION='"${finalAttrs.version}"' \
        "''${pkg_config_flags[@]}" \
        "''${pkg_config_consumer_flags[@]}" \
        -o pkg-config-consumer
      LD_LIBRARY_PATH="$lib/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        ./pkg-config-consumer

      ${lib.optionalString (cmakeSupport || staticOnlySupport) ''
        read -r -a pkg_config_static_flags <<< "$(pkg-config --static --cflags --libs mustache)"
        pkg_config_forced_static_flags=()
        for flag in "''${pkg_config_static_flags[@]}"; do
          if test "$flag" = -lmustache; then
            pkg_config_forced_static_flags+=("$lib/lib/libmustache.a")
          else
            pkg_config_forced_static_flags+=("$flag")
          fi
        done
        "$CXX" "$src/tests/cmake-consumer/main.cpp" \
          -std=c++11 \
          -DMUSTACHE_EXPECTED_VERSION='"${finalAttrs.version}"' \
          "''${pkg_config_forced_static_flags[@]}" \
          "''${pkg_config_consumer_flags[@]}" \
          -o pkg-config-static-consumer
        ./pkg-config-static-consumer
      ''}
    ''
    + lib.optionalString cmakeSupport ''
      consumer_cmake_flags=()
      subproject_cmake_flags=()
      ${lib.optionalString sanitizerSupport ''
        consumer_cmake_flags+=(
          '-DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all'
          '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all'
        )
        subproject_cmake_flags+=(
          -DMUSTACHE_ENABLE_SANITIZERS=ON
        )
      ''}
      ${lib.optionalString threadSanitizerSupport ''
        consumer_cmake_flags+=(
          '-DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer -fsanitize=thread'
          '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread'
        )
        subproject_cmake_flags+=(
          -DMUSTACHE_ENABLE_THREAD_SANITIZER=ON
        )
      ''}
      cmake -S "$src/tests/cmake-consumer" -B consumer-shared \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$dev;$lib" \
        "''${consumer_cmake_flags[@]}"
      cmake --build consumer-shared --parallel${lib.optionalString threadSanitizerSupport " 1"}
      consumer-shared/mustache_consumer

      cmake -S "$src/tests/cmake-consumer" -B consumer-static \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$dev;$lib" \
        -DMUSTACHE_CONSUMER_LINKAGE=static \
        "''${consumer_cmake_flags[@]}"
      cmake --build consumer-static --parallel${lib.optionalString threadSanitizerSupport " 1"}
      consumer-static/mustache_consumer

      cmake -S "$src/tests/cmake-find-quiet" -B find-quiet \
        -DCMAKE_PREFIX_PATH="$dev;$lib" \
        -DMUSTACHE_EXPECT_STATIC_WITHOUT_DEPENDENCIES=${
        if libyaml == null
        then "ON"
        else "OFF"
      }

      cmake -S "$src/tests/cmake-subproject" -B subproject-build \
        -DCMAKE_BUILD_TYPE=Release \
        "''${subproject_cmake_flags[@]}" \
        "''${consumer_cmake_flags[@]}"
      cmake --build subproject-build --parallel${lib.optionalString threadSanitizerSupport " 1"}
      subproject-build/mustache_subproject_consumer
    ''
    + ''
      runHook postInstallCheck
    '';

  # Explicitly disabled adapters and the bundled Autotools Cista selection
  # must ignore caller/config-site overrides. Poison these variables so checks
  # enforce that contract.
  YAML_CFLAGS = lib.optionalString (libyaml == null) "-I/definitely-missing-libmustache-yaml-headers";
  YAML_LIBS = lib.optionalString (libyaml == null) "-lmustache-disabled-yaml-must-not-link";
  CISTA_CFLAGS =
    lib.optionalString (!cmakeSupport && archivedTemplateSupport != false && !useSystemCista)
    "-include /definitely-missing-libmustache-system-cista-header";
  XXHASH_CFLAGS =
    lib.optionalString (!cmakeSupport && archivedTemplateSupport != false && !useSystemXxhash)
    "-I/definitely-missing-libmustache-system-xxhash-headers";
  XXHASH_LIBS =
    lib.optionalString (!cmakeSupport && archivedTemplateSupport != false && !useSystemXxhash)
    "-lmustache-vendored-xxhash-must-not-link";

  meta = {
    description = "C++ implementation of Mustache templates";
    homepage = "https://github.com/jbboehr/libmustache";
    license = lib.licenses.mit;
    mainProgram = "mustachec";
    platforms = lib.platforms.linux;
  };
})
