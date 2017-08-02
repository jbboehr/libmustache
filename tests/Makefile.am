
check_PROGRAMS = test_spec test_utils

test_spec_SOURCES = test_spec.cpp test_spec.hpp ./fixtures/lambdas.cpp ./fixtures/lambdas.hpp
test_utils_SOURCES = test_utils.cpp

TESTS = test_spec test_utils
TESTS_ENVIRONMENT = \
	top_srcdir=$(top_srcdir) \
	top_builddir=$(top_builddir) \
	mustache_spec_dir=@MUSTACHE_SPEC_DIR@

clean-local:
	rm -rf logs

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src
AM_LDFLAGS = -static  # for speed and Valgrind
LDADD = $(top_builddir)/src/libmustache.la
