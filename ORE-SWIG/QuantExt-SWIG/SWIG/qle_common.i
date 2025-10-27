/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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
#ifndef qle_common_i
#define qle_common_i

%{
#include<boost/optional.hpp>
%}


#if defined(SWIGPYTHON)
%typemap(in) boost::optional<bool> %{
    if($input == Py_None) {
        $1 = boost::optional<bool>();
    }
    else if($input == Py_True){
        $1 = boost::optional<bool>(true);
    } else{
        $1 = boost::optional<bool>(false);
    }
%}

%typemap(out) boost::optional<bool> %{
    if($1) {
        $result = PyBool_FromBool(*$1);
    }
    else {
        $result = Py_None;
        Py_INCREF(Py_None);
    }
%}

%typemap(BoostFilesystemPath) const boost::filesystem::path& "String"

%typecheck (QL_TYPECHECK_BOOL) boost::optional<bool> {
if (PyBool_Check($input) || Py_None == $input) 
	$1 = 1;
else
	$1 = 0;
}

%typemap(in) ext::optional<bool> %{
    if ($input == Py_None)
        $1 = ext::nullopt;
    else if (PyBool_Check($input))
        $1 = $input == Py_True;
    else
        SWIG_exception(SWIG_TypeError, "bool expected");
%}
%typecheck (QL_TYPECHECK_BOOL) ext::optional<bool> %{
    $1 = (PyBool_Check($input) || $input == Py_None) ? 1 : 0;
%}
%typemap(out) ext::optional<bool> %{
    $result = !$1 ? Py_None : *$1 ? Py_True : Py_False;
    Py_INCREF($result);
%}

#endif

#endif