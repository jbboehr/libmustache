{
  pkgs,
  makePackage,
}: {
  libmustache-static-only = makePackage {
    cmakeSupport = false;
    staticOnlySupport = true;
  };
  libmustache-sanitized = makePackage {
    cmakeSupport = true;
    debugSupport = true;
    sanitizerSupport = true;
  };
  libmustache-thread-sanitized = makePackage {
    stdenv = pkgs.llvmPackages.stdenv;
    cmakeSupport = true;
    debugSupport = true;
    threadSanitizerSupport = true;
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
    zlib = null;
    archivedTemplateSupport = false;
  };
  libmustache-cmake-no-yaml = makePackage {
    cmakeSupport = true;
    libyaml = null;
  };
  libmustache-cmake-minimal = makePackage {
    cmakeSupport = true;
    nlohmann_json = null;
    libyaml = null;
    zlib = null;
    archivedTemplateSupport = false;
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
    archivedTemplateSupport = false;
    cistaBenchmarkSupport = true;
    cistaBuiltinXxh3Support = true;
  };
  libmustache-archived-no-zlib-cmake = makePackage {
    cmakeSupport = true;
    cista = null;
    xxhash = null;
    zlib = null;
    archivedTemplateSupport = true;
  };
  libmustache-archived-no-zlib-autotools = makePackage {
    cmakeSupport = false;
    cista = null;
    xxhash = null;
    zlib = null;
    archivedTemplateSupport = true;
  };
  libmustache-archived-system-autotools = makePackage {
    cmakeSupport = false;
    cista = pkgs.cista;
    xxhash = pkgs.xxhash;
    zlib = null;
    archivedTemplateSupport = null;
    useSystemCista = true;
    useSystemXxhash = true;
  };
  libmustache-archived-system-cista-vendored-xxhash-cmake = makePackage {
    cmakeSupport = true;
    cista = pkgs.cista;
    xxhash = null;
    zlib = null;
    archivedTemplateSupport = true;
    useSystemCista = true;
  };
  libmustache-archived-vendored-cista-system-xxhash-cmake = makePackage {
    cmakeSupport = true;
    cista = null;
    xxhash = pkgs.xxhash;
    zlib = null;
    archivedTemplateSupport = true;
    useSystemXxhash = true;
  };
  libmustache-archived-system-cista-vendored-xxhash-autotools = makePackage {
    cmakeSupport = false;
    cista = pkgs.cista;
    xxhash = null;
    zlib = null;
    archivedTemplateSupport = true;
    useSystemCista = true;
  };
  libmustache-archived-vendored-cista-system-xxhash-autotools = makePackage {
    cmakeSupport = false;
    cista = null;
    xxhash = pkgs.xxhash;
    zlib = null;
    archivedTemplateSupport = true;
    useSystemXxhash = true;
  };
}
