
bin_PROGRAMS = mustachec

EXTRA_DIST = CMakeLists.txt

mustachec_SOURCES = mustache.cpp cli.cpp cli.hpp

AM_CXXFLAGS = @MUSTACHE_CXXFLAGS@
AM_CPPFLAGS = \
	@MUSTACHE_CPPFLAGS@ \
	@MUSTACHE_CONSUMER_EXPORT_CPPFLAGS@ \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src
AM_LDFLAGS = @MUSTACHE_LDFLAGS@
LDADD = $(top_builddir)/src/libmustache.la
