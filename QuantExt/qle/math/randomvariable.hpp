/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#pragma once

#include <ql/errors.hpp>
#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>
#include <ql/types.hpp>

#include <boost/function.hpp>

#include <initializer_list>
#include <vector>

namespace QuantExt {

using namespace QuantLib;

// filter class

struct Filter {
    // ctors
    Filter() : n_(0), deterministic_(false) {}
    explicit Filter(const Size n, const bool value = false) : n_(n), data_(1, value), deterministic_(true) {}
    // modifiers
    void clear();
    void set(const Size i, const bool v);
    void setAll(const bool v);
    // inspectors
    // true => det., but false => non-det. only after updateDeterministic()
    bool deterministic() const { return deterministic_; }
    void updateDeterministic();

    bool initialised() const { return n_ != 0; }
    Size size() const { return n_; }
    bool operator[](const Size i) const; // no bound check
    bool at(const Size i) const;         // with bound check
    //
    friend Filter operator&&(Filter, const Filter&);
    friend Filter operator||(Filter, const Filter&);
    friend Filter equal(Filter, const Filter&);
    friend Filter operator!(Filter);
    friend bool operator==(const Filter&, const Filter&);

    // expand vector to full size and set deterministic to false
    void expand();

private:
    Size n_;
    std::vector<bool> data_;
    bool deterministic_;
};

bool operator==(const Filter& a, const Filter& b);

Filter operator&&(Filter, const Filter&);
Filter operator||(Filter, const Filter&);
Filter equal(Filter, const Filter&);
Filter operator!(Filter);

// random variable class

struct RandomVariable {
    // ctors
    RandomVariable() : n_(0), deterministic_(false), time_(Null<Real>()) {}
    explicit RandomVariable(const Size n, const Real value = 0.0, const Real time = Null<Real>())
        : n_(n), data_(1, value), deterministic_(true), time_(time) {}
    explicit RandomVariable(const Filter& f, const Real valueTrue = 1.0, const Real valueFalse = 0.0,
                            const Real time = Null<Real>());
    // interop with ql classes
    explicit RandomVariable(const QuantLib::Array& array, const Real time = Null<Real>());
    void copyToMatrixCol(QuantLib::Matrix&, const Size j) const;
    void copyToArray(QuantLib::Array& array) const;
    // modifiers
    void clear();
    void set(const Size i, const Real v);
    // all negative times are treated as 0 ( = deterministic value )
    void setTime(const Real time) { time_ = std::max(time, 0.0); }

    void setAll(const Real v);
    // inspectors
    // true => det., but false => non-det. only after updateDeterministic()
    bool deterministic() const { return deterministic_; }
    void updateDeterministic();
    bool initialised() const { return n_ != 0; }
    Size size() const { return n_; }
    Real operator[](const Size i) const; // no bound check
    Real at(const Size i) const;         // with bound check
    Real time() const { return time_; }
    RandomVariable& operator+=(const RandomVariable&);
    RandomVariable& operator-=(const RandomVariable&);
    RandomVariable& operator*=(const RandomVariable&);
    RandomVariable& operator/=(const RandomVariable&);
    friend RandomVariable operator+(RandomVariable, const RandomVariable&);
    friend RandomVariable operator-(RandomVariable, const RandomVariable&);
    friend RandomVariable operator*(RandomVariable, const RandomVariable&);
    friend RandomVariable operator/(RandomVariable, const RandomVariable&);
    friend RandomVariable max(RandomVariable, const RandomVariable&);
    friend RandomVariable min(RandomVariable, const RandomVariable&);
    friend RandomVariable pow(RandomVariable, const RandomVariable&);
    friend RandomVariable operator-(RandomVariable);
    friend RandomVariable abs(RandomVariable);
    friend RandomVariable exp(RandomVariable);
    friend RandomVariable log(RandomVariable);
    friend RandomVariable sqrt(RandomVariable);
    friend RandomVariable sin(RandomVariable);
    friend RandomVariable cos(RandomVariable);
    friend RandomVariable normalCdf(RandomVariable);
    friend RandomVariable normalPdf(RandomVariable);
    friend Filter close_enough(const RandomVariable&, const RandomVariable&);
    friend Filter operator<(const RandomVariable&, const RandomVariable&);
    friend Filter operator<=(const RandomVariable&, const RandomVariable&);
    friend Filter operator>(const RandomVariable&, const RandomVariable&);
    friend Filter operator>=(const RandomVariable&, const RandomVariable&);
    friend bool close_enough_all(const RandomVariable&, const RandomVariable&);
    friend RandomVariable applyFilter(RandomVariable, const Filter&);
    friend RandomVariable applyInverseFilter(RandomVariable, const Filter&);
    friend RandomVariable conditionalResult(const Filter&, RandomVariable, const RandomVariable&);
    friend RandomVariable indicatorEq(RandomVariable, const RandomVariable&, const Real trueVal, const Real falseVal);
    friend RandomVariable indicatorGt(RandomVariable, const RandomVariable&, const Real trueVal, const Real falseVal);
    friend RandomVariable indicatorGeq(RandomVariable, const RandomVariable&, const Real trueVal, const Real falseVal);

    void expand();

    static std::function<void(RandomVariable&)> deleter;

private:
    void checkTimeConsistencyAndUpdate(const Real t);
    Size n_;
    std::vector<Real> data_;
    bool deterministic_;
    Real time_;
};

bool operator==(const RandomVariable& a, const RandomVariable& b);

RandomVariable operator+(RandomVariable, const RandomVariable&);
RandomVariable operator-(RandomVariable, const RandomVariable&);
RandomVariable operator*(RandomVariable, const RandomVariable&);
RandomVariable operator/(RandomVariable, const RandomVariable&);
RandomVariable max(RandomVariable, const RandomVariable&);
RandomVariable min(RandomVariable, const RandomVariable&);
RandomVariable pow(RandomVariable, const RandomVariable&);
RandomVariable operator-(RandomVariable);
RandomVariable abs(RandomVariable);
RandomVariable exp(RandomVariable);
RandomVariable log(RandomVariable);
RandomVariable sqrt(RandomVariable);
RandomVariable sin(RandomVariable);
RandomVariable cos(RandomVariable);
RandomVariable normalCdf(RandomVariable);
RandomVariable normalPdf(RandomVariable);
RandomVariable indicatorEq(RandomVariable, const RandomVariable&, const Real trueVal = 1.0, const Real falseVal = 0.0);
RandomVariable indicatorGt(RandomVariable, const RandomVariable&, const Real trueVal = 1.0, const Real falseVal = 0.0);
RandomVariable indicatorGeq(RandomVariable, const RandomVariable&, const Real trueVal = 1.0, const Real falseVal = 0.0);

Filter close_enough(const RandomVariable&, const RandomVariable&);
bool close_enough_all(const RandomVariable&, const RandomVariable&);

RandomVariable conditionalResult(const Filter&, RandomVariable, const RandomVariable&);

void checkTimeConsistency(const RandomVariable& x, const RandomVariable& y);

// set all entries to 0 where filter = false, leave the others unchanged
RandomVariable applyFilter(RandomVariable, const Filter&);
// set all entries to 0 where filter = true, leave the others unchanged
RandomVariable applyInverseFilter(RandomVariable, const Filter&);

// compute regression coefficients
Array regressionCoefficients(
    RandomVariable r, const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Filter& filter = Filter());

// evaluate regression function
RandomVariable conditionalExpectation(
    const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Array& coefficients);

// compute and evaluate regression in one run
RandomVariable conditionalExpectation(
    const RandomVariable& r, const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Filter& filter = Filter());

// time zero expectation
RandomVariable expectation(const RandomVariable& r);

// black formula
RandomVariable black(const RandomVariable& omega, const RandomVariable& t, const RandomVariable& strike,
                     const RandomVariable& forward, const RandomVariable& impliedVol);

// derivative of indicator function 1_{x>0}
RandomVariable indicatorDerivative(const RandomVariable& x, const double eps);

} // namespace QuantExt
