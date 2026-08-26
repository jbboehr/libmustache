AC_PREREQ([2.69])
AC_INIT([mustache], [0.6.0], [jbboehr@gmail.com], [mustache], [https://github.com/jbboehr/libmustache])
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

MUSTACHE_CXXFLAGS=
MUSTACHE_CPPFLAGS=
MUSTACHE_LDFLAGS=
MUSTACHE_JSON_CXXFLAGS=
MUSTACHE_LIBRARY_EXPORT_CPPFLAGS=
MUSTACHE_CONSUMER_EXPORT_CPPFLAGS=

AS_IF([test "x$enable_shared" = "xno" &&
        test "x$enable_static" = "xyes"], [
    MUSTACHE_LIBRARY_EXPORT_CPPFLAGS=-DMUSTACHE_STATIC_DEFINE
    MUSTACHE_CONSUMER_EXPORT_CPPFLAGS=-DMUSTACHE_STATIC_DEFINE
], [
    MUSTACHE_LIBRARY_EXPORT_CPPFLAGS=-DMUSTACHE_BUILDING_LIBRARY
])
AC_SUBST([MUSTACHE_LIBRARY_EXPORT_CPPFLAGS])
AC_SUBST([MUSTACHE_CONSUMER_EXPORT_CPPFLAGS])

AC_CACHE_CHECK([for a $CXX option that enables C++17],
    [mustache_cv_cxx17_flag], [
    mustache_save_CXXFLAGS=$CXXFLAGS
    mustache_cv_cxx17_flag=unsupported
    mustache_cxx17_default=no
    CXXFLAGS=$mustache_save_CXXFLAGS
    AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
            #include <optional>
            #include <string_view>
            #include <utility>
        ]], [[
            std::optional<std::string_view> value("mustache");
            auto pair = std::make_pair(value->size(), true);
            auto [size, valid] = pair;
            return valid && size > 0 ? 0 : 1;
        ]])
    ], [mustache_cxx17_default=yes])
    for mustache_cxx17_flag in \
            -std=c++17 -std=c++1z -std=gnu++17 -std=gnu++1z; do
        CXXFLAGS="$mustache_save_CXXFLAGS $mustache_cxx17_flag"
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[
                #include <optional>
                #include <string_view>
                #include <utility>
            ]], [[
                std::optional<std::string_view> value("mustache");
                auto pair = std::make_pair(value->size(), true);
                auto [size, valid] = pair;
                return valid && size > 0 ? 0 : 1;
            ]])
        ], [
            mustache_cv_cxx17_flag=$mustache_cxx17_flag
            break
        ])
    done
    AS_IF([test "x$mustache_cv_cxx17_flag" = "xunsupported" &&
            test "x$mustache_cxx17_default" = "xyes"], [
        mustache_cv_cxx17_flag=none
    ])
    CXXFLAGS=$mustache_save_CXXFLAGS
])
AS_CASE([$mustache_cv_cxx17_flag],
    [unsupported], [AC_MSG_ERROR([a compiler with C++17 support is required])],
    [none], [MUSTACHE_CXX_STANDARD_FLAG=],
    [
        CXXFLAGS="$CXXFLAGS $mustache_cv_cxx17_flag"
        MUSTACHE_CXX_STANDARD_FLAG=$mustache_cv_cxx17_flag
    ])
AC_SUBST([MUSTACHE_CXX_STANDARD_FLAG])
AC_DEFINE([CXX_STANDARD], [17], [Required C++ standard version])
AC_DEFINE([HAVE_CXX17], [1], [Define if the compiler supports C++17])
AC_DEFINE([HAVE_CXX11], [1],
    [Deprecated compatibility alias; C++17 support includes C++11])

AC_CACHE_CHECK([whether $CXX supports private C++ symbol visibility],
    [mustache_cv_cxx_visibility_hidden], [
    mustache_save_CXXFLAGS=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS -Werror -fvisibility=hidden -fvisibility-inlines-hidden"
    AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
            template <typename T> class HiddenValue {
              public:
                T value;
            };
        ]], [[
            HiddenValue<int> value = {1};
            return value.value == 1 ? 0 : 1;
        ]])
    ], [mustache_cv_cxx_visibility_hidden=yes],
       [mustache_cv_cxx_visibility_hidden=no])
    CXXFLAGS=$mustache_save_CXXFLAGS
])
AS_IF([test "x$mustache_cv_cxx_visibility_hidden" = "xyes"], [
    MUSTACHE_JSON_CXXFLAGS="-fvisibility=hidden -fvisibility-inlines-hidden"
])
AC_SUBST([MUSTACHE_JSON_CXXFLAGS])

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
                AC_CACHE_CHECK([whether $CXX supports _FORTIFY_SOURCE=3],
                    [mustache_cv_cxx_fortify_source_3], [
                    mustache_save_CPPFLAGS=$CPPFLAGS
                    mustache_save_CXXFLAGS=$CXXFLAGS
                    CPPFLAGS="$CPPFLAGS -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3"
                    CXXFLAGS="$CXXFLAGS -Werror"
                    AC_COMPILE_IFELSE([
                        AC_LANG_PROGRAM([[
                            #if !defined(__OPTIMIZE__) || __OPTIMIZE__ <= 0
                            #error _FORTIFY_SOURCE requires optimization
                            #endif
                            #if !defined(_FORTIFY_SOURCE) || _FORTIFY_SOURCE != 3
                            #error _FORTIFY_SOURCE=3 was not selected
                            #endif
                            #include <cstring>
                        ]], [[
                            char source[] = "abc";
                            char destination[sizeof(source)];
                            std::memcpy(destination, source, sizeof(source));
                            return destination[0] == 'a' ? 0 : 1;
                        ]])
                    ], [mustache_cv_cxx_fortify_source_3=yes],
                       [mustache_cv_cxx_fortify_source_3=no])
                    CPPFLAGS=$mustache_save_CPPFLAGS
                    CXXFLAGS=$mustache_save_CXXFLAGS
                ])
                AS_IF([test "x$mustache_cv_cxx_fortify_source_3" = "xyes"], [
                    MUSTACHE_CPPFLAGS="$MUSTACHE_CPPFLAGS -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3"
                ], [
                    AC_MSG_WARN([_FORTIFY_SOURCE=3 is unavailable; continuing with the compiler default])
                ])
            ])
            AS_CASE([" $CPPFLAGS $CXXFLAGS "], [*_GLIBCXX_ASSERTIONS*], [], [
                MUSTACHE_CPPFLAGS="$MUSTACHE_CPPFLAGS -D_GLIBCXX_ASSERTIONS=1"
            ])
            MUSTACHE_LDFLAGS="$MUSTACHE_LDFLAGS -Wl,-z,relro -Wl,-z,now"
        ])
    ])
])

AC_ARG_ENABLE([warnings-as-errors],
    [AS_HELP_STRING([--enable-warnings-as-errors],
        [treat project compiler warnings as errors])],
    [], [enable_warnings_as_errors=no])
AS_IF([test "x$enable_warnings_as_errors" = "xyes"], [
    AS_IF([test "x$GXX" != "xyes"], [
        AC_MSG_ERROR([--enable-warnings-as-errors requires a GCC-compatible C++ compiler])
    ])
    MUSTACHE_CXXFLAGS="$MUSTACHE_CXXFLAGS -Werror"
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
mustache_spec_enabled=no
AS_IF([test "x$with_mustache_spec" != "xno"], [
    mustache_spec_enabled=yes
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
AM_CONDITIONAL([MUSTACHE_SPEC_ENABLED],
    [test "x$mustache_spec_enabled" = "xyes"])

AC_ARG_WITH([json],
    [AS_HELP_STRING([--with-json@<:@=auto|yes|no@:>@],
        [build JSON support using nlohmann/json @<:@default=auto@:>@])],
    [], [with_json=auto])
AS_CASE([$with_json],
    [auto|yes|no], [],
    [AC_MSG_ERROR([--with-json accepts only auto, yes, or no])])
mustache_json_enabled=no
AS_IF([test "x$with_json" != "xno"], [
    PKG_CHECK_MODULES([NLOHMANN_JSON], [nlohmann_json >= 3.10.5], [
        mustache_json_enabled=yes
    ], [
        AS_IF([test "x$with_json" = "xyes"], [
            AC_MSG_ERROR([nlohmann_json >= 3.10.5 is required for --with-json=yes])
        ])
    ])
])
AS_IF([test "x$mustache_json_enabled" = "xyes"], [
    AS_IF([test "x$mustache_cv_cxx_visibility_hidden" != "xyes"], [
        AC_MSG_ERROR([JSON support requires private C++ symbol visibility])
    ])
    AC_DEFINE([HAVE_LIBJSON], [1], [Defined if JSON support is enabled])
])
AM_CONDITIONAL([MUSTACHE_JSON_ENABLED],
    [test "x$mustache_json_enabled" = "xyes"])
AC_MSG_CHECKING([whether JSON support is enabled])
AC_MSG_RESULT([$mustache_json_enabled])

AC_ARG_WITH([yaml],
    [AS_HELP_STRING([--with-yaml@<:@=auto|yes|no@:>@],
        [build YAML support using libyaml @<:@default=auto@:>@])],
    [], [with_yaml=auto])
AS_CASE([$with_yaml],
    [auto|yes|no], [],
    [AC_MSG_ERROR([--with-yaml accepts only auto, yes, or no])])
mustache_yaml_enabled=no
AS_IF([test "x$with_yaml" != "xno"], [
    PKG_CHECK_MODULES([YAML], [yaml-0.1], [
        mustache_yaml_enabled=yes
    ], [
        AS_IF([test "x$with_yaml" = "xyes"], [
            AC_MSG_ERROR([libyaml is required for --with-yaml=yes])
        ])
    ])
])
AS_IF([test "x$mustache_yaml_enabled" = "xyes"], [
    AC_DEFINE([HAVE_LIBYAML], [1], [Defined if YAML support is enabled])
    MUSTACHE_PKGCONFIG_REQUIRES_PRIVATE=yaml-0.1
], [
    YAML_CFLAGS=
    YAML_LIBS=
    MUSTACHE_PKGCONFIG_REQUIRES_PRIVATE=
])
AC_SUBST([MUSTACHE_PKGCONFIG_REQUIRES_PRIVATE])
AM_CONDITIONAL([MUSTACHE_YAML_ENABLED],
    [test "x$mustache_yaml_enabled" = "xyes"])
AC_MSG_CHECKING([whether YAML support is enabled])
AC_MSG_RESULT([$mustache_yaml_enabled])

AM_CONDITIONAL([MUSTACHE_SPEC_TEST_ENABLED],
    [test "x$mustache_spec_enabled" = "xyes" &&
     test "x$mustache_yaml_enabled" = "xyes"])

AC_CHECK_HEADERS([cstdlib dirent.h exception fstream iostream list map memory sstream stack stdexcept string utility vector yaml.h])
AC_TYPE_INT32_T
AC_TYPE_LONG_LONG_INT

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
