
# EXTRA_DIST = mustache.def
CLEANFILES = mustache_config.h

# include_HEADERS = mustache.hpp mustache_config.h
mustacheincludedir = $(includedir)/mustache
mustacheinclude_HEADERS = \
	lambda.hpp \
	data.hpp \
	exception.hpp \
	mustache.hpp \
	node.hpp \
	renderer.hpp \
	stack.hpp \
	tokenizer.hpp \
	utils.hpp \
	mustache_config.h

lib_LTLIBRARIES = libmustache.la
libmustache_la_SOURCES = \
	lambda.cpp \
	lambda.hpp \
	data.cpp \
	data.hpp \
	exception.hpp \
	mustache.cpp \
	mustache.hpp \
	node.cpp \
	node.hpp \
	renderer.cpp \
	renderer.hpp \
	stack.hpp \
	tokenizer.cpp \
	tokenizer.hpp \
	utils.cpp \
	utils.hpp \
	mustache_config.h

libmustache_la_LIBADD = $(JSON_LIBS) $(YAML_LIBS)

# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
libmustache_la_LDFLAGS = \
	-no-undefined \
	-version-info 5:0:0
