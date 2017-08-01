
check_PROGRAMS = test_spec

test_spec_SOURCES = test_spec.cpp test_spec.hpp ../fixtures/lambdas.cpp ../fixtures/lambdas.hpp

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src
AM_LDFLAGS = -static  # for speed and Valgrind
LDADD = $(top_builddir)/src/libmustache.la
