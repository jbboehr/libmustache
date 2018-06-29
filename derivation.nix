{ stdenv, fetchurl, pkgconfig, glib, autoconf, automake, json_c, 
  libyaml, libtool, m4, mustache_spec, autoreconfHook, libstdcxx5 }:

stdenv.mkDerivation rec {
  name = "libmustache-0.4.4";

  src = fetchurl {
    url = https://github.com/jbboehr/libmustache/archive/v0.4.4.tar.gz;
    sha256 = "05089w08n24jx7hgs6h1j50jh53yavc7l9bgy02iygx14ajznb86";
  };

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
