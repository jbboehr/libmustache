
SUBDIRS = spec
EXTRA_DIST = run

TESTS = spec/test_spec
TESTS_ENVIRONMENT = \
	top_srcdir=$(top_srcdir) \
	top_builddir=$(top_builddir) \
	mustache_spec_dir=$(top_srcdir)/spec/specs

clean-local:
	rm -rf logs
