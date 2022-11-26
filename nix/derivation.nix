{
  lib, stdenv, fetchurl, pkgconfig,
  libtool, m4, autoconf, automake, autoreconfHook,
  cmake ? null,
  glib, json_c, libyaml, libstdcxx5,
  mustache_spec, 
  libmustacheVersion ? null,
  libmustacheSrc ? null,
  libmustacheSha256 ? null,

  checkSupport ? true,
  cmakeSupport ? false,
  debugSupport ? false
}:

let 
  orDefault = x: y: (if (!isNull x) then x else y);
in

stdenv.mkDerivation rec {
  name = "libmustache-${version}";
  version = orDefault libmustacheVersion "v0.5.0";
  src = orDefault libmustacheSrc (fetchurl {
    url = "https://github.com/jbboehr/libmustache/archive/${version}.tar.gz";
    sha256 = orDefault libmustacheSha256 "1afa6654dz7j6hsnhyzdciy4rjccdif6fjivhxxrjwbm8pxxx4gz";
  });

  outputs = [ "out" "dev" "lib" ];

  enableParallelBuilding = true;
  buildInputs = [ glib json_c libyaml stdenv.cc.cc.lib ];
  nativeBuildInputs = [ mustache_spec ]
    ++ lib.optionals cmakeSupport [ cmake ]
    ++ lib.optionals (!cmakeSupport) [ autoreconfHook libtool m4 autoconf automake ]
    ;
  propagatedNativeBuildInputs = [ pkgconfig ];

  doCheck = checkSupport;
  configureFlags = ["--libdir=$(lib)/lib" "--includedir=$(dev)/include"]
    ++ lib.optional  checkSupport "--with-mustache-spec=${mustache_spec}/share/mustache-spec/specs"
    ;

  cmakeFlags = []
    ++ lib.optional  debugSupport "-DCMAKE_BUILD_TYPE=Debug"
    ++ lib.optional  (!debugSupport) "-DCMAKE_BUILD_TYPE=Release"
    ++ lib.optionals checkSupport ["-DMUSTACHE_ENABLE_TESTS=1" "-DMUSTACHE_SPEC_DIR=${mustache_spec}/share/mustache-spec/specs"]
    ;

  postInstall = ''
      patchelf --set-rpath "${lib.makeLibraryPath buildInputs}:$out/lib" $out/bin/mustachec
    '';

  meta = with lib; {
    description = "C++ implementation of mustache";
    homepage = https://github.com/jbboehr/libmustache;
    license = "MIT";
    maintainers = [ "John Boehr <jbboehr@gmail.com>" ];
  };
}
