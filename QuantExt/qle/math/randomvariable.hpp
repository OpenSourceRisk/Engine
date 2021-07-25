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
    template <class I> Filter(I begin, I end) : n_(end - begin), data_(begin, end), deterministic_(false) {}
    template <class T> Filter(std::initializer_list<T> init) : Filter(init.begin(), init.end()) {}
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

    // the raw data
    const std::vector<bool>& data() const { return data_; }
    std::vector<bool>& data() { return data_; }
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
    template <class I>
    RandomVariable(I begin, I end, const Real time = Null<Real>())
        : n_(end - begin), data_(begin, end), deterministic_(false), time_(time) {}
    template <class T>
    RandomVariable(std::initializer_list<T> init, const Real time = Null<Real>())
        : RandomVariable(init.begin(), init.end(), time) {}
    // ctor from filter, set components to valueTrue where filter is true and to valueFalse otherwise
    explicit RandomVariable(const Filter& f, const Real valueTrue = 1.0, const Real valueFalse = 0.0,
                            const Real time = Null<Real>());
    // modifiers
    void clear();
    void set(const Size i, const Real v);
    void setTime(const Real time) { time_ = time; }

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
    // iterators
    Real* begin();
    Real* end();
    const Real* begin() const;
    const Real* end() const;
    //
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

    // the raw data
    const std::vector<Real>& data() const { return data_; }
    std::vector<Real>& data() { return data_; }
    // expand vector to full size and set determinisitc to false
    void expand();

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

} // namespace QuantExt
