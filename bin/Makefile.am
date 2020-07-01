
bin_PROGRAMS = mustachec

mustachec_SOURCES = mustache.cpp

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src
AM_LDFLAGS = -static  # for speed and Valgrind
LDADD = $(top_builddir)/src/libmustache.la
