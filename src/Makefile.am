
# EXTRA_DIST = mustache.def

include_HEADERS = mustache.hpp mustache_config.h

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
	utils.hpp
libmustache_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex '^mustache' \
	-version-info 0:0:0 # https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
