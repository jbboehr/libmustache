
AC_PREREQ([2.60])
AC_INIT([mustache], [0.2.1], [jbboehr@gmail.com])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/mustache.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build])

AM_INIT_AUTOMAKE([1.10 foreign])

#m4_include(m4/ax_cxx_compile_stdcxx_11.m4)
#m4_include(m4/stl_hash.m4)

AC_SUBST([VERSION_INT], [1])

# debug
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debug mode]))
if test "x$enable_debug" == "xyes"; then
    AC_DEFINE([DEBUG], [1], [Defined if debug functions requested])
fi

# gcov
AC_ARG_ENABLE([gcov],
    AS_HELP_STRING([--enable-gcov], [Enable gcov]))

AS_IF([test "x$enable_gcov" = "xyes"], [
  LDFLAGS="$LDFLAGS -lgcov"
  CPPFLAGS="$CPPFLAGS -fprofile-arcs -ftest-coverage"
])

# gprof
AC_ARG_ENABLE([gprof],
    AS_HELP_STRING([--enable-gprof], [Enable gprof]))

AS_IF([test "x$enable_gprof" = "xyes"], [
  # LDFLAGS="$LDFLAGS -lc_p"
  CXXFLAGS="$CXXFLAGS -pg"
])

# operand size
AC_ARG_VAR(VM_OP_SIZE, [The VM operand size in bytes])
AS_IF([test "x$VM_OP_SIZE" == "x"], [
  VM_OP_SIZE=2
])
AC_DEFINE_UNQUOTED([VM_OP_SIZE], $VM_OP_SIZE, [Size of the VM operands])

# Checks for programs.
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_LANG(C++)
AX_CXX_COMPILE_STDCXX_11

# Checks for libraries.
AC_CHECK_LIB(jansson, json_equal)
AC_CHECK_LIB(json, json_tokener_new)
AC_CHECK_LIB(yaml, yaml_get_version_string)
AS_IF([test "x$enable_gcov" = "xyes"], [
  AC_CHECK_LIB(gcov, main)
])

# Checks for header files.
AC_CHECK_HEADERS(cstdlib)
AC_CHECK_HEADERS(dirent.h)
AC_CHECK_HEADERS(exception)
AC_CHECK_HEADERS(iostream)
AC_CHECK_HEADERS(fstream)
AC_CHECK_HEADERS(list)
AC_CHECK_HEADERS(map)
AC_CHECK_HEADERS(memory)
AC_CHECK_HEADERS(sstream)
AC_CHECK_HEADERS(stack)
AC_CHECK_HEADERS(stdexcept)
AC_CHECK_HEADERS(string)
AC_CHECK_HEADERS(utility)
AC_CHECK_HEADERS(vector)
AC_CHECK_HEADERS(yaml.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_LONG_LONG_INT

# Checks for hash libraries
AC_CXX_STL_HASH

# Checks for library functions.

# Conditionals
AM_CONDITIONAL([GCC], [test x$GCC = xyes])
AM_CONDITIONAL([GCOV], [test x$enable_gcov = xyes])
AM_CONDITIONAL([GPROF], [test x$enable_gprof = xyes])

AC_CONFIG_FILES([
        mustache.pc
        Makefile
        doc/Makefile
        src/Makefile
        bin/Makefile
        test/Makefile
        test/spec/Makefile
])
AC_OUTPUT
