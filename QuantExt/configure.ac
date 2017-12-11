# Process this file with autoconf to produce a configure script.
AC_INIT([QuantExt], [1.8.3],
        [opensourcerisk.org/forum],
        [QuantExt])
AC_PREREQ(2.62)
AC_CONFIG_SRCDIR([qle/quantext.hpp])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([qle/config.hpp])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_MACRO_DIR([m4])

AC_COPYRIGHT([
ORE is

Copyright (C) 2016 Quaternion Risk Management Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

    Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

    Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    Neither the names of the copyright holders nor the names of the ORE
    Group or its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.])

AC_PROG_AWK
AC_PROG_INSTALL

# Default compilation variables
AC_SUBST([CXXFLAGS],[${CXXFLAGS='-g -O2 -Wall'}])

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

# Set ql_boost_lib_path if --with-boost-lib is provided
AC_ARG_WITH([boost-lib],
            AC_HELP_STRING([--with-boost-lib=LIB_PATH],
                           [Supply the location of Boost libraries]),
            [ql_boost_lib_path="`cd ${withval} 2>/dev/null && pwd`"],
            [])

# If --with-boost-lib is not provided, check env $BOOST
if test [ -z "$ql_boost_lib_path" ] ; then
  if test [ -n "$BOOST" ] ; then
    ql_boost_lib_path="${BOOST}/stage/lib"
  fi
fi

if test [ -n "$ql_boost_lib_path" ] ; then
   AC_SUBST([BOOST_LIB],["-L${ql_boost_lib_path}"])
   AC_SUBST([LDFLAGS],["${LDFLAGS} -L${ql_boost_lib_path}"])
fi

# QuantLib
AC_ARG_WITH([quantlib-prefix],
            AC_HELP_STRING([--with-quantlib-prefix=PREFIX],
                           [Supply the location of QuantLib if already installed]),
            [ql_prefix="`cd ${withval} 2>/dev/null && pwd`"],
            [])

if test [ -n "$ql_prefix" ] ; then
    AC_SUBST([CPPFLAGS],["${CPPFLAGS} -I${ql_prefix}/include"])
    AC_SUBST([LDFLAGS],["${LDFLAGS} -L${ql_prefix}/lib"])
else
    ql_path="`cd ../QuantLib 2>/dev/null && pwd`"
    if test [ -n "$ql_path" ] ; then
        AC_SUBST([CPPFLAGS],["${CPPFLAGS} -I${ql_path}"])
        AC_SUBST([LDFLAGS],["${LDFLAGS} -L${ql_path}/ql/.libs"])
    fi
fi

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

# Check for QuantLib version
QL_CHECK_QUANTLIB

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
