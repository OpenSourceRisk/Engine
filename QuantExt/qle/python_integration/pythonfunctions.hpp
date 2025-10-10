/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file qle/python_integration/pythonfunctions.hpp
    \brief interface to external python functions
*/

#pragma once

#include <ql/patterns/singleton.hpp>

#ifdef ORE_PYTHON_INTEGRATION

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#endif

namespace QuantExt {

class PythonFunctions : public QuantLib::Singleton<PythonFunctions> {
public:
    PythonFunctions();
    ~PythonFunctions();

    RandomVariable conditionalExpectation(const RandomVariable& r, const std::vector<const RandomVariable*>& regressor,
                                          const Filter& filter = Filter());


private:
    bool initialized_ = false;
#ifdef ORE_PYTHON_INTEGRATION
    PyObject* pModule_;
    PyObject* pConditionalExpectation_;
#endif


};

} // namespace QuantExt
