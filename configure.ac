AC_PREREQ([2.69])
AC_INIT([mustache], [0.5.0], [jbboehr@gmail.com], [mustache], [https://github.com/jbboehr/libmustache])
AC_CONFIG_SRCDIR([src/mustache.cpp])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AX_PREFIX_CONFIG_H([src/mustache_config.h])

AM_INIT_AUTOMAKE([1.14 foreign subdir-objects parallel-tests])
AM_SILENT_RULES([yes])

AC_CANONICAL_HOST
AC_PROG_CXX
AC_LANG([C++])
LT_INIT
PKG_PROG_PKG_CONFIG
AX_CXX_COMPILE_STDCXX_11

MUSTACHE_CXXFLAGS=
MUSTACHE_CPPFLAGS=
MUSTACHE_LDFLAGS=

AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug], [enable debug mode])],
    [], [enable_debug=no])
AS_IF([test "x$enable_debug" = "xyes"], [
    AC_DEFINE([DEBUG], [1], [Defined if debug functions are requested])
])

AC_ARG_ENABLE([hardening],
    [AS_HELP_STRING([--disable-hardening], [disable compiler and linker hardening])],
    [], [enable_hardening=yes])
AS_IF([test "x$GXX" = "xyes"], [
    MUSTACHE_CXXFLAGS="$MUSTACHE_CXXFLAGS -Wall -Wextra -Wpedantic"
    AS_IF([test "x$enable_hardening" = "xyes"], [
        MUSTACHE_CXXFLAGS="$MUSTACHE_CXXFLAGS -fstack-protector-strong"
        AS_CASE([$host_os], [linux*], [
            AS_CASE([" $CPPFLAGS $CXXFLAGS "], [*_FORTIFY_SOURCE*], [], [
                MUSTACHE_CPPFLAGS="$MUSTACHE_CPPFLAGS -D_FORTIFY_SOURCE=3"
            ])
            AS_CASE([" $CPPFLAGS $CXXFLAGS "], [*_GLIBCXX_ASSERTIONS*], [], [
                MUSTACHE_CPPFLAGS="$MUSTACHE_CPPFLAGS -D_GLIBCXX_ASSERTIONS=1"
            ])
            MUSTACHE_LDFLAGS="$MUSTACHE_LDFLAGS -Wl,-z,relro -Wl,-z,now"
        ])
    ])
])

AC_ARG_ENABLE([sanitizers],
    [AS_HELP_STRING([--enable-sanitizers], [enable AddressSanitizer and UndefinedBehaviorSanitizer])],
    [], [enable_sanitizers=no])
AS_IF([test "x$enable_sanitizers" = "xyes"], [
    AS_IF([test "x$GXX" != "xyes"], [
        AC_MSG_ERROR([--enable-sanitizers requires a GCC-compatible C++ compiler])
    ])
    MUSTACHE_CXXFLAGS="$MUSTACHE_CXXFLAGS -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all"
    MUSTACHE_LDFLAGS="$MUSTACHE_LDFLAGS -fsanitize=address,undefined -fno-sanitize-recover=all"
])

AC_ARG_ENABLE([gcov],
    [AS_HELP_STRING([--enable-gcov], [enable gcov coverage instrumentation])],
    [], [enable_gcov=no])
AS_IF([test "x$enable_gcov" = "xyes"], [
    MUSTACHE_CXXFLAGS="$MUSTACHE_CXXFLAGS --coverage"
    MUSTACHE_LDFLAGS="$MUSTACHE_LDFLAGS --coverage"
])

AC_ARG_ENABLE([gprof],
    [AS_HELP_STRING([--enable-gprof], [enable gprof instrumentation])],
    [], [enable_gprof=no])
AS_IF([test "x$enable_gprof" = "xyes"], [
    MUSTACHE_CXXFLAGS="$MUSTACHE_CXXFLAGS -pg"
    MUSTACHE_LDFLAGS="$MUSTACHE_LDFLAGS -pg"
])

AC_ARG_WITH([mustache-spec],
    [AS_HELP_STRING([--with-mustache-spec=SPEC_LOCATION],
        [location of the mustache specification @<:@default=./spec/specs@>:@])])
AS_IF([test "x$with_mustache_spec" != "xno"], [
    AS_IF([test -n "$with_mustache_spec"], [
        AS_IF([test -f "$with_mustache_spec/comments.yml"], [
            AC_SUBST([MUSTACHE_SPEC_DIR], [$with_mustache_spec])
        ], [
            AC_MSG_FAILURE([mustache spec missing (--without-mustache-spec to disable)])
        ])
    ], [
        AC_SUBST([MUSTACHE_SPEC_DIR], ['$(top_srcdir)/spec/specs'])
    ])
])

PKG_CHECK_MODULES([JSON], [json-c], [
    AC_DEFINE([HAVE_LIBJSON], [1], [Defined if json-c is enabled])
], [
    AC_MSG_ERROR([json-c is required])
])
PKG_CHECK_MODULES([YAML], [yaml-0.1], [
    AC_DEFINE([HAVE_LIBYAML], [1], [Defined if libyaml is enabled])
], [
    AC_MSG_ERROR([libyaml is required])
])

AC_CHECK_HEADERS([cstdlib dirent.h exception fstream iostream list map memory sstream stack stdexcept string utility vector yaml.h])
AC_TYPE_INT32_T
AC_TYPE_LONG_LONG_INT
AC_CXX_STL_HASH

AC_SUBST([MUSTACHE_CXXFLAGS])
AC_SUBST([MUSTACHE_CPPFLAGS])
AC_SUBST([MUSTACHE_LDFLAGS])

AM_CONDITIONAL([GCOV], [test "x$enable_gcov" = "xyes"])
AM_CONDITIONAL([GPROF], [test "x$enable_gprof" = "xyes"])

AC_CONFIG_FILES([
    mustache.pc
    Makefile
    src/Makefile
    bin/Makefile
    tests/Makefile
])
AC_OUTPUT
