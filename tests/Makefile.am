
check_PROGRAMS = test_utils test_misc test_data_behavior test_node_serialization test_node_template test_ownership test_length_aware_api test_tokenizer_safety test_compiled_template test_renderer_safety test_allocation_failure test_cli

if MUSTACHE_JSON_ENABLED
check_PROGRAMS += test_json_numeric_locale
endif

if MUSTACHE_SPEC_TEST_ENABLED
check_PROGRAMS += test_spec
endif

if MUSTACHE_ARCHIVED_TEMPLATES_ENABLED
check_PROGRAMS += test_cista_archive test_archived_template
endif

test_spec_SOURCES = test_spec.cpp test_spec.hpp spec_expectations.hpp ./fixtures/lambdas.cpp ./fixtures/lambdas.hpp
test_utils_SOURCES = test_utils.cpp
test_misc_SOURCES = test_misc.cpp
test_data_behavior_SOURCES = test_data_behavior.cpp
test_json_numeric_locale_SOURCES = test_json_numeric_locale.cpp
test_node_serialization_SOURCES = test_node_serialization.cpp
test_node_template_SOURCES = test_node_template.cpp
test_ownership_SOURCES = test_ownership.cpp
test_length_aware_api_SOURCES = test_length_aware_api.cpp
test_tokenizer_safety_SOURCES = test_tokenizer_safety.cpp
test_compiled_template_SOURCES = test_compiled_template.cpp
test_renderer_safety_SOURCES = test_renderer_safety.cpp
test_allocation_failure_SOURCES = test_allocation_failure.cpp
# Replacement new/delete use malloc/free; GCC -Werror treats that pairing as
# mismatched-new-delete after inlining into std::make_unique.
test_allocation_failure_CXXFLAGS = $(AM_CXXFLAGS) -Wno-mismatched-new-delete
test_cli_SOURCES = test_cli.cpp ../bin/cli.cpp ../bin/cli.hpp
test_archived_template_SOURCES = test_archived_template.cpp
test_cista_archive_SOURCES = \
	../benchmarks/test-cista-archive.cpp \
	../benchmarks/cista-archive.cpp \
	../benchmarks/cista-archive.hpp \
	../benchmarks/cista-xxh3.cpp
test_cista_archive_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/benchmarks \
	-I$(top_srcdir)/benchmarks/cista-xxh3 \
	$(ZLIB_CFLAGS) \
	-DCISTA_XXH3=1 \
	-DMUSTACHE_CISTA_RUNTIME_VERSION_XXH3=1 \
	-DMUSTACHE_CISTA_ARCHIVE_PROTOTYPE_ONLY=1
if MUSTACHE_SYSTEM_XXHASH
test_cista_archive_CPPFLAGS += $(XXHASH_CFLAGS)
else
test_cista_archive_CPPFLAGS += \
	-isystem$(top_srcdir)/vendor/xxhash \
	-DXXH_IMPLEMENTATION=1 \
	-DXXH_STATIC_LINKING_ONLY=1 \
	-DXXH_NAMESPACE=MUSTACHE_CISTA_XXH_
endif
if MUSTACHE_SYSTEM_CISTA
test_cista_archive_CPPFLAGS += \
	$(CISTA_CFLAGS) \
	-UMUSTACHE_USE_VENDORED_CISTA
else
test_cista_archive_CPPFLAGS += \
	-I$(top_srcdir)/vendor/cista \
	-DMUSTACHE_USE_VENDORED_CISTA=1
endif
test_cista_archive_CXXFLAGS = $(AM_CXXFLAGS) -UCISTA_FMT
test_cista_archive_LDADD = \
	$(top_builddir)/src/libmustache.la \
	$(ZLIB_LIBS)
if MUSTACHE_SYSTEM_XXHASH
test_cista_archive_LDADD += $(XXHASH_LIBS)
endif

EXTRA_DIST = \
	CMakeLists.txt \
	check_installed_shared_consumer.cmake \
	run_fuzzer_smoke.cmake \
	run_spec_crlf.cmake \
	verify_fuzzer_smoke_safety.cmake \
	fixtures/cista-archive-v1-x86_64-le-itanium.hex \
	fuzz_node_unserialize.cpp \
	fuzz_tokenizer.cpp \
	fuzz_data_parser.cpp \
	fuzz_cista_archive.cpp \
	fuzz/node_unserialize.dict \
	fuzz/tokenizer.dict \
	fuzz/data_parser.dict \
	fuzz/cista_archive.dict \
	fuzz/corpus/cista_archive/template-semantics \
	fuzz/corpus/cista_archive/nested-node-limit \
	fuzz/corpus/cista_archive/truncated-preamble \
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

TESTS = test_utils test_misc test_data_behavior test_node_serialization test_node_template test_ownership test_length_aware_api test_tokenizer_safety test_compiled_template test_renderer_safety test_allocation_failure test_cli

if MUSTACHE_JSON_ENABLED
TESTS += test_json_numeric_locale
endif

if MUSTACHE_SPEC_TEST_ENABLED
TESTS += test_spec
endif
if MUSTACHE_ARCHIVED_TEMPLATES_ENABLED
TESTS += test_cista_archive test_archived_template
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
