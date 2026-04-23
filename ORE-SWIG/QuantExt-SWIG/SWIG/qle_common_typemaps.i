/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef qle_common_typemaps_i
#define qle_common_typemaps_i

%{
#include <ql/optional.hpp>
#include <string>
%}

#if defined(SWIGPYTHON)
%typemap(in) QuantLib::ext::optional<bool> %{
    if ($input == Py_None)
        $1 = ext::nullopt;
    else if (PyBool_Check($input))
        $1 = $input == Py_True;
    else
        SWIG_exception(SWIG_TypeError, "bool expected");
%}
%typecheck (QL_TYPECHECK_BOOL) QuantLib::ext::optional<bool> %{
    $1 = (PyBool_Check($input) || $input == Py_None) ? 1 : 0;
%}
%typemap(out) QuantLib::ext::optional<bool> %{
    $result = !$1 ? Py_None : *$1 ? Py_True : Py_False;
    Py_INCREF($result);
%}

%typemap(in) QuantLib::ext::optional<Integer> %{
    if ($input == Py_None)
        $1 = ext::nullopt;
    else if (PyLong_Check($input))
        $1 = PyLong_AsLong($input);
    else
        SWIG_exception(SWIG_TypeError, "int expected");
%}
%typecheck (QL_TYPECHECK_INTEGER) QuantLib::ext::optional<Integer> %{
    $1 = (PyLong_Check($input) || $input == Py_None) ? 1 : 0;
%}
%typemap(out) QuantLib::ext::optional<Integer> %{
    $result = !$1 ? Py_None : PyLong_FromLong(*$1);
    Py_INCREF($result);
%}

%typemap(in) QuantLib::ext::optional<Real> %{
    if ($input == Py_None)
        $1 = ext::nullopt;
    else if (PyFloat_Check($input) || PyLong_Check($input))
        $1 = PyFloat_AsDouble($input);
    else
        SWIG_exception(SWIG_TypeError, "float expected");
%}
%typecheck (SWIG_TYPECHECK_DOUBLE) QuantLib::ext::optional<Real> %{
    $1 = (PyFloat_Check($input) || PyLong_Check($input) || $input == Py_None) ? 1 : 0;
%}
%typemap(out) QuantLib::ext::optional<Real> %{
    $result = !$1 ? Py_None : PyFloat_FromDouble(*$1);
    Py_INCREF($result);
%}

%typemap(in) QuantLib::ext::optional<std::string> %{
    if ($input == Py_None)
        $1 = ext::nullopt;
    else if (PyUnicode_Check($input)) {
        PyObject* bytes = PyUnicode_AsUTF8String($input);
        if (!bytes)
            SWIG_exception(SWIG_TypeError, "str expected");
        $1 = std::string(PyBytes_AsString(bytes));
        Py_DECREF(bytes);
    } else
        SWIG_exception(SWIG_TypeError, "str expected");
%}
%typecheck (SWIG_TYPECHECK_STRING) QuantLib::ext::optional<std::string> %{
    $1 = (PyUnicode_Check($input) || $input == Py_None) ? 1 : 0;
%}
%typemap(out) QuantLib::ext::optional<std::string> %{
    $result = !$1 ? Py_None : PyUnicode_FromString((*$1).c_str());
    Py_INCREF($result);
%}
#endif

#endif
