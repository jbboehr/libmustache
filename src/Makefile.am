
EXTRA_DIST = CMakeLists.txt stack.cpp
CLEANFILES = mustache_config.h

# include_HEADERS = mustache.hpp mustache_config.h
mustacheincludedir = $(includedir)/mustache
mustacheinclude_HEADERS = \
	archived_template.hpp \
	compiled_template.hpp \
	lambda.hpp \
	data.hpp \
	exception.hpp \
	mustache.hpp \
	mustache_export.hpp \
	node.hpp \
	renderer.hpp \
	stack.hpp \
	tokenizer.hpp \
	utils.hpp

nodist_mustacheinclude_HEADERS = mustache_config.h

noinst_LTLIBRARIES =
if MUSTACHE_JSON_ENABLED
noinst_LTLIBRARIES += libmustache_json_adapter.la
libmustache_json_adapter_la_SOURCES = json_parser.cpp
libmustache_json_adapter_la_CXXFLAGS = \
	@MUSTACHE_CXXFLAGS@ \
	@MUSTACHE_JSON_CXXFLAGS@
libmustache_json_adapter_la_CPPFLAGS = \
	@MUSTACHE_CPPFLAGS@ \
	-DMUSTACHE_STATIC_DEFINE \
	$(NLOHMANN_JSON_CFLAGS)
endif

if MUSTACHE_ARCHIVED_TEMPLATES_ENABLED
noinst_LTLIBRARIES += libmustache_archive_adapter.la
libmustache_archive_adapter_la_SOURCES = \
	archive/cista.cpp \
	archive/xxh3.cpp \
	archived_template.hpp
libmustache_archive_adapter_la_CXXFLAGS = \
	@MUSTACHE_CXXFLAGS@ \
	@MUSTACHE_ARCHIVE_CXXFLAGS@ \
	-UCISTA_FMT
libmustache_archive_adapter_la_CPPFLAGS = \
	@MUSTACHE_CPPFLAGS@ \
	@MUSTACHE_LIBRARY_EXPORT_CPPFLAGS@ \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/archive/xxh3 \
	-DCISTA_XXH3=1 \
	-DMUSTACHE_CISTA_ARCHIVE_PRODUCTION_ONLY=1 \
	-DMUSTACHE_CISTA_RUNTIME_VERSION_XXH3=1
if MUSTACHE_SYSTEM_XXHASH
libmustache_archive_adapter_la_CPPFLAGS += $(XXHASH_CFLAGS)
else
libmustache_archive_adapter_la_CPPFLAGS += \
	-isystem$(top_srcdir)/vendor/xxhash \
	-DXXH_IMPLEMENTATION=1 \
	-DXXH_STATIC_LINKING_ONLY=1 \
	-DXXH_NAMESPACE=MUSTACHE_CISTA_XXH_
endif
if MUSTACHE_SYSTEM_CISTA
libmustache_archive_adapter_la_CPPFLAGS += \
	$(CISTA_CFLAGS) \
	-UMUSTACHE_USE_VENDORED_CISTA
else
libmustache_archive_adapter_la_CPPFLAGS += \
	-I$(top_srcdir)/vendor/cista \
	-DMUSTACHE_USE_VENDORED_CISTA=1
endif
endif

lib_LTLIBRARIES = libmustache.la
libmustache_la_SOURCES = \
	compiled_template.cpp \
	compiled_template.hpp \
	lambda.cpp \
	lambda.hpp \
	data.cpp \
	data.hpp \
	exception.hpp \
	mustache.cpp \
	mustache.hpp \
	mustache_export.hpp \
	node.cpp \
	node.hpp \
	render_engine.hpp \
	renderer.cpp \
	renderer.hpp \
	stack.hpp \
	tokenizer.cpp \
	tokenizer.hpp \
	utils.cpp \
	utils.hpp

libmustache_la_LIBADD = $(YAML_LIBS)
if MUSTACHE_JSON_ENABLED
libmustache_la_LIBADD += libmustache_json_adapter.la
endif
if MUSTACHE_ARCHIVED_TEMPLATES_ENABLED
libmustache_la_LIBADD += libmustache_archive_adapter.la
if MUSTACHE_SYSTEM_XXHASH
libmustache_la_LIBADD += $(XXHASH_LIBS)
endif
endif

# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
libmustache_la_LDFLAGS = \
	-no-undefined \
	-version-info 6:0:0

AM_CXXFLAGS = @MUSTACHE_CXXFLAGS@
AM_CPPFLAGS = \
	@MUSTACHE_CPPFLAGS@ \
	@MUSTACHE_LIBRARY_EXPORT_CPPFLAGS@ \
	$(YAML_CFLAGS)
AM_LDFLAGS = @MUSTACHE_LDFLAGS@
