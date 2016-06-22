# Process this file with autoconf to produce a configure script.
AC_INIT([QuantExt], [0.1],
        [dev@quaternion.com],
        [QuantExt])
AC_PREREQ(2.62)
AC_CONFIG_SRCDIR([qle/quantext.hpp])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([qle/config.hpp])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_MACRO_DIR([m4])

AC_COPYRIGHT([
Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
All Rights Reserved.])

AC_PROG_AWK
AC_PROG_INSTALL

# Default compilation variables
AC_SUBST([CXXFLAGS],[${CXXFLAGS='-g -O2 -Wall -Wextra'}])

# Help the compiler find external software

AC_ARG_WITH([boost-include],
            AC_HELP_STRING([--with-boost-include=INCLUDE_PATH],
                           [Supply the location of Boost header files]),
            [ql_boost_include_path="`cd ${withval} 2>/dev/null && pwd`"],
            [ql_boost_include_path="${BOOST}"])
if test [ -n "$ql_boost_include_path" ] ; then
   AC_SUBST([BOOST_INCLUDE],["-I${ql_boost_include_path}"])
   AC_SUBST([CPPFLAGS],["${CPPFLAGS} -I${ql_boost_include_path}"])
fi
AC_ARG_WITH([boost-lib],
            AC_HELP_STRING([--with-boost-lib=LIB_PATH],
                           [Supply the location of Boost libraries]),
            [ql_boost_lib_path="`cd ${withval} 2>/dev/null && pwd`"],
            [ql_boost_lib_path="${BOOST}/stage/lib"])
if test [ -n "$ql_boost_lib_path" ] ; then
   AC_SUBST([BOOST_LIB],["-L${ql_boost_lib_path}"])
   AC_SUBST([LDFLAGS],["${LDFLAGS} -L${ql_boost_lib_path}"])
fi

# QuantLib
ql_path="`cd ../QuantLib 2>/dev/null && pwd`"
AC_SUBST([CPPFLAGS],["${CPPFLAGS} -I${ql_path}"])
AC_SUBST([LDFLAGS],["${LDFLAGS} -L${ql_path}/ql/.libs"])

# Continue setup

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LIBTOOL

AM_CONDITIONAL(ENABLE_STATIC, test "${enable_static}" == "yes")

AC_LANG([C++])

AC_ARG_ENABLE([openmp],
              AC_HELP_STRING([--enable-openmp],
                             [If enabled, configure will try to detect
                              and enable OpenMP support.]),
              [ql_openmp=$enableval],
              [ql_openmp=no])
if test "$ql_openmp" = "yes" ; then
   AC_OPENMP
   AC_SUBST([CXXFLAGS],["${CXXFLAGS} ${OPENMP_CXXFLAGS}"])
fi

# Check for mandatory features

QL_CHECK_ASINH

# Check for Boost components
QL_CHECK_BOOST
AM_CONDITIONAL(BOOST_UNIT_TEST_FOUND, test "x${BOOST_UNIT_TEST_LIB}" != "x")

# Check for optional features
QL_CHECK_LONG_LONG

# Check for Emacs
AM_PATH_LISPDIR
AM_CONDITIONAL(EMACS_FOUND, test "${EMACS}" != "no")

# Check for tools needed for building documentation
AC_PATH_PROG([DOXYGEN], [doxygen])
AC_PATH_PROG([SED], [sed])
AC_PATH_PROG([LATEX], [latex])
AC_PATH_PROG([PDFLATEX], [pdflatex])
AC_PATH_PROG([MAKEINDEX], [makeindex])
AC_PATH_PROG([DVIPS], [dvips])


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
AC_CONFIG_FILES([
    Makefile
    qle/Makefile
    qle/cashflows/Makefile
    qle/currencies/Makefile
    qle/indexes/Makefile
    qle/indexes/ibor/Makefile
    qle/instruments/Makefile
    qle/math/Makefile
    qle/methods/Makefile
    qle/models/Makefile
    qle/pricingengines/Makefile
    qle/processes/Makefile
    qle/termstructures/Makefile
    qle/quotes/Makefile
    m4/Makefile
    doc/Makefile
    test/Makefile])
AC_OUTPUT
