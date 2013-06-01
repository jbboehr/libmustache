
AC_PREREQ([2.60])
AC_INIT([mustache], [0.0.1-dev], [jbboehr@gmail.com])

AM_INIT_AUTOMAKE([1.10 foreign])

AC_CONFIG_SRCDIR([src/mustache.cpp])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AM_CONDITIONAL([GCC], [test x$GCC = xyes])

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_LONG_LONG_INT

# Checks for library functions.

AC_CONFIG_FILES([
        mustache.pc
        Makefile
        doc/Makefile
        src/Makefile
        src/mustache_config.h
        test/Makefile
        test/spec/Makefile
])
AC_OUTPUT
