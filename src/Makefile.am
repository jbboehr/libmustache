
# EXTRA_DIST = mustache.def

# include_HEADERS = mustache.hpp mustache_config.h
mustacheincludedir = $(includedir)/mustache
mustacheinclude_HEADERS = \
	data.hpp \
	exception.hpp \
	mustache.hpp \
	node.hpp \
	renderer.hpp \
	tokenizer.hpp \
	utils.hpp \
	compiler.hpp \
	vm.hpp

lib_LTLIBRARIES = libmustache.la
libmustache_la_SOURCES = \
	data.cpp \
	data.hpp \
	exception.hpp \
	mustache.cpp \
	mustache.hpp \
	node.cpp \
	node.hpp \
	renderer.cpp \
	renderer.hpp \
	tokenizer.cpp \
	tokenizer.hpp \
	utils.cpp \
	utils.hpp \
	compiler.cpp \
	compiler.hpp \
	vm.cpp \
	vm.hpp

# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
libmustache_la_LDFLAGS = \
	-no-undefined \
	-version-info 1:0:0
