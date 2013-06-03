
bin_PROGRAMS = test_spec

test_spec_SOURCES = test_spec.cpp test_spec.hpp

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src
LDFLAGS = -lyaml -static  # for speed and Valgrind
LDADD = $(top_builddir)/src/libmustache.la
