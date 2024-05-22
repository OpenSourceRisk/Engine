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
#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/patterns/singleton.hpp>
#include <ql/types.hpp>

#include <ql/functional.hpp>
#include <boost/timer/timer.hpp>

#include <initializer_list>
#include <vector>

namespace QuantExt {

using namespace QuantLib;

// statistics

struct RandomVariableStats : public QuantLib::Singleton<RandomVariableStats> {
    RandomVariableStats() {
        data_timer.start();
        data_timer.stop();
        calc_timer.start();
        calc_timer.stop();
    }

    void reset() {
      enabled = false;
      data_ops = 0;
      calc_ops = 0;
      data_timer.stop();
      calc_timer.stop();
    }

    bool enabled = false;
    std::size_t data_ops = 0;
    std::size_t calc_ops = 0;
    boost::timer::cpu_timer data_timer;
    boost::timer::cpu_timer calc_timer;
};

// filter class

struct Filter {
    // ctors
    ~Filter();
    Filter();
    Filter(const Filter& r);
    Filter(Filter&& r);
    Filter& operator=(const Filter& r);
    Filter& operator=(Filter&& r);

    explicit Filter(const Size n, const bool value = false);
    // modifiers
    void clear();
    void set(const Size i, const bool v);
    void setAll(const bool v);
    void resetSize(const Size n);

    // inspectors
    // true => det., but false => non-det. only after updateDeterministic()
    bool deterministic() const { return deterministic_; }
    void updateDeterministic();

    bool initialised() const { return n_ != 0; }
    Size size() const { return n_; }
    bool operator[](const Size i) const; // undefined if uninitialized or i out of bounds
    bool at(const Size i) const;         // with checks for initialized, i within bounds
    //
    friend Filter operator&&(Filter, const Filter&);
    friend Filter operator||(Filter, const Filter&);
    friend Filter equal(Filter, const Filter&);
    friend Filter operator!(Filter);
    friend bool operator==(const Filter&, const Filter&);

    // expand vector to full size and set deterministic to false
    void expand();

    // pointer to raw data, this is null for deterministic variables
    bool* data();

private:
    // for invariants see the corresponding section below in class RandomVariable
    Size n_;
    bool constantData_;
    bool* data_;
    bool deterministic_;
};

// inline element-wise access operators

inline void Filter::set(const Size i, const bool v) {
    QL_REQUIRE(i < n_, "Filter::set(" << i << "): out of bounds, size is " << n_);
    if (deterministic_) {
        if (v != constantData_)
            expand();
        else
            return;
    }
    data_[i] = v;
}

inline bool Filter::operator[](const Size i) const {
    if (deterministic_)
        return constantData_;
    else
        return data_[i];
}

inline bool Filter::at(const Size i) const {
    QL_REQUIRE(n_ > 0, "Filter::at(" << i << "): dimension is zero");
    if (deterministic_)
        return constantData_;
    QL_REQUIRE(i < n_, "Filter::at(" << i << "): out of bounds, size is " << n_);
    return operator[](i);
}

inline bool* Filter::data() { return data_; }

bool operator==(const Filter& a, const Filter& b);
bool operator!=(const Filter& a, const Filter& b);

Filter operator&&(Filter, const Filter&);
Filter operator||(Filter, const Filter&);
Filter equal(Filter, const Filter&);
Filter operator!(Filter);

// random variable class

struct RandomVariable {
    // ctors
    ~RandomVariable();
    RandomVariable();
    RandomVariable(const RandomVariable& r);
    RandomVariable(RandomVariable&& r);
    RandomVariable& operator=(const RandomVariable& r);
    RandomVariable& operator=(RandomVariable&& r);

    explicit RandomVariable(const Size n, const Real value = 0.0, const Real time = Null<Real>());
    explicit RandomVariable(const Filter& f, const Real valueTrue = 1.0, const Real valueFalse = 0.0,
                            const Real time = Null<Real>());
    explicit RandomVariable(const Size n, const Real* const data, const Real time = Null<Real>());
    explicit RandomVariable(const std::vector<double>& data, const Real time = Null<Real>());
    // interop with ql classes
    explicit RandomVariable(const QuantLib::Array& data, const Real time = Null<Real>());
    void copyToMatrixCol(QuantLib::Matrix&, const Size j) const;
    void copyToArray(QuantLib::Array& array) const;
    // modifiers
    void clear();
    void set(const Size i, const Real v);
    // all negative times are treated as 0 ( = deterministic value )
    void setTime(const Real time) { time_ = std::max(time, 0.0); }
    void setAll(const Real v);
    void resetSize(const Size n);

    // inspectors
    // true => det., but false => non-det. only after updateDeterministic()
    bool deterministic() const { return deterministic_; }
    void updateDeterministic();
    bool initialised() const { return n_ != 0; }
    Size size() const { return n_; }
    Real operator[](const Size i) const; // undefined if uninitialized or i out of bounds
    Real at(const Size i) const;         // with checks for initialized, i within bounds
    Real time() const { return time_; }
    RandomVariable& operator+=(const RandomVariable&);
    RandomVariable& operator-=(const RandomVariable&);
    RandomVariable& operator*=(const RandomVariable&);
    RandomVariable& operator/=(const RandomVariable&);
    friend bool operator==(const RandomVariable&, const RandomVariable&);
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
    friend RandomVariable indicatorGt(RandomVariable, const RandomVariable&, const Real trueVal, const Real falseVal,
                                      const Real eps);
    friend RandomVariable indicatorGeq(RandomVariable, const RandomVariable&, const Real trueVal, const Real falseVal,
                                       const Real eps);

    void expand();
    // pointer to raw data, this is null for deterministic variables
    double* data();

    static std::function<void(RandomVariable&)> deleter;

private:
    void checkTimeConsistencyAndUpdate(const Real t);
    /* Invariants that hold at all times for instances of this class:

       n_ = 0 means uninitialized, n_ > 0 means initialized.

       For an uninitialized instance:
       - constantData_ = 0.0, data_ = nullptr, deterministic_ = false, time_ = Null<Real>()

       For an initialized instance:
       - if deterministic = true a constant value is represented with
         - constantData_ the constant value
         - data_ = nullptr
       - if deterministic = false a possibly non-constant value is represented with
         - constantData_ initialized with last constant value that was set
         - data_ an array of size n_
    */
    Size n_;
    double constantData_;
    double* data_;
    bool deterministic_;
    Real time_;
};

bool operator==(const RandomVariable& a, const RandomVariable& b);
bool operator!=(const RandomVariable& a, const RandomVariable& b);

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
RandomVariable indicatorGt(RandomVariable, const RandomVariable&, const Real trueVal = 1.0, const Real falseVal = 0.0,
                           const Real eps = 0.0);
RandomVariable indicatorGeq(RandomVariable, const RandomVariable&, const Real trueVal = 1.0, const Real falseVal = 0.0,
                            const Real eps = 0.0);

Filter close_enough(const RandomVariable&, const RandomVariable&);
bool close_enough_all(const RandomVariable&, const RandomVariable&);

RandomVariable conditionalResult(const Filter&, RandomVariable, const RandomVariable&);

void checkTimeConsistency(const RandomVariable& x, const RandomVariable& y);

// set all entries to 0 where filter = false, leave the others unchanged
RandomVariable applyFilter(RandomVariable, const Filter&);
// set all entries to 0 where filter = true, leave the others unchanged
RandomVariable applyInverseFilter(RandomVariable, const Filter&);

/* Perform a factor reduction: We keep m factors so that "1 - varianceCutoff" of the total variance of the n regressor
   variables is explained by the m factors. The return value is m x n transforming from original coordinates to new
   coordinates. This can be a useful preprocessing step for linear regression to reduce the dimensionality or also to
   handle collinear regressors. */
Matrix pcaCoordinateTransform(const std::vector<const RandomVariable*>& regressor, const Real varianceCutoff = 1E-5);

/* Apply a coordinate transform */
std::vector<RandomVariable> applyCoordinateTransform(const std::vector<const RandomVariable*>& regressor,
                                                     const Matrix& transform);

/* Create vector of pointers to rvs from vector of rvs */
std::vector<const RandomVariable*> vec2vecptr(const std::vector<RandomVariable>& values);

// compute regression coefficients
enum class RandomVariableRegressionMethod { QR, SVD };
Array regressionCoefficients(
    RandomVariable r, std::vector<const RandomVariable*> regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Filter& filter = Filter(), const RandomVariableRegressionMethod = RandomVariableRegressionMethod::QR,
    const std::string& debugLabel = std::string());

// evaluate regression function
RandomVariable conditionalExpectation(
    const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Array& coefficients);

// compute and evaluate regression in one run
RandomVariable conditionalExpectation(
    const RandomVariable& r, const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Filter& filter = Filter(), const RandomVariableRegressionMethod = RandomVariableRegressionMethod::QR);

// time zero expectation
RandomVariable expectation(const RandomVariable& r);

// time zero variance
RandomVariable variance(const RandomVariable& r);

// time zero covariance
RandomVariable covariance(const RandomVariable& r, const RandomVariable& s);

// black formula
RandomVariable black(const RandomVariable& omega, const RandomVariable& t, const RandomVariable& strike,
                     const RandomVariable& forward, const RandomVariable& impliedVol);

// derivative of indicator function 1_{x>0}
RandomVariable indicatorDerivative(const RandomVariable& x, const double eps);

// is the given random variable deterministic and zero?
inline bool isDeterministicAndZero(const RandomVariable& x) {
    return x.deterministic() && QuantLib::close_enough(x[0], 0.0);
}

// inline element-wise access operators

inline void RandomVariable::set(const Size i, const Real v) {
    QL_REQUIRE(i < n_, "RandomVariable::set(" << i << "): out of bounds, size is " << n_);
    if (deterministic_) {
        if (!QuantLib::close_enough(v, constantData_)) {
            expand();
        } else {
            return;
        }
    }
    data_[i] = v;
}

inline Real RandomVariable::operator[](const Size i) const {
    if (deterministic_)
        return constantData_;
    else
        return data_[i];
}

inline Real RandomVariable::at(const Size i) const {
    QL_REQUIRE(n_ > 0, "RandomVariable::at(" << i << "): dimension is zero");
    if (deterministic_)
        return constantData_;
    QL_REQUIRE(i < n_, "RandomVariable::at(" << i << "): out of bounds, size is " << n_);
    return operator[](i);
}

inline double* RandomVariable::data() { return data_; }

/*! helper function that returns a LSM basis system with size restriction: the order is reduced until
  the size of the basis system is not greater than the given bound (if this is not null) or the order is 1 */
std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>
multiPathBasisSystem(Size dim, Size order, QuantLib::LsmBasisSystem::PolynomialType type,
                     Size basisSystemSizeBound = Null<Size>());

} // namespace QuantExt
