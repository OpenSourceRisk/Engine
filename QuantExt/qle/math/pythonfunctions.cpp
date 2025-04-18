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

#include <qle/math/randomvariable.hpp>
#include <qle/python_integration/pythonfunctions.hpp>

#include <ql/errors.hpp>

#include <iostream>

namespace QuantExt {

#ifdef ORE_PYTHON_INTEGRATION

PyObject* toPythonList(const std::vector<const RandomVariable*>& x) {
    PyObject* x0 = PyList_New(x.front()->size());
    for (std::size_t k = 0; k < x.front()->size(); ++k) {
        PyObject* x1 = PyList_New(x.size());
        for (std::size_t i = 0; i < x.size(); ++i) {
            PyObject* d = PyFloat_FromDouble(x[i]->operator[](k));
            PyList_SET_ITEM(x1, i, d);
        }
        PyList_SET_ITEM(x0, k, x1);
    }
    return x0;
}

PythonFunctions::PythonFunctions() {

    Py_Initialize();

    PyObject* pName = PyUnicode_DecodeFSDefault("ore_python_integration");
    pModule_ = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule_ == nullptr) {
        std::cerr << "error initializing ore_python_integration:" << std::endl;
        if (PyErr_Occurred())
            PyErr_Print();
        return;
    }

    pConditionalExpectation_ = PyObject_GetAttrString(pModule_, "conditional_expectation");

    if (pConditionalExpectation_ == nullptr || !PyCallable_Check(pConditionalExpectation_)) {
        std::cerr << "conditional_expectation is null or not callable" << std::endl;
        return;
    }

    initialized_ = true;
}

PythonFunctions::~PythonFunctions() {
    Py_XDECREF(pConditionalExpectation_);
    Py_XDECREF(pModule_);
    if (Py_FinalizeEx() < 0) {
        std::cerr << "PythonFunctions: error in destructor." << std::endl;
    }
}

RandomVariable PythonFunctions::conditionalExpectation(const RandomVariable& r,
                                                       const std::vector<const RandomVariable*>& regressor,
                                                       const Filter& filter) {

    QL_REQUIRE(initialized_, "PythonFunctions::conditionalExpectation(): not initialized.");
    QL_REQUIRE(!filter.initialised(), "PythonFunctions::conditionalExpectation() does not support non-empty filter");
    QL_REQUIRE(!regressor.empty(), "PythonFunctions::conditionalExpectation(): empty regressor not allowed.");

    PyObject* xl = toPythonList(regressor);
    PyObject* yl = toPythonList({&r});

    PyObject* pArgs = PyTuple_New(2);
    PyTuple_SetItem(pArgs, 0, xl);
    PyTuple_SetItem(pArgs, 1, yl);

    PyObject* result = PyObject_CallObject(pConditionalExpectation_, pArgs);

    RandomVariable tmp;

    if (result != nullptr && PyList_Check(result)) {
        std::size_t n = PyList_Size(result);
        tmp = RandomVariable(n);
        for (std::size_t i = 0; i < n; ++i) {
            tmp.set(i, PyFloat_AsDouble(PyList_GET_ITEM(result, i)));
        }
    }

    Py_DECREF(pArgs);

    if (PyErr_Occurred()) {
        std::cerr << "PythonFunctions::conditionalExpectation(): an error occured." << std::endl;
        PyErr_Print();
    }

    return tmp;
}

#else

PythonFunctions::PythonFunctions() {}
PythonFunctions::~PythonFunctions() {}

RandomVariable PythonFunctions::conditionalExpectation(const RandomVariable& r,
                                                       const std::vector<const RandomVariable*>& regressor,
                                                       const Filter& filter) {
    QL_REQUIRE(initialized_,
               "PythonFunctions::conditionalExpectation(): not available, compile with ORE_PYTHON_INTEGRATION");
}

#endif

} // namespace QuantExt
