# Process this file with autoconf to produce a configure script.
AC_INIT([App], [0.1], [roland.lichters@quaternion.com], [App])
AC_PREREQ(2.50)
AC_CONFIG_SRCDIR([./ore.hpp])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([./config.hpp])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_MACRO_DIR([m4])

AC_COPYRIGHT([])

AC_PROG_AWK
AC_PROG_INSTALL

# Default compilation variables

#AC_SUBST([CXXFLAGS],[${CXXFLAGS='-g -O2 -Wall'}])
AC_SUBST([CXXFLAGS],[${CXXFLAGS='-g -O2 -Wall -std=c++11'}])

# Help the compiler find external software

AC_ARG_WITH([boost-include],
            AC_HELP_STRING([--with-boost-include=INCLUDE_PATH],
                           [Supply the location of Boost header directory boost/]),
            [ql_boost_include_path=$withval],
            [ql_boost_include_path=""])
if test [ -n "$ql_boost_include_path" ] ; then
   AC_SUBST([CPPFLAGS],["${CPPFLAGS} -I${ql_boost_include_path}"])
fi
AC_ARG_WITH([boost-lib],
            AC_HELP_STRING([--with-boost-lib=LIB_PATH],
                           [Supply the location of Boost libraries]),
            [ql_boost_lib_path=$withval],
            [ql_boost_lib_path=""])
if test [ -n "$ql_boost_lib_path" ] ; then
   AC_SUBST([LDFLAGS],["${LDFLAGS} -L${ql_boost_lib_path}"])
fi

#AC_ARG_WITH([ql-include],
#            AC_HELP_STRING([--with-ql-include=INCLUDE_PATH],
#                           [Supply the location of QuantLib header directory ql/]),
#            [ql_include_path=${withval}],
#            [AC_MSG_ERROR([Specify the QuantLib include path: "--with-ql-include=path/to/ql])])
#if test [ -n "$ql_include_path" ] ; then
#   AC_SUBST([CPPFLAGS],["${CPPFLAGS} -I${ql_include_path}"])
#fi
#AC_ARG_WITH([ql-lib],
#            AC_HELP_STRING([--with-ql-lib=LIB_PATH],
#                           [Supply the location of the QuantLib library]),
#            [ql_lib_path=${withval}],
#            [AC_MSG_ERROR([Specify the QuantLib library path: "--with-ql-lib=path/to/lib])])
#if test [ -n "$ql_lib_path" ] ; then
#   AC_SUBST([LDFLAGS],["${LDFLAGS} -L${ql_lib_path}"])
#fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LIBTOOL

AM_CONDITIONAL(ENABLE_STATIC, test "${enable_static}" == "yes")

# Language
AC_LANG([C++])

# Check for mandatory features and header files

QL_CHECK_NAMESPACES

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([float.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([floor pow sqrt strstr strtol])

# Check for Boost components, macro defined in acinclude.m4
#QL_CHECK_BOOST
#AM_CONDITIONAL(BOOST_UNIT_TEST_FOUND, test "x${BOOST_UNIT_TEST_LIB}" != "x")

# Check for QuantLib, macro defined in acinclude.m4
#QL_CHECK_QUANTLIB

# Check compiling code that uses QuantLib
#QL_CHECK_FUNC([QuantLib::Date::todaysDate], [], [ql/quantlib.hpp])

# Check for tools needed for building documentation
AC_PATH_PROG([DOXYGEN], [doxygen])
AC_PATH_PROG([SED], [sed])
AC_PATH_PROG([LATEX], [latex])
AC_PATH_PROG([PDFLATEX], [pdflatex])
AC_PATH_PROG([MAKEINDEX], [makeindex])
AC_PATH_PROG([DVIPS], [dvips])

# site configuration

AC_ARG_ENABLE([tracing],
              AC_HELP_STRING([--enable-tracing],
                             [If enabled, tracing messages might be emitted
                              by the library depending on run-time
                              settings. Enabling this option can degrade
                              performance.]),
              [tracing=$enableval],
              [tracing=no])
if test "$tracing" = "yes" ; then
   AC_DEFINE([ENABLE_TRACING],[1],
             [Define this if tracing messages should allowed (whether they
              are actually emitted will depend on run-time settings.)])

fi

AC_ARG_ENABLE([extra-safety-checks],
              AC_HELP_STRING([--enable-extra-safety-checks],
                             [If enabled, extra run-time checks are added
                              to a few functions. This can prevent their
                              inlining and degrade performance.]),
              [belt_and_braces=$enableval],
              [belt_and_braces=no])
if test "$belt_and_braces" = "yes" ; then
   AC_DEFINE([EXTRA_SAFETY_CHECKS],[1],
             [Define this if extra safety checks should be performed.
              This can degrade performance.])
fi

AC_ARG_ENABLE([deprecated-code],
              AC_HELP_STRING([--enable-deprecated-code],
                             [If enabled (the default,) deprecated code
                              will be included in the library.]),
              [ql_use_deprecated_code=$enableval],
              [ql_use_deprecated_code=yes])
if test "$ql_use_deprecated_code" = "no" ; then
   AC_DEFINE([QL_DISABLE_DEPRECATED],[1],
             [Define this if you want to disable deprecated code.])
fi

# manual configurations for specific hosts
case $host in
  powerpc-apple-darwin*)
    AC_DEFINE([QL_PATCH_DARWIN],[1],
              [Define if running on a Mac OS X machine.])
    ;;
  sparc-sun-solaris*)
    AC_DEFINE([QL_PATCH_SOLARIS],[1],
              [Define if running on a Sun Solaris machine.])
    ;;
esac


# done, output the configured files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

