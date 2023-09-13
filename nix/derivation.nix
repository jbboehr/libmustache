{
  lib,
  stdenv,
  fetchurl,
  pkg-config,
  libtool,
  m4,
  autoconf,
  automake,
  autoreconfHook,
  cmake ? null,
  glib,
  json_c,
  libyaml,
  libstdcxx5,
  mustache_spec,
  gitignoreFilterWith,
  libmustacheSrc ? ../.,
  checkSupport ? true,
  cmakeSupport ? false,
  debugSupport ? false,
}:
stdenv.mkDerivation rec {
  name = "libmustache";

  src = lib.cleanSourceWith {
    name = "libmustache-source";
    src = libmustacheSrc;
    filter = gitignoreFilterWith {
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

  outputs = ["out" "dev" "lib"];

  enableParallelBuilding = true;
  buildInputs = [glib json_c libyaml stdenv.cc.cc.lib];
  nativeBuildInputs =
    [mustache_spec]
    ++ lib.optionals cmakeSupport [cmake]
    ++ lib.optionals (!cmakeSupport) [autoreconfHook libtool m4 autoconf automake];
  propagatedNativeBuildInputs = [pkg-config];

  doCheck = checkSupport;
  configureFlags =
    ["--libdir=$(lib)/lib" "--includedir=$(dev)/include"]
    ++ lib.optional checkSupport "--with-mustache-spec=${mustache_spec}/share/mustache-spec/specs";

  cmakeFlags =
    []
    ++ lib.optional debugSupport "-DCMAKE_BUILD_TYPE=Debug"
    ++ lib.optional (!debugSupport) "-DCMAKE_BUILD_TYPE=Release"
    ++ lib.optionals checkSupport ["-DMUSTACHE_ENABLE_TESTS=1" "-DMUSTACHE_SPEC_DIR=${mustache_spec}/share/mustache-spec/specs"];

  postInstall = ''
    patchelf --set-rpath "${lib.makeLibraryPath buildInputs}:$out/lib" $out/bin/mustachec
  '';

  meta = with lib; {
    description = "C++ implementation of mustache";
    homepage = https://github.com/jbboehr/libmustache;
    license = "MIT";
    maintainers = ["John Boehr <jbboehr@gmail.com>"];
  };
}
