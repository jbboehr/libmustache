{
  lib,
  stdenvNoCC,
  cmake,
  llvmPackages,
  nlohmann_json,
  makeFontsConf,
  wineWow64Packages,
  writeText,
  sdk,
  source,
  linkage,
}: let
  wine = wineWow64Packages.stable;
  llvm = llvmPackages.llvm;
  linkerFlags = "/libpath:${sdk}/crt/lib/x64 /libpath:${sdk}/sdk/lib/ucrt/x64 /libpath:${sdk}/sdk/lib/um/x64";
  toolchain = writeText "xwin-toolchain.cmake" ''
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_SYSTEM_PROCESSOR AMD64)
    set(CMAKE_CXX_COMPILER ${llvmPackages.clang-unwrapped}/bin/clang-cl)
    set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
    set(CMAKE_LINKER ${llvmPackages.lld}/bin/lld-link)
    set(CMAKE_AR ${llvm}/bin/llvm-lib)
    set(CMAKE_MT ${llvm}/bin/llvm-mt)
    set(CMAKE_RC_COMPILER ${llvm}/bin/llvm-rc)
    # SOURCE_DATE_EPOCH does not control clang-cl's COFF object timestamps.
    set(CMAKE_CXX_FLAGS_INIT "/Brepro /vctoolsdir ${sdk}/crt /winsdkdir ${sdk}/sdk")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${linkerFlags}")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "${linkerFlags}")
    # PHP uses the DLL runtime, including when libmustache itself is static.
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${wine}/bin/wine)
  '';
in
  stdenvNoCC.mkDerivation {
    pname = "libmustache-windows-x64-xwin-${linkage}";
    version = (builtins.fromJSON (builtins.readFile ../vcpkg.json)).version-string;
    src = source;
    nativeBuildInputs = [cmake llvmPackages.clang-unwrapped llvm llvmPackages.lld wine];
    strictDeps = true;
    enableParallelBuilding = true;

    # Also inherited by the CMake consumer projects created by the test suite.
    CMAKE_TOOLCHAIN_FILE = toolchain;
    configurePhase = ''
      runHook preConfigure
      # CMake runs the archive compatibility witness during configuration too.
      export WINEPREFIX="$TMPDIR/wine-prefix" WINEDEBUG=-all
      export XDG_CACHE_HOME="$TMPDIR/cache"
      export FONTCONFIG_FILE=${makeFontsConf {fontDirectories = [];}}
      mkdir -p "$WINEPREFIX" "$XDG_CACHE_HOME/fontconfig"
      export WINEDLLOVERRIDES='mscoree,mshtml,winemenubuilder,winex11.drv,winewayland.drv='
      unset DISPLAY WAYLAND_DISPLAY
      trap 'wineserver -k' EXIT
      wineserver -p
      timeout 60 wineboot -i
      cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$out" \
        -DCMAKE_INSTALL_BINDIR=bin -DCMAKE_INSTALL_INCLUDEDIR=include -DCMAKE_INSTALL_LIBDIR=lib \
        -DMUSTACHE_WARNINGS_AS_ERRORS=ON -DMUSTACHE_ENABLE_TESTS=ON \
        -DMUSTACHE_ENABLE_ARCHIVED_TEMPLATES=ON -DMUSTACHE_ENABLE_JSON=ON \
        -DMUSTACHE_ENABLE_YAML=OFF \
        -DMUSTACHE_CLI_STATIC=${lib.boolToString (linkage == "static")} \
        -Dnlohmann_json_DIR=${nlohmann_json}/share/cmake/nlohmann_json
      cd build
      runHook postConfigure
    '';
    doCheck = true;
    checkPhase = ''
      runHook preCheck
      ctest --output-on-failure --parallel 2 --timeout 120
      runHook postCheck
    '';
    doInstallCheck = true;
    installCheckPhase = ''
      runHook preInstallCheck
      cmake -S "$src/tests/cmake-consumer" -B consumer \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$out" \
        -DMUSTACHE_CONSUMER_LINKAGE=${linkage}
      cmake --build consumer --parallel "$NIX_BUILD_CORES"
      cp "$out/bin/mustache.dll" consumer/
      wine consumer/mustache_consumer.exe
      wine "$out/bin/mustachec.exe" -v
      printf 'Hello, {{name}}!' > example.mustache
      printf '{"name":"PHP"}' > example.json
      wine "$out/bin/mustachec.exe" -t example.mustache -d example.json > actual.txt
      printf 'Hello, PHP!' > expected.txt
      diff -u expected.txt actual.txt
      runHook postInstallCheck
    '';
    dontFixup = true;
    meta.platforms = ["x86_64-linux"];
  }
