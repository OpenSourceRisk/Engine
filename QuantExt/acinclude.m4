# QL_TO_UPPER(STRING)
# -------------------
# Convert the passed string to uppercase
AC_DEFUN([QL_TO_UPPER],
[translit([$1],[abcdefghijklmnopqrstuvwxyz.],[ABCDEFGHIJKLMNOPQRSTUVWXYZ_])])

# QL_CHECK_HEADER(HEADER)
# -----------------------
# Check whether <cheader> exists, falling back to <header.h>.
# It defines HAVE_CHEADER or HAVE_HEADER_H and sets the rl_header
# variable depending on the one found.
AC_DEFUN([QL_CHECK_HEADER],
[AC_CHECK_HEADER(
    [c$1],
    [AC_SUBST([rl_$1],[c$1])
     AC_DEFINE(QL_TO_UPPER([have_c$1]),[1],
               [Define to 1 if you have the <c$1> header file.])
    ],
    [AC_CHECK_HEADER([$1.h],
        [AC_SUBST([ql_$1],[$1.h])
         AC_DEFINE(QL_TO_UPPER(have_$1_h),1,
                   [Define to 1 if you have the <$1.h> header file.])
        ],
        [AC_MSG_ERROR([$1 not found])])
    ])
])

# QL_CHECK_FUNC(FUNCTION,ARGS,HEADER)
# -----------------------------------
# Check whether FUNCTION (including namespace qualifier) when compiling
# after including HEADER. 
AC_DEFUN([QL_CHECK_FUNC],
[AC_MSG_CHECKING([for compilation with $1])
 AC_TRY_COMPILE(
    [@%:@include <$3>],
    [$1($2);],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([$1 did not compile])])
])

# QL_CHECK_NAMESPACES
# ----------------------------------------------
# Check whether namespaces are supported.
AC_DEFUN([QL_CHECK_NAMESPACES],
[AC_MSG_CHECKING([namespace support])
 AC_TRY_COMPILE(
    [namespace Foo { struct A {}; }
     using namespace Foo;
    ],
    [A a;],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([namespaces not supported])
    ])
])

# QL_CHECK_BOOST_DEVEL
# --------------------
# Check whether the Boost headers are available
AC_DEFUN([QL_CHECK_BOOST_DEVEL],
[AC_MSG_CHECKING([for Boost development files])
 AC_TRY_COMPILE(
    [@%:@include <boost/version.hpp>
     @%:@include <boost/shared_ptr.hpp>
     @%:@include <boost/assert.hpp>
     @%:@include <boost/current_function.hpp>],
    [],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([Boost development files not found])
    ])
])

# QL_CHECK_BOOST_VERSION
# ----------------------
# Check whether the Boost installation is up to date
AC_DEFUN([QL_CHECK_BOOST_VERSION],
[AC_MSG_CHECKING([Boost version])
 AC_REQUIRE([QL_CHECK_BOOST_DEVEL])
 AC_TRY_COMPILE(
    [@%:@include <boost/version.hpp>],
    [@%:@if BOOST_VERSION < 103400
     @%:@error too old
     @%:@endif],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([outdated Boost installation])
    ])
])

# QL_CHECK_BOOST_UNIT_TEST
# ------------------------
# Check whether the Boost unit-test framework is available
AC_DEFUN([QL_CHECK_BOOST_UNIT_TEST],
[AC_MSG_CHECKING([for Boost unit-test framework])
 AC_REQUIRE([AC_PROG_CC])
 ql_original_LIBS=$LIBS
 ql_original_CXXFLAGS=$CXXFLAGS
 CC_BASENAME=`basename $CC`
 CC_VERSION=`echo "__GNUC__ __GNUC_MINOR__" | $CC -E -x c - | tail -n 1 | $SED -e "s/ //"`
 for boost_lib in boost_unit_test_framework-$CC_BASENAME$CC_VERSION \
                  boost_unit_test_framework-$CC_BASENAME \
                  boost_unit_test_framework \
                  boost_unit_test_framework-mt-$CC_BASENAME$CC_VERSION \
                  boost_unit_test_framework-$CC_BASENAME$CC_VERSION-mt \
                  boost_unit_test_framework-x$CC_BASENAME$CC_VERSION-mt \
                  boost_unit_test_framework-mt-$CC_BASENAME \
                  boost_unit_test_framework-$CC_BASENAME-mt \
                  boost_unit_test_framework-mt ; do
     LIBS="$ql_original_LIBS -l$boost_lib"
     # 1.33.1 or 1.34 static
     CXXFLAGS="$ql_original_CXXFLAGS"
     boost_unit_found=no
     AC_LINK_IFELSE(
         [@%:@include <boost/test/unit_test.hpp>
          using namespace boost::unit_test_framework;
          test_suite*
          init_unit_test_suite(int argc, char** argv)
          {
              return (test_suite*) 0;
          }
         ],
         [boost_unit_found=$boost_lib
          boost_defines=""
          break],
         [])
     # 1.34 shared
     CXXFLAGS="$ql_original_CXXFLAGS -DBOOST_TEST_MAIN -DBOOST_TEST_DYN_LINK"
     boost_unit_found=no
     AC_LINK_IFELSE(
         [@%:@include <boost/test/unit_test.hpp>
          using namespace boost::unit_test_framework;
          test_suite*
          init_unit_test_suite(int argc, char** argv)
          {
              return (test_suite*) 0;
          }
         ],
         [boost_unit_found=$boost_lib
          boost_defines="-DBOOST_TEST_DYN_LINK"
          break],
         [])
 done
 LIBS="$ql_original_LIBS"
 CXXFLAGS="$ql_original_CXXFLAGS"
 if test "$boost_unit_found" = no ; then
     AC_MSG_RESULT([no])
     AC_SUBST([BOOST_UNIT_TEST_LIB],[""])
     AC_SUBST([BOOST_UNIT_TEST_MAIN_CXXFLAGS],[""])
     AC_MSG_WARN([Boost unit-test framework not found.])
     AC_MSG_WARN([The test suite will be disabled.])
 else
     AC_MSG_RESULT([yes])
     AC_SUBST([BOOST_UNIT_TEST_LIB],[$boost_lib])
     AC_SUBST([BOOST_UNIT_TEST_MAIN_CXXFLAGS],[$boost_defines])
 fi
])

# for boost_lib in boost_unit_test_framework-$CC boost_unit_test_framework \
#           boost_unit_test_framework-mt-$CC boost_unit_test_framework-mt ; do
#     LIBS="$ql_original_LIBS -l$boost_lib"
#     boost_unit_found=no
#     AC_LINK_IFELSE(
#         [@%:@include <boost/test/unit_test.hpp>
#          using namespace boost::unit_test_framework;
#          test_suite*
#          init_unit_test_suite(int argc, char** argv)
#          {
#              return (test_suite*) 0;
#          }
#         ],
#         [boost_unit_found=$boost_lib
#          break],
#         [])
# done
# LIBS="$ql_original_LIBS"
# if test "$boost_unit_found" = no ; then
#     AC_MSG_RESULT([no])
#     AC_SUBST([BOOST_UNIT_TEST_LIB],[""])
#     AC_MSG_WARN([Boost unit-test framework not found])
#     AC_MSG_WARN([The test suite will be disabled])
# else
#     AC_MSG_RESULT([yes])
#     AC_SUBST([BOOST_UNIT_TEST_LIB],[$boost_lib])
# fi
#])

# QL_CHECK_BOOST_TEST_STREAM
# --------------------------
# Check whether Boost unit-test stream accepts std::fixed
AC_DEFUN([QL_CHECK_BOOST_TEST_STREAM],
[AC_MSG_CHECKING([whether Boost unit-test streams work])
 AC_REQUIRE([AC_PROG_CC])
 AC_TRY_COMPILE(
    [@%:@include <boost/test/unit_test.hpp>
     @%:@include <iomanip>],
    [BOOST_ERROR("foo " << std::fixed << 42.0);],
    [AC_MSG_RESULT([yes])
     AC_SUBST(BOOST_UNIT_TEST_DEFINE,[-DQL_WORKING_BOOST_STREAMS])],
    [AC_MSG_RESULT([no])
     AC_SUBST(BOOST_UNIT_TEST_DEFINE,[""])
    ])
])

# QL_CHECK_BOOST
# --------------
# Boost-related tests
AC_DEFUN([QL_CHECK_BOOST],
[AC_REQUIRE([QL_CHECK_BOOST_DEVEL])
 AC_REQUIRE([QL_CHECK_BOOST_VERSION])
 AC_REQUIRE([QL_CHECK_BOOST_UNIT_TEST])
 AC_REQUIRE([QL_CHECK_BOOST_TEST_STREAM])
])

# QL_CHECK_QUANTLIB
# -----------------
# Check whether the QuantLib headers are available
AC_DEFUN([QL_CHECK_QUANTLIB_HEADER],
[AC_MSG_CHECKING([for QuantLib header files])
 AC_TRY_COMPILE(
    [@%:@include <ql/quantlib.hpp>],
    [],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([QuantLib header files not found])
    ])
])

# QL_CHECK_QUANTLIB_VERSION
# -------------------------
# Check whether the QuantLib installation is up to date
AC_DEFUN([QL_CHECK_QUANTLIB_VERSION],
[AC_MSG_CHECKING([QuantLib version])
 AC_REQUIRE([QL_CHECK_QUANTLIB_HEADER])
 AC_TRY_COMPILE(
    [@%:@include <ql/version.hpp>],
    [@%:@if QL_HEX_VERSION < 0x010001f0
     @%:@error too old
     @%:@endif],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([QuantLib outdated])
    ])
])

# QL_CHECK_QUANTLIB
# -----------------
# QuantLib-related tests
AC_DEFUN([QL_CHECK_QUANTLIB],
[AC_REQUIRE([QL_CHECK_QUANTLIB_HEADER])
 AC_REQUIRE([QL_CHECK_QUANTLIB_VERSION])
])

# CHECK_MUPARSER_HEADER
# ---------------------
# Check whether the muParser headers are available
AC_DEFUN([CHECK_MUPARSER_HEADER],
[AC_MSG_CHECKING([for muParser header files])
 AC_TRY_COMPILE(
    [@%:@include <muParser.h>],
    [],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([MuParser header files not found])
    ])
])

