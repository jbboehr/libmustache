{ stdenv, fetchurl, pkgconfig, glib, autoconf, automake, json_c, 
  libyaml, libtool, m4, mustache_spec, autoreconfHook, libstdcxx5,
  libmustacheVersion ? null,
  libmustacheSrc ? null,
  libmustacheSha256 ? null
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

  enableParallelBuilding = true;
  buildInputs = [ glib json_c libyaml stdenv.cc.cc.lib ];
  propagatedBuildInputs = [ pkgconfig ];
  nativeBuildInputs = [ autoreconfHook mustache_spec libtool m4 autoconf automake ];

  doCheck = true;
  configureFlags = [
    "--with-mustache-spec=${mustache_spec}/share/mustache-spec/specs"
  ];

  postInstall = ''
      patchelf --set-rpath "${stdenv.lib.makeLibraryPath buildInputs}:$out/lib" $out/bin/mustache
    '';

  meta = with stdenv.lib; {
    description = "C++ implementation of mustache";
    homepage = https://github.com/jbboehr/libmustache;
    license = "MIT";
    maintainers = [ "John Boehr <jbboehr@gmail.com>" ];
  };
}
