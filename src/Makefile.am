
EXTRA_DIST = CMakeLists.txt stack.cpp
CLEANFILES = mustache_config.h

# include_HEADERS = mustache.hpp mustache_config.h
mustacheincludedir = $(includedir)/mustache
mustacheinclude_HEADERS = \
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

noinst_LTLIBRARIES = libmustache_json_adapter.la
libmustache_json_adapter_la_SOURCES = json_parser.cpp
libmustache_json_adapter_la_CXXFLAGS = \
	@MUSTACHE_CXXFLAGS@ \
	@MUSTACHE_JSON_CXXFLAGS@
libmustache_json_adapter_la_CPPFLAGS = \
	@MUSTACHE_CPPFLAGS@ \
	-DMUSTACHE_STATIC_DEFINE \
	$(NLOHMANN_JSON_CFLAGS)

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
	renderer.cpp \
	renderer.hpp \
	stack.hpp \
	tokenizer.cpp \
	tokenizer.hpp \
	utils.cpp \
	utils.hpp

libmustache_la_LIBADD = libmustache_json_adapter.la $(YAML_LIBS)

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
