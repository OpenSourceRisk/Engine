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

#include <qle/math/randomvariable.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/generallinearleastsquares.hpp>
#include <ql/math/matrixutilities/qrdecomposition.hpp>

#include <boost/math/distributions/normal.hpp>

namespace QuantExt {

void Filter::clear() {
    n_ = 0;
    data_.clear();
    data_.shrink_to_fit();
    deterministic_ = false;
}

void Filter::updateDeterministic() {
    if (deterministic_ || !initialised())
        return;
    for (Size i = 1; i < n_; ++i) {
        if (data_[i] != data_.front())
            return;
    }
    setAll(data_.front());
}

void Filter::set(const Size i, const bool v) {
    QL_REQUIRE(i < n_, "Filter::set(" << i << "): out of bounds, size is " << n_);
    if (deterministic_) {
        if (v != data_.front())
            expand();
        else
            return;
    }
    data_[i] = v;
}

void Filter::setAll(const bool v) {
    data_ = std::vector<bool>(1, v);
    deterministic_ = true;
}

bool Filter::operator[](const Size i) const {
    if (deterministic_)
        return data_.front();
    else
        return data_[i];
}

bool Filter::at(const Size i) const {
    QL_REQUIRE(n_ > 0, "Filter::at(" << i << "): dimension is zero");
    if (deterministic_)
        return data_.front();
    QL_REQUIRE(i < n_, "Filter::at(" << i << "): out of bounds, size is " << n_);
    return operator[](i);
}

void Filter::expand() {
    if (!deterministic_)
        return;
    deterministic_ = false;
    data_.resize(size(), data_.front());
}

bool operator==(const Filter& a, const Filter& b) {
    if (a.size() != b.size())
        return false;
    for (Size j = 0; j < a.size(); ++j)
        if (a[j] != b[j])
            return false;
    return true;
}

Filter operator&&(Filter x, const Filter& y) {
    QL_REQUIRE(!x.initialised() || !y.initialised() || x.size() == y.size(),
               "RandomVariable: x && y: x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    if (x.deterministic() && !x.data_.front())
        return Filter(x.size(), false);
    if (y.deterministic() && !y.data_.front())
        return Filter(y.size(), false);
    if (!x.initialised() || !y.initialised())
        return Filter();
    if (!y.deterministic_)
        x.expand();
    else if (y.data_.front())
        return x;
    else
        return Filter(x.size(), false);
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = x.data_[i] && y[i];
    }
    return x;
}

Filter operator||(Filter x, const Filter& y) {
    QL_REQUIRE(!x.initialised() || !y.initialised() || x.size() == y.size(),
               "RandomVariable: x || y: x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    if (x.deterministic() && x.data_.front())
        return Filter(x.size(), true);
    if (y.deterministic() && y.data_.front())
        return Filter(y.size(), true);
    if (!x.initialised() || !y.initialised())
        return Filter();
    if (!y.deterministic_)
        x.expand();
    else if (!y.data_.front())
        return x;
    else
        return Filter(x.size(), true);
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = x.data_[i] || y[i];
    }
    return x;
}

Filter equal(Filter x, const Filter& y) {
    if (!x.initialised() || !y.initialised())
        return Filter();
    QL_REQUIRE(x.size() == y.size(),
               "RandomVariable: equal(x,y): x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    if (!y.deterministic_)
        x.expand();
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = x.data_[i] == y[i];
    }
    return x;
}

Filter operator!(Filter x) {
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = !x.data_[i];
    }
    return x;
}

RandomVariable::RandomVariable(const Filter& f, const Real valueTrue, const Real valueFalse, const Real time) {
    if (!f.initialised()) {
        clear();
        return;
    }
    n_ = f.size();
    if (f.deterministic())
        setAll(f.at(0) ? valueTrue : valueFalse);
    else {
        deterministic_ = false;
        data_.resize(n_);
        for (Size i = 0; i < n_; ++i)
            set(i, f[i] ? valueTrue : valueFalse);
    }
    time_ = time;
}

RandomVariable::RandomVariable(const QuantLib::Array& array, const Real time) {
    n_ = array.size();
    deterministic_ = false;
    time_ = time;
    data_ = std::vector<Real>(array.begin(), array.end());
}

void RandomVariable::copyToMatrixCol(QuantLib::Matrix& m, const Size j) const {
    if (deterministic_)
        std::fill(m.column_begin(j), std::next(m.column_end(j), n_), data_.front());
    else
        std::copy(data_.begin(), data_.end(), m.column_begin(j));
}

void RandomVariable::copyToArray(QuantLib::Array& array) const {
    if (deterministic_)
        std::fill(array.begin(), array.end(), data_.front());
    else
        std::copy(data_.begin(), data_.end(), array.begin());
}

void RandomVariable::clear() {
    n_ = 0;
    data_.clear();
    data_.shrink_to_fit();
    deterministic_ = false;
    time_ = Null<Real>();
}

void RandomVariable::updateDeterministic() {
    if (deterministic_ || !initialised())
        return;
    for (Size i = 1; i < n_; ++i) {
        if (!QuantLib::close_enough(data_[i], data_.front()))
            return;
    }
    setAll(data_.front());
}

void RandomVariable::set(const Size i, const Real v) {
    QL_REQUIRE(i < n_, "RandomVariable::set(" << i << "): out of bounds, size is " << n_);
    if (deterministic_) {
        if (!QuantLib::close_enough(v, data_.front()))
            expand();
        else
            return;
    }
    data_[i] = v;
}

void RandomVariable::setAll(const Real v) {
    data_ = std::vector<Real>(1, v);
    deterministic_ = true;
}

Real RandomVariable::operator[](const Size i) const {
    if (deterministic_)
        return data_.front();
    else
        return data_[i];
}

Real RandomVariable::at(const Size i) const {
    QL_REQUIRE(n_ > 0, "RandomVariable::at(" << i << "): dimension is zero");
    if (deterministic_)
        return data_.front();
    QL_REQUIRE(i < n_, "RandomVariable::at(" << i << "): out of bounds, size is " << n_);
    return operator[](i);
}

void RandomVariable::expand() {
    if (!deterministic_)
        return;
    deterministic_ = false;
    data_.resize(size(), data_.front());
}

// Real* RandomVariable::begin() {
//     QL_REQUIRE(!deterministic_, "Deterministic_ RandomVariable does not provide iterators");
//     return &data_.front();
// }
// Real* RandomVariable::end() {
//     QL_REQUIRE(!deterministic_, "Deterministic_ RandomVariable does not provide iterators");
//     return &data_.front() + n_;
// }
// const Real* RandomVariable::begin() const {
//     QL_REQUIRE(!deterministic_, "Deterministic_ RandomVariable does not provide iterators");
//     return &data_.front();
// }
// const Real* RandomVariable::end() const {
//     QL_REQUIRE(!deterministic_, "Deterministic_ RandomVariable does not provide iterators");
//     return &data_.front() + n_;
// }

void RandomVariable::checkTimeConsistencyAndUpdate(const Real t) {
    QL_REQUIRE((time_ == Null<Real>() || t == Null<Real>()) || QuantLib::close_enough(time_, t),
               "RandomVariable: inconsistent times " << time_ << " and " << t);
    if (time_ == Null<Real>())
        time_ = t;
}

void checkTimeConsistency(const RandomVariable& x, const RandomVariable& y) {
    QL_REQUIRE((x.time() == Null<Real>() || y.time() == Null<Real>()) || QuantLib::close_enough(x.time(), y.time()),
               "got inconsistent random variable times (" << x.time() << ", " << y.time() << ")");
}

bool operator==(const RandomVariable& a, const RandomVariable& b) {
    if (a.size() != b.size())
        return false;
    for (Size j = 0; j < a.size(); ++j)
        if (a[j] != b[j])
            return false;
    return QuantLib::close_enough(a.time(), b.time());
}

RandomVariable& RandomVariable::operator+=(const RandomVariable& y) {
    if (!y.initialised())
        clear();
    if (!initialised())
        return *this;
    QL_REQUIRE(size() == y.size(),
               "RandomVariable: x += y: x size (" << size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        expand();
    else if (QuantLib::close_enough(y.data_.front(), 0.0))
        return *this;
    for (Size i = 0; i < data_.size(); ++i) {
        data_[i] += y[i];
    }
    return *this;
}

RandomVariable& RandomVariable::operator-=(const RandomVariable& y) {
    if (!y.initialised())
        clear();
    if (!initialised())
        return *this;
    QL_REQUIRE(size() == y.size(),
               "RandomVariable: x -= y: x size (" << size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        expand();
    else if (QuantLib::close_enough(y.data_.front(), 0.0))
        return *this;
    for (Size i = 0; i < data_.size(); ++i) {
        data_[i] -= y[i];
    }
    return *this;
}

RandomVariable& RandomVariable::operator*=(const RandomVariable& y) {
    if (!y.initialised())
        clear();
    if (!initialised())
        return *this;
    QL_REQUIRE(size() == y.size(),
               "RandomVariable: x *= y: x size (" << size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        expand();
    else if (QuantLib::close_enough(y.data_.front(), 1.0))
        return *this;
    for (Size i = 0; i < data_.size(); ++i) {
        data_[i] *= y[i];
    }
    return *this;
}

RandomVariable& RandomVariable::operator/=(const RandomVariable& y) {
    if (!y.initialised())
        clear();
    if (!initialised())
        return *this;
    QL_REQUIRE(size() == y.size(),
               "RandomVariable: x /= y: x size (" << size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        expand();
    else if (QuantLib::close_enough(y.data_.front(), 1.0))
        return *this;
    for (Size i = 0; i < data_.size(); ++i) {
        data_[i] /= y[i];
    }
    return *this;
}

RandomVariable operator+(RandomVariable x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    x += y;
    return x;
}

RandomVariable operator-(RandomVariable x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    x -= y;
    return x;
}

RandomVariable operator*(RandomVariable x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    x *= y;
    return x;
}

RandomVariable operator/(RandomVariable x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    x /= y;
    return x;
}

RandomVariable max(RandomVariable x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    QL_REQUIRE(x.size() == y.size(),
               "RandomVariable: max(x,y): x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    x.checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        x.expand();
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = std::max(x.data_[i], y[i]);
    }
    return x;
}

RandomVariable min(RandomVariable x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    QL_REQUIRE(x.size() == y.size(),
               "RandomVariable: min(x,y): x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    x.checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        x.expand();
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = std::min(x.data_[i], y[i]);
    }
    return x;
}

RandomVariable pow(RandomVariable x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    QL_REQUIRE(x.size() == y.size(),
               "RandomVariable: pow(x,y): x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    x.checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        x.expand();
    else if (QuantLib::close_enough(y.data_.front(), 1.0))
        return x;
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = std::pow(x.data_[i], y[i]);
    }
    return x;
}

RandomVariable operator-(RandomVariable x) {
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = -x.data_[i];
    }
    return x;
}

RandomVariable abs(RandomVariable x) {
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = std::abs(-x.data_[i]);
    }
    return x;
}

RandomVariable exp(RandomVariable x) {
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = std::exp(x.data_[i]);
    }
    return x;
}

RandomVariable log(RandomVariable x) {
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = std::log(x.data_[i]);
    }
    return x;
}

RandomVariable sqrt(RandomVariable x) {
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = std::sqrt(x.data_[i]);
    }
    return x;
}

RandomVariable sin(RandomVariable x) {
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = std::sin(x.data_[i]);
    }
    return x;
}

RandomVariable cos(RandomVariable x) {
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = std::cos(x.data_[i]);
    }
    return x;
}

RandomVariable normalCdf(RandomVariable x) {
    static const boost::math::normal_distribution<double> n;
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = boost::math::cdf(n, x.data_[i]);
    }
    return x;
}

RandomVariable normalPdf(RandomVariable x) {
    static const boost::math::normal_distribution<double> n;
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = boost::math::pdf(n, x.data_[i]);
    }
    return x;
}

Filter close_enough(const RandomVariable& x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return Filter();
    QL_REQUIRE(x.size() == y.size(), "RandomVariable: close_enough(x,y): x size ("
                                         << x.size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistency(x, y);
    if (x.deterministic_ && y.deterministic_) {
        return Filter(x.size(), QuantLib::close_enough(x.data_.front(), y.data_.front()));
    }
    Filter result(x.size(), false);
    for (Size i = 0; i < x.size(); ++i) {
        result.set(i, QuantLib::close_enough(x[i], y[i]));
    }
    return result;
}

bool close_enough_all(const RandomVariable& x, const RandomVariable& y) {
    QL_REQUIRE(x.size() == y.size(), "RandomVariable: close_enough_all(x,y): x size ("
                                         << x.size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistency(x, y);
    if (x.deterministic_ && y.deterministic_) {
        return QuantLib::close_enough(x.data_.front(), y.data_.front());
    }
    for (Size i = 0; i < x.size(); ++i) {
        if (!QuantLib::close_enough(x[i], y[i]))
            return false;
    }
    return true;
}

RandomVariable conditionalResult(const Filter& f, RandomVariable x, const RandomVariable& y) {
    if (!f.initialised() || !x.initialised() || !y.initialised())
        return RandomVariable();
    QL_REQUIRE(f.size() == x.size(),
               "conditionalResult(f,x,y): f size (" << f.size() << ") must match x size (" << x.size() << ")");
    QL_REQUIRE(f.size() == y.size(),
               "conditionalResult(f,x,y): f size (" << f.size() << ") must match y size (" << y.size() << ")");
    x.checkTimeConsistencyAndUpdate(y.time());
    if (f.deterministic())
        return f.at(0) ? x : y;
    x.expand();
    for (Size i = 0; i < f.size(); ++i) {
        if (!f[i])
            x.set(i, y[i]);
    }
    return x;
}

RandomVariable indicatorEq(RandomVariable x, const RandomVariable& y, const Real trueVal, const Real falseVal) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    QL_REQUIRE(x.size() == y.size(), "RandomVariable: indicatorEq(x,y): x size ("
                                         << x.size() << ") must be equal to y size (" << y.size() << ")");
    x.checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        x.expand();
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = QuantLib::close_enough(x.data_[i], y[i]) ? trueVal : falseVal;
    }
    return x;
}

RandomVariable indicatorGt(RandomVariable x, const RandomVariable& y, const Real trueVal, const Real falseVal) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    QL_REQUIRE(x.size() == y.size(), "RandomVariable: indicatorEq(x,y): x size ("
                                         << x.size() << ") must be equal to y size (" << y.size() << ")");
    x.checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        x.expand();
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = (x.data_[i] > y[i] && !QuantLib::close_enough(x.data_[i], y[i])) ? trueVal : falseVal;
    }
    return x;
}

RandomVariable indicatorGeq(RandomVariable x, const RandomVariable& y, const Real trueVal, const Real falseVal) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    QL_REQUIRE(x.size() == y.size(), "RandomVariable: indicatorEq(x,y): x size ("
                                         << x.size() << ") must be equal to y size (" << y.size() << ")");
    x.checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        x.expand();
    for (Size i = 0; i < x.data_.size(); ++i) {
        x.data_[i] = (x.data_[i] > y[i] || QuantLib::close_enough(x.data_[i], y[i])) ? trueVal : falseVal;
    }
    return x;
}

Filter operator<(const RandomVariable& x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return Filter();
    QL_REQUIRE(x.size() == y.size(),
               "RandomVariable: x < y: x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistency(x, y);
    if (x.deterministic_ && y.deterministic_) {
        return Filter(x.size(),
                      x.data_.front() < y.data_.front() && !QuantLib::close_enough(x.data_.front(), y.data_.front()));
    }
    Filter result(x.size(), false);
    for (Size i = 0; i < x.size(); ++i) {
        result.set(i, x[i] < y[i] && !QuantLib::close_enough(x[i], y[i]));
    }
    return result;
}

Filter operator<=(const RandomVariable& x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return Filter();
    QL_REQUIRE(x.size() == y.size(),
               "RandomVariable: x <= y: x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistency(x, y);
    if (x.deterministic_ && y.deterministic_) {
        return Filter(x.size(),
                      x.data_.front() < y.data_.front() || QuantLib::close_enough(x.data_.front(), y.data_.front()));
    }
    Filter result(x.size(), false);
    for (Size i = 0; i < x.size(); ++i) {
        result.set(i, x[i] < y[i] || QuantLib::close_enough(x[i], y[i]));
    }
    return result;
}

Filter operator>(const RandomVariable& x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return Filter();
    QL_REQUIRE(x.size() == y.size(),
               "RandomVariable: x > y: x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistency(x, y);
    if (x.deterministic_ && y.deterministic_) {
        return Filter(x.size(),
                      x.data_.front() > y.data_.front() && !QuantLib::close_enough(x.data_.front(), y.data_.front()));
    }
    Filter result(x.size(), false);
    for (Size i = 0; i < x.size(); ++i) {
        result.set(i, x[i] > y[i] && !QuantLib::close_enough(x[i], y[i]));
    }
    return result;
}

Filter operator>=(const RandomVariable& x, const RandomVariable& y) {
    if (!x.initialised() || !y.initialised())
        return Filter();
    QL_REQUIRE(x.size() == y.size(),
               "RandomVariable: x >= y: x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistency(x, y);
    if (x.deterministic_ && y.deterministic_) {
        return Filter(x.size(),
                      x.data_.front() > y.data_.front() || QuantLib::close_enough(x.data_.front(), y.data_.front()));
    }
    Filter result(x.size(), false);
    for (Size i = 0; i < x.size(); ++i) {
        result.set(i, x[i] > y[i] || QuantLib::close_enough(x[i], y[i]));
    }
    return result;
}

RandomVariable applyFilter(RandomVariable x, const Filter& f) {
    if (!x.initialised())
        return x;
    QL_REQUIRE(!f.initialised() || f.size() == x.size(), "RandomVariable: applyFitler(x,f): filter size ("
                                                             << f.size() << ") must be equal to x size (" << x.size()
                                                             << ")");
    if (!f.initialised())
        return x;
    if (f.deterministic()) {
        if (!f[0])
            return RandomVariable(x.size(), 0.0, x.time());
        else
            return x;
    }
    if (x.deterministic_ && QuantLib::close_enough(x.data_.front(), 0.0))
        return x;
    for (Size i = 0; i < x.size(); ++i) {
        if (!f[i])
            x.set(i, 0.0);
    }
    return x;
}

RandomVariable applyInverseFilter(RandomVariable x, const Filter& f) {
    if (!x.initialised())
        return x;
    QL_REQUIRE(!f.initialised() || f.size() == x.size(), "RandomVariable: applyFitler(x,f): filter size ("
                                                             << f.size() << ") must be equal to x size (" << x.size()
                                                             << ")");
    if (!f.initialised())
        return x;
    if (f.deterministic()) {
        if (f[0])
            return RandomVariable(x.size(), 0.0, x.time());
        else
            return x;
    }
    if (x.deterministic_ && QuantLib::close_enough(x.data_.front(), 0.0))
        return x;
    for (Size i = 0; i < x.size(); ++i) {
        if (f[i])
            x.set(i, 0.0);
    }
    return x;
}

Array regressionCoefficients(
    RandomVariable r, const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Filter& filter) {
    for (auto const reg : regressor) {
        QL_REQUIRE(reg->size() == r.size(),
                   "regressor size (" << reg->size() << ") must match regressand size (" << r.size() << ")");
    }
    QL_REQUIRE(filter.size() == 0 || filter.size() == r.size(),
               "filter size (" << filter.size() << ") must match regressand size (" << r.size() << ")");
    Matrix A(r.size(), basisFn.size());
    for (Size j = 0; j < basisFn.size(); ++j) {
        RandomVariable a = basisFn[j](regressor);
        if (filter.initialised()) {
            a = applyFilter(a, filter);
        }
        if (a.deterministic())
            std::fill(A.column_begin(j), A.column_end(j), a[0]);
        else
            a.copyToMatrixCol(A, j);
    }

    if (filter.size() > 0) {
        r = applyFilter(r, filter);
    }
    Array b(r.size());
    if (r.deterministic())
        std::fill(b.begin(), b.end(), r[0]);
    else
        r.copyToArray(b);
    Array res = qrSolve(A, b);
    return res;
}

RandomVariable conditionalExpectation(
    const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Array& coefficients) {
    QL_REQUIRE(!regressor.empty(), "regressor vector is empty");
    Size n = regressor.front()->size();
    for (Size i = 1; i < regressor.size(); ++i) {
        QL_REQUIRE(n == regressor[i]->size(), "regressor #" << i << " size (" << regressor[i]->size()
                                                            << ") must match regressor #0 size (" << n << ")");
    }
    QL_REQUIRE(basisFn.size() == coefficients.size(),
               "basisFn size (" << basisFn.size() << ") must match coefficients size (" << coefficients.size() << ")");
    RandomVariable r(n, 0.0);
    for (Size i = 0; i < coefficients.size(); ++i) {
        r = r + RandomVariable(n, coefficients[i]) * basisFn[i](regressor);
    }
    return r;
}

RandomVariable conditionalExpectation(
    const RandomVariable& r, const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Filter& filter) {
    if (r.deterministic())
        return r;
    auto coeff = regressionCoefficients(r, regressor, basisFn, filter);
    return conditionalExpectation(regressor, basisFn, coeff);
}

RandomVariable expectation(const RandomVariable& r) {
    if (r.deterministic())
        return r;
    Real sum = 0.0;
    for (Size i = 0; i < r.size(); ++i)
        sum += r[i];
    return RandomVariable(r.size(), sum / static_cast<Real>(r.size()));
}

RandomVariable black(const RandomVariable& omega, const RandomVariable& t, const RandomVariable& strike,
                     const RandomVariable& forward, const RandomVariable& impliedVol) {
    Filter zeroStrike = close_enough(strike, RandomVariable(omega.size(), 0.0));
    Filter call = omega > RandomVariable(omega.size(), 0.0);
    RandomVariable stdDev = impliedVol * sqrt(t);
    RandomVariable d1 = log(forward / strike) / stdDev + RandomVariable(omega.size(), 0.5) * stdDev;
    RandomVariable d2 = d1 - stdDev;
    return applyFilter(forward, zeroStrike && call) +
           applyInverseFilter(omega * (forward * normalCdf(omega * d1) - strike * normalCdf(omega * d2)), zeroStrike);
}

RandomVariable indicatorDerivative(const RandomVariable& x, const double eps) {
    RandomVariable tmp(x.size(), 0.0);

    // We follow section 4, eq 10 in
    // Fries, 2017: Automatic Backward Differentiation for American Monte-Carlo Algorithms -
    //              ADD for Conditional Expectations and Indicator Functions

    if (QuantLib::close_enough(eps, 0.0) || x.deterministic())
        return tmp;

    Real sum = 0.0;
    for (Size i = 0; i < x.size(); ++i) {
        sum += x[i] * x[i];
    }

    Real delta = std::sqrt(sum / static_cast<Real>(x.size())) * eps / 2.0;

    if (QuantLib::close_enough(delta, 0.0))
        return tmp;

    // compute derivative

    for (Size i = 0; i < tmp.size(); ++i) {
        Real ax = std::abs(x[i]);

        // linear approximation of step
        // if (ax < delta) {
        //     tmp.set(i, 1.0 / (2.0 * delta));
        // }

        // logistic function
        tmp.set(i, std::exp(-1.0 / delta * ax) / (delta * std::pow(1.0 + std::exp(-1.0 / delta * ax), 2.0)));
    }

    return tmp;
}

std::function<void(RandomVariable&)> RandomVariable::deleter =
    std::function<void(RandomVariable&)>([](RandomVariable& x) { x.clear(); });

} // namespace QuantExt
