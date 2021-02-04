/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef quantext_autolink_hpp
#define quantext_autolink_hpp

#include <boost/config.hpp>

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

#define QE_LIB_NAME "QuantExt" QE_LIB_PLATFORM QE_LIB_THREAD_OPT QE_LIB_RT_OPT ".lib"

#pragma comment(lib, QE_LIB_NAME)
#ifdef BOOST_LIB_DIAGNOSTIC
#pragma message("Will (need to) link to lib file: " QE_LIB_NAME)
#endif

#endif
