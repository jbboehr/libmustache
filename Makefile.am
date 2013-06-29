
EXTRA_DIST = Readme.md
SUBDIRS = doc src test

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mustache.pc

ACLOCAL_AMFLAGS = -I m4

if GCC
# These flags are gcc specific
export AM_CFLAGS = -Wall -Wextra -Wdeclaration-after-statement
endif

test: all check

coverage: test