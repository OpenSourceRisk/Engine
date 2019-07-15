/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef orea_autolink_hpp
#define orea_autolink_hpp

#include <boost/config.hpp>

// select toolset:
#if (_MSC_VER >= 1920)
#  define OPEN_SOURCE_RISKANALYTICS_LIB_TOOLSET "vc142"
#elif (_MSC_VER >= 1910)
#  define OPEN_SOURCE_RISKANALYTICS_LIB_TOOLSET "vc141"
#elif (_MSC_VER >= 1900)
#  define OPEN_SOURCE_RISKANALYTICS_LIB_TOOLSET "vc140"
#elif (_MSC_VER >= 1800)
#  define OPEN_SOURCE_RISKANALYTICS_LIB_TOOLSET "vc120"
#elif (_MSC_VER >= 1700)
#  define OPEN_SOURCE_RISKANALYTICS_LIB_TOOLSET "vc110"
#elif (_MSC_VER >= 1600)
#  define OPEN_SOURCE_RISKANALYTICS_LIB_TOOLSET "vc100"
#else
#  error "unsupported Microsoft compiler"
#endif

#ifdef _M_X64
#define OPEN_SOURCE_RISKANALYTICS_LIB_PLATFORM "-x64"
#else
#define OPEN_SOURCE_RISKANALYTICS_LIB_PLATFORM
#endif

/*** libraries to be linked ***/

// select thread opt:
#ifdef _MT
#define OPEN_SOURCE_RISKANALYTICS_LIB_THREAD_OPT "-mt"
#else
#define OPEN_SOURCE_RISKANALYTICS_LIB_THREAD_OPT
#endif

// select linkage opt:
#ifdef _DLL
#if defined(_DEBUG)
#define OPEN_SOURCE_RISKANALYTICS_LIB_RT_OPT "-gd"
#else
#define OPEN_SOURCE_RISKANALYTICS_LIB_RT_OPT
#endif
#else
#if defined(_DEBUG)
#define OPEN_SOURCE_RISKANALYTICS_LIB_RT_OPT "-sgd"
#else
#define OPEN_SOURCE_RISKANALYTICS_LIB_RT_OPT "-s"
#endif
#endif

#define OPEN_SOURCE_RISKANALYTICS_LIB_NAME                                                                               \
    "OREAnalytics-" OPEN_SOURCE_RISKANALYTICS_LIB_TOOLSET OPEN_SOURCE_RISKANALYTICS_LIB_PLATFORM                \
        OPEN_SOURCE_RISKANALYTICS_LIB_THREAD_OPT OPEN_SOURCE_RISKANALYTICS_LIB_RT_OPT ".lib"

#pragma comment(lib, OPEN_SOURCE_RISKANALYTICS_LIB_NAME)
#ifdef BOOST_LIB_DIAGNOSTIC
#pragma message("Will (need to) link to lib file: " OPEN_SOURCE_RISKANALYTICS_LIB_NAME)
#endif

#endif
