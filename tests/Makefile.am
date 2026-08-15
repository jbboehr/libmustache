
check_PROGRAMS = test_utils test_misc test_data_behavior test_node_serialization test_node_template test_ownership test_length_aware_api test_tokenizer_safety test_compiled_template test_renderer_safety test_cli

if MUSTACHE_SPEC_ENABLED
check_PROGRAMS += test_spec
endif

test_spec_SOURCES = test_spec.cpp test_spec.hpp spec_expectations.hpp ./fixtures/lambdas.cpp ./fixtures/lambdas.hpp
test_utils_SOURCES = test_utils.cpp
test_misc_SOURCES = test_misc.cpp
test_data_behavior_SOURCES = test_data_behavior.cpp
test_node_serialization_SOURCES = test_node_serialization.cpp
test_node_template_SOURCES = test_node_template.cpp
test_ownership_SOURCES = test_ownership.cpp
test_length_aware_api_SOURCES = test_length_aware_api.cpp
test_tokenizer_safety_SOURCES = test_tokenizer_safety.cpp
test_compiled_template_SOURCES = test_compiled_template.cpp
test_renderer_safety_SOURCES = test_renderer_safety.cpp
test_cli_SOURCES = test_cli.cpp ../bin/cli.cpp ../bin/cli.hpp

EXTRA_DIST = \
	CMakeLists.txt \
	run_spec_crlf.cmake \
	fuzz_node_unserialize.cpp \
	fuzz_tokenizer.cpp \
	fuzz_data_parser.cpp \
	fuzz/node_unserialize.dict \
	fuzz/tokenizer.dict \
	fuzz/data_parser.dict \
	fuzz/corpus/data_parser/json-deep \
	fuzz/corpus/data_parser/json-duplicate-keys \
	fuzz/corpus/data_parser/json-floating-overflow \
	fuzz/corpus/data_parser/json-nul-key \
	fuzz/corpus/data_parser/json-object \
	fuzz/corpus/data_parser/json-trailing \
	fuzz/corpus/data_parser/yaml-alias \
	fuzz/corpus/data_parser/yaml-cycle \
	fuzz/corpus/data_parser/yaml-multiple-documents \
	fuzz/corpus/node_unserialize/deeply-nested \
	fuzz/corpus/node_unserialize/oversized-child-region \
	fuzz/corpus/node_unserialize/php-variable \
	fuzz/corpus/node_unserialize/trailing-byte \
	fuzz/corpus/node_unserialize/truncated-header \
	fuzz/corpus/tokenizer/deep-valid-section \
	fuzz/corpus/tokenizer/delimiter-change \
	fuzz/corpus/tokenizer/many-data-parts \
	fuzz/corpus/tokenizer/mismatched-section \
	fuzz/corpus/tokenizer/standalone-tags \
	fuzz/corpus/tokenizer/triple-mustache \
	fuzz/corpus/tokenizer/unclosed-tag \
	fuzz/corpus/tokenizer/valid-section

TESTS = test_utils test_misc test_data_behavior test_node_serialization test_node_template test_ownership test_length_aware_api test_tokenizer_safety test_compiled_template test_renderer_safety test_cli

if MUSTACHE_SPEC_ENABLED
TESTS += test_spec
endif
TESTS_ENVIRONMENT = \
	top_srcdir=$(top_srcdir) \
	top_builddir=$(top_builddir) \
	mustache_spec_dir=@MUSTACHE_SPEC_DIR@

clean-local:
	rm -rf logs

AM_CXXFLAGS = @MUSTACHE_CXXFLAGS@
AM_CPPFLAGS = \
	@MUSTACHE_CPPFLAGS@ \
	@MUSTACHE_CONSUMER_EXPORT_CPPFLAGS@ \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	$(YAML_CFLAGS)
test_cli_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/bin
AM_LDFLAGS = @MUSTACHE_LDFLAGS@
LDADD = $(top_builddir)/src/libmustache.la $(YAML_LIBS)
