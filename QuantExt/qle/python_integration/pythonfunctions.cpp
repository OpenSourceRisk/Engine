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

#ifdef ORE_PYTHON_INTEGRATION
#include <boost/scoped_array.hpp>
#include <earth.h>
#endif

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

RandomVariable PythonFunctions::conditionalExpectationEarth(const RandomVariable& r,
                                                            const std::vector<const RandomVariable*>& regressor,
                                                            const Filter& filter) {

    // std::cout << "conditionalExpectationEarth..." << std::flush;

    QL_REQUIRE(!filter.initialised(), "PythonFunctions::conditionalExpectation() does not support non-empty filter");
    QL_REQUIRE(!regressor.empty(), "PythonFunctions::conditionalExpectation(): empty regressor not allowed.");

    // input

    int nCases = r.size();
    int nPreds = regressor.size();
    int nresponses = 1;

    int nMaxDegree = 1;
    int nMaxTerms = 101;
    double Trace = 0.0; // 3.0;
    // bool Format = false;
    double ForwardStepThresh = 0.00001;
    int K = 20;
    double FastBeta = 0.0;
    double NewVarPenalty = 0.0;
    double Penalty = ((nMaxDegree > 1) ? 3 : 2);
    double AdjuntEndSpan = 0.0;
    std::vector<int> LinPreds(nPreds, 0);
    std::vector<double> x(nCases * nPreds);
    std::vector<double> y(nCases * nresponses);

    for (Size i = 0; i < regressor.size(); ++i) {
        for (Size j = 0; j < nCases; ++j) {
            x[j + nCases * i] = regressor[i]->at(j);
        }
    }

    for (Size j = 0; j < nCases; ++j) {
        y[j] = r.at(j);
    }

    // set by call to Earth

    double BestGcv;
    int nTerms;
    int TermCond = 0;
    std::vector<double> bx(nCases * nMaxTerms);
    // bool* BestSet = new bool[nMaxTerms];
    std::unique_ptr<bool[]> BestSet(new bool[nMaxTerms]);
    std::vector<int> Dirs(nMaxTerms * nPreds, 0);
    std::vector<double> Cuts(nMaxTerms * nPreds, 0.0);
    std::vector<double> Residuals(nCases * nresponses, 0.0);
    std::vector<double> Betas(nMaxTerms * nresponses, 0.0);

    // train

    Earth(&BestGcv, &nTerms, &TermCond, BestSet.get(), &bx[0], &Dirs[0], &Cuts[0], &Residuals[0], &Betas[0], &x[0],
          &y[0], NULL, // weights are NULL
          nCases, nresponses, nPreds, nMaxDegree, nMaxTerms, Penalty, ForwardStepThresh,
          0,    // MinSpan
          0,    // EndSpan
          true, // Prune
          K, FastBeta, NewVarPenalty, &LinPreds[0], AdjuntEndSpan, true, Trace);

    // predict and set result

    RandomVariable result(nCases);

    std::vector<double> xVec(nPreds);
    std::vector<double> yHat(nresponses);
    for (int iCase = 0; iCase < nCases; iCase++) {
        for (int iPred = 0; iPred < nPreds; iPred++)
            xVec[iPred] = x[iCase + iPred * nCases];
        PredictEarth(&yHat[0], &xVec[0], BestSet.get(), &Dirs[0], &Cuts[0], &Betas[0], nPreds, nresponses, nTerms,
                     nMaxTerms);
        result.set(iCase, yHat[0]);
    }

    // std::cout << " done." << std::endl;

    return result;
}

#else

PythonFunctions::PythonFunctions() {}
PythonFunctions::~PythonFunctions() {}

RandomVariable PythonFunctions::conditionalExpectation(const RandomVariable& r,
                                                       const std::vector<const RandomVariable*>& regressor,
                                                       const Filter& filter) {
    QL_FAIL("PythonFunctions::conditionalExpectation(): not available, compile with ORE_PYTHON_INTEGRATION");
}

#endif

} // namespace QuantExt
