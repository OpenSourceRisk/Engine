/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef quantext_autolink_hpp
#define quantext_autolink_hpp

#include <boost/config.hpp>

// select toolset:
#if (_MSC_VER < 1310)
#error "unsupported Microsoft compiler"
#elif(_MSC_VER == 1310)
#define QE_LIB_TOOLSET "vc71"
#elif(_MSC_VER == 1400)
#define QE_LIB_TOOLSET "vc80"
#elif(_MSC_VER == 1500)
#ifdef x64
#define QE_LIB_TOOLSET "vc90-x64"
#else
#define QE_LIB_TOOLSET "vc90"
#endif
#elif(_MSC_VER == 1600)
#define QE_LIB_TOOLSET "vc100"
#elif(_MSC_VER == 1700)
#define QE_LIB_TOOLSET "vc110"
#elif(_MSC_VER == 1800)
#define QE_LIB_TOOLSET "vc120"
#elif(_MSC_VER == 1900)
#define QE_LIB_TOOLSET "vc140"
#else
#define QE_LIB_TOOLSET "vc141"
#endif

#ifdef _M_X64
#define QE_LIB_PLATFORM "-x64"
#else
#define QE_LIB_PLATFORM
#endif

/*** libraries to be linked ***/

// select thread opt:
#ifdef _MT
#define QE_LIB_THREAD_OPT "-mt"
#else
#define QE_LIB_THREAD_OPT
#endif

// select linkage opt:
#ifdef _DLL
#if defined(_DEBUG)
#define QE_LIB_RT_OPT "-gd"
#else
#define QE_LIB_RT_OPT
#endif
#else
#if defined(_DEBUG)
#define QE_LIB_RT_OPT "-sgd"
#else
#define QE_LIB_RT_OPT "-s"
#endif
#endif

#define QE_LIB_NAME "QuantExt-" QE_LIB_TOOLSET QE_LIB_PLATFORM QE_LIB_THREAD_OPT QE_LIB_RT_OPT ".lib"

#pragma comment(lib, QE_LIB_NAME)
#ifdef BOOST_LIB_DIAGNOSTIC
#pragma message("Will (need to) link to lib file: " QE_LIB_NAME)
#endif

#endif
