
AC_PREREQ([2.60])
AC_INIT([mustache], [0.0.1-dev], [jbboehr@gmail.com])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/mustache.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build])

AM_INIT_AUTOMAKE([1.10 foreign])

m4_include(m4/ax_cxx_compile_stdcxx_11.m4)

AC_SUBST([VERSION_INT], [1])

# gcov
AC_ARG_ENABLE([gcov],
    AS_HELP_STRING([--enable-gcov], [Enable gcov]))

AS_IF([test "x$enable_gcov" = "xyes"], [
  LDFLAGS="$LDFLAGS -lgcov"
  CPPFLAGS="$CPPFLAGS -fprofile-arcs -ftest-coverage"
])

# Checks for programs.
AC_PROG_CXX
AC_PROG_LIBTOOL
AM_CONDITIONAL([GCC], [test x$GCC = xyes])
AC_LANG(C++)
AX_CXX_COMPILE_STDCXX_11

# Checks for libraries.
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

# Checks for library functions.

# Checks for c++0x
AC_DEFUN([AC_COMPILE_STDCXX_0X], [
  AC_CACHE_CHECK(if g++ supports C++0x features without additional flags,
  ac_cv_cxx_compile_cxx0x_native,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([
  template <typename T>
    struct check
    {
      static_assert(sizeof(int) <= sizeof(T), "not big enough");
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c;
    check_type&& cr = c;],,
  ac_cv_cxx_compile_cxx0x_native=yes, ac_cv_cxx_compile_cxx0x_native=no)
  AC_LANG_RESTORE
  ])

  AC_CACHE_CHECK(if g++ supports C++0x features with -std=c++0x,
  ac_cv_cxx_compile_cxx0x_cxx,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -std=c++0x"
  AC_TRY_COMPILE([
  template <typename T>
    struct check
    {
      static_assert(sizeof(int) <= sizeof(T), "not big enough");
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c;
    check_type&& cr = c;],,
  ac_cv_cxx_compile_cxx0x_cxx=yes, ac_cv_cxx_compile_cxx0x_cxx=no)
  CXXFLAGS="$ac_save_CXXFLAGS"
  AC_LANG_RESTORE
  ])

  AC_CACHE_CHECK(if g++ supports C++0x features with -std=gnu++0x,
  ac_cv_cxx_compile_cxx0x_gxx,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -std=gnu++0x"
  AC_TRY_COMPILE([
  template <typename T>
    struct check
    {
      static_assert(sizeof(int) <= sizeof(T), "not big enough");
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c;
    check_type&& cr = c;],,
  ac_cv_cxx_compile_cxx0x_gxx=yes, ac_cv_cxx_compile_cxx0x_gxx=no)
  CXXFLAGS="$ac_save_CXXFLAGS"
  AC_LANG_RESTORE
  ])

  if test "$ac_cv_cxx_compile_cxx0x_native" = yes ||
     test "$ac_cv_cxx_compile_cxx0x_cxx" = yes ||
     test "$ac_cv_cxx_compile_cxx0x_gxx" = yes; then
    AC_DEFINE(HAVE_STDCXX_0X,,[Define if g++ supports C++0x features. ])
  fi
])
AC_COMPILE_STDCXX_0X

# Checks for c++11
# AC_COMPILE_STDCXX_11
AC_DEFUN([AC_COMPILE_STDCXX_11], [
  AC_CACHE_CHECK(if g++ supports C++11 features without additional flags,
  ac_cv_cxx_compile_cxx11_native,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([
  template <typename T>
    struct check final
    {
      static constexpr T value{ __cplusplus };
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c{};
    check_type&& cr = static_cast<check_type&&>(c);

    static_assert(check_type::value == 201103L, "C++11 compiler");],,
  ac_cv_cxx_compile_cxx11_native=yes, ac_cv_cxx_compile_cxx11_native=no)
  AC_LANG_RESTORE
  ])

  AC_CACHE_CHECK(if g++ supports C++11 features with -std=c++11,
  ac_cv_cxx_compile_cxx11_cxx,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -std=c++11"
  AC_TRY_COMPILE([
  template <typename T>
    struct check final
    {
      static constexpr T value{ __cplusplus };
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c{};
    check_type&& cr = static_cast<check_type&&>(c);

    static_assert(check_type::value == 201103L, "C++11 compiler");],,
  ac_cv_cxx_compile_cxx11_cxx=yes, ac_cv_cxx_compile_cxx11_cxx=no)
  CXXFLAGS="$ac_save_CXXFLAGS"
  AC_LANG_RESTORE
  ])

  AC_CACHE_CHECK(if g++ supports C++11 features with -std=gnu++11,
  ac_cv_cxx_compile_cxx11_gxx,
  [AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -std=gnu++11"
  AC_TRY_COMPILE([
  template <typename T>
    struct check final
    {
      static constexpr T value{ __cplusplus };
    };

    typedef check<check<bool>> right_angle_brackets;

    int a;
    decltype(a) b;

    typedef check<int> check_type;
    check_type c{};
    check_type&& cr = static_cast<check_type&&>(c);

    static_assert(check_type::value == 201103L, "C++11 compiler");],,
  ac_cv_cxx_compile_cxx11_gxx=yes, ac_cv_cxx_compile_cxx11_gxx=no)
  CXXFLAGS="$ac_save_CXXFLAGS"
  AC_LANG_RESTORE
  ])

  if test "$ac_cv_cxx_compile_cxx11_native" = yes ||
     test "$ac_cv_cxx_compile_cxx11_cxx" = yes ||
     test "$ac_cv_cxx_compile_cxx11_gxx" = yes; then
    AC_DEFINE(HAVE_STDCXX_11,,[Define if g++ supports C++11 features. ])
  fi
])
AC_COMPILE_STDCXX_11

AC_CONFIG_FILES([
        mustache.pc
        Makefile
        doc/Makefile
        src/Makefile
        test/Makefile
        test/spec/Makefile
])
AC_OUTPUT
