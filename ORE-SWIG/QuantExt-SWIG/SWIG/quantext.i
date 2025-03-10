/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#if defined(SWIGRUBY)
%module QuantExtc
#elif defined(SWIGCSHARP)
%module(directors="1") NQuantExtc
#elif defined(SWIGJAVA)
%module(directors="1") QuantExt
#else
%module QuantExt
#endif

%include exception.i

%exception {
    try {
        $action
    } catch (std::out_of_range& e) {
        SWIG_exception(SWIG_IndexError,const_cast<char*>(e.what()));
    } catch (std::exception& e) {
        SWIG_exception(SWIG_RuntimeError,const_cast<char*>(e.what()));
    } catch (...) {
        SWIG_exception(SWIG_UnknownError,"unknown error");
    }
}

#if defined(SWIGPYTHON)
%{
#include <qle/version.hpp>
const int    __hexversion__ = OPEN_SOURCE_RISK_VERSION_NUM;
const char* __version__    = OPEN_SOURCE_RISK_VERSION;
%}

const int __hexversion__;
%immutable;
const char* __version__;
%mutable;
#endif

#if defined(JAVA_AUTOLOAD)
// Automatically load the shared library for JAVA binding
%pragma(java) jniclasscode=%{
  /// Load the JNI library
  static {
    System.loadLibrary("QuantExtJNI");
  }
%}
#endif

//#if defined(SWIGPYTHON)
//%feature("autodoc");
//#endif

// include all quantlib .i's
%include ql.i

// include all quantext .i's
%include qle.i
