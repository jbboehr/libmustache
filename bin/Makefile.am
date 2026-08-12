
bin_PROGRAMS = mustachec

mustachec_SOURCES = mustache.cpp

AM_CXXFLAGS = @MUSTACHE_CXXFLAGS@
AM_CPPFLAGS = @MUSTACHE_CPPFLAGS@ -I$(top_builddir)/src -I$(top_srcdir)/src
AM_LDFLAGS = @MUSTACHE_LDFLAGS@
LDADD = $(top_builddir)/src/libmustache.la
