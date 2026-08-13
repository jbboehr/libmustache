
check_PROGRAMS = test_spec test_utils test_misc test_data_behavior test_node_serialization test_ownership test_length_aware_api test_tokenizer_safety

test_spec_SOURCES = test_spec.cpp test_spec.hpp spec_expectations.hpp ./fixtures/lambdas.cpp ./fixtures/lambdas.hpp
test_utils_SOURCES = test_utils.cpp
test_misc_SOURCES = test_misc.cpp
test_data_behavior_SOURCES = test_data_behavior.cpp
test_node_serialization_SOURCES = test_node_serialization.cpp
test_ownership_SOURCES = test_ownership.cpp
test_length_aware_api_SOURCES = test_length_aware_api.cpp
test_tokenizer_safety_SOURCES = test_tokenizer_safety.cpp

EXTRA_DIST = \
	fuzz_node_unserialize.cpp \
	fuzz/node_unserialize.dict \
	fuzz/corpus/node_unserialize/deeply-nested \
	fuzz/corpus/node_unserialize/oversized-child-region \
	fuzz/corpus/node_unserialize/php-variable \
	fuzz/corpus/node_unserialize/trailing-byte \
	fuzz/corpus/node_unserialize/truncated-header

TESTS = test_spec test_utils test_misc test_data_behavior test_node_serialization test_ownership test_length_aware_api test_tokenizer_safety
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
