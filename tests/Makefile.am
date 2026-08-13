
check_PROGRAMS = test_spec test_utils test_misc

test_spec_SOURCES = test_spec.cpp test_spec.hpp spec_expectations.hpp ./fixtures/lambdas.cpp ./fixtures/lambdas.hpp
test_utils_SOURCES = test_utils.cpp
test_misc_SOURCES = test_misc.cpp

TESTS = test_spec test_utils test_misc
TESTS_ENVIRONMENT = \
	top_srcdir=$(top_srcdir) \
	top_builddir=$(top_builddir) \
	mustache_spec_dir=@MUSTACHE_SPEC_DIR@

clean-local:
	rm -rf logs

AM_CXXFLAGS = @MUSTACHE_CXXFLAGS@
AM_CPPFLAGS = @MUSTACHE_CPPFLAGS@ -I$(top_builddir)/src -I$(top_srcdir)/src $(JSON_CFLAGS) $(YAML_CFLAGS)
AM_LDFLAGS = @MUSTACHE_LDFLAGS@
LDADD = $(top_builddir)/src/libmustache.la $(JSON_LIBS) $(YAML_LIBS)
