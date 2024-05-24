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
#include <qle/math/randomvariablelsmbasissystem.hpp>

#include <ql/experimental/math/moorepenroseinverse.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/generallinearleastsquares.hpp>
#include <ql/math/matrixutilities/qrdecomposition.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>

#include <boost/math/distributions/normal.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/covariance.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>

#include <iostream>
#include <map>

// if defined, RandomVariableStats are updated (this might impact perfomance!), default is undefined
//#define ENABLE_RANDOMVARIABLE_STATS

namespace QuantExt {

namespace {

#ifdef ENABLE_RANDOMVARIABLE_STATS

inline void resumeDataStats() {
    if (RandomVariableStats::instance().enabled)
        RandomVariableStats::instance().data_timer.resume();
}

inline void stopDataStats(const std::size_t n) {
    if (RandomVariableStats::instance().enabled) {
        RandomVariableStats::instance().data_timer.stop();
        RandomVariableStats::instance().data_ops += n;
    }
}

inline void resumeCalcStats() {
    if (RandomVariableStats::instance().enabled)
        RandomVariableStats::instance().calc_timer.resume();
}

inline void stopCalcStats(const std::size_t n) {
    if (RandomVariableStats::instance().enabled) {
        RandomVariableStats::instance().calc_timer.stop();
        RandomVariableStats::instance().calc_ops += n;
    }
}

#else

inline void resumeDataStats() {}
inline void stopDataStats(const std::size_t n) {}
inline void resumeCalcStats() {}
inline void stopCalcStats(const std::size_t n) {}

#endif

double getDelta(const RandomVariable& x, const Real eps) {
    Real sum = 0.0;
    for (Size i = 0; i < x.size(); ++i) {
        sum += x[i] * x[i];
    }
    return std::sqrt(sum / static_cast<Real>(x.size())) * eps / 2.0;
}

} // namespace

Filter::~Filter() { clear(); }

Filter::Filter() : n_(0), constantData_(false), data_(nullptr), deterministic_(false) {}

Filter::Filter(const Filter& r) {
    n_ = r.n_;
    constantData_ = r.constantData_;
    if (r.data_) {
        resumeDataStats();
        data_ = new bool[n_];
        // std::memcpy(data_, r.data_, n_ * sizeof(bool));
        std::copy(r.data_, r.data_ + n_, data_);
        stopDataStats(n_);
    } else {
        data_ = nullptr;
    }
    deterministic_ = r.deterministic_;
}

Filter::Filter(Filter&& r) {
    n_ = r.n_;
    constantData_ = r.constantData_;
    data_ = r.data_;
    r.data_ = nullptr;
    deterministic_ = r.deterministic_;
}

Filter& Filter::operator=(const Filter& r) {
    if (r.deterministic_) {
        deterministic_ = true;
        if (data_) {
            delete[] data_;
            data_ = nullptr;
        }
    } else {
        deterministic_ = false;
        if (r.n_ != 0) {
            resumeDataStats();
            if (n_ != r.n_) {
                if (data_)
                    delete[] data_;
                data_ = new bool[r.n_];
            }
            // std::memcpy(data_, r.data_, r.n_ * sizeof(bool));
            std::copy(r.data_, r.data_ + r.n_, data_);
            stopDataStats(r.n_);
        } else {
            if (data_) {
                delete[] data_;
                data_ = nullptr;
            }
        }
    }
    n_ = r.n_;
    constantData_ = r.constantData_;
    return *this;
}

Filter& Filter::operator=(Filter&& r) {
    n_ = r.n_;
    constantData_ = r.constantData_;
    if (data_) {
        delete[] data_;
    }
    data_ = r.data_;
    r.data_ = nullptr;
    deterministic_ = r.deterministic_;
    return *this;
}

Filter::Filter(const Size n, const bool value) : n_(n), constantData_(value), data_(nullptr), deterministic_(n != 0) {}

void Filter::clear() {
    n_ = 0;
    constantData_ = false;
    if (data_) {
        delete[] data_;
        data_ = nullptr;
    }
    deterministic_ = false;
}

void Filter::updateDeterministic() {
    if (deterministic_ || !initialised())
        return;
    resumeCalcStats();
    for (Size i = 0; i < n_; ++i) {
        if (data_[i] != constantData_) {
            stopCalcStats(i);
            return;
        }
    }
    setAll(constantData_);
    stopCalcStats(n_);
}

void Filter::setAll(const bool v) {
    QL_REQUIRE(n_ > 0, "Filter::setAll(): dimension is zero");
    if (data_) {
        delete[] data_;
        data_ = nullptr;
    }
    constantData_ = v;
    deterministic_ = true;
}

void Filter::resetSize(const Size n) {
    QL_REQUIRE(deterministic_, "Filter::resetSize(): only possible for deterministic variables.");
    n_ = n;
}

void Filter::expand() {
    if (!deterministic_)
        return;
    deterministic_ = false;
    resumeDataStats();
    data_ = new bool[n_];
    std::fill(data_, data_ + n_, constantData_);
    stopDataStats(n_);
}

bool operator==(const Filter& a, const Filter& b) {
    if (a.size() != b.size())
        return false;
    if (a.deterministic_ && b.deterministic_) {
        return a.constantData_ == b.constantData_;
    } else {
        resumeCalcStats();
        for (Size j = 0; j < a.size(); ++j)
            if (a[j] != b[j]) {
                stopCalcStats(j);
                return false;
            }
        stopCalcStats(a.size());
    }
    return true;
}

bool operator!=(const Filter& a, const Filter& b) { return !(a == b); }

Filter operator&&(Filter x, const Filter& y) {
    QL_REQUIRE(!x.initialised() || !y.initialised() || x.size() == y.size(),
               "RandomVariable: x && y: x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    if (x.deterministic() && !x.constantData_)
        return Filter(x.size(), false);
    if (y.deterministic() && !y.constantData_)
        return Filter(y.size(), false);
    if (!x.initialised() || !y.initialised())
        return Filter();
    if (!y.deterministic_)
        x.expand();
    if (x.deterministic_) {
        x.constantData_ = x.constantData_ && y.constantData_;
    } else {
        resumeCalcStats();
        for (Size i = 0; i < x.size(); ++i) {
            x.data_[i] = x.data_[i] && y[i];
        }
        stopCalcStats(x.size());
    }
    return x;
}

Filter operator||(Filter x, const Filter& y) {
    QL_REQUIRE(!x.initialised() || !y.initialised() || x.size() == y.size(),
               "RandomVariable: x || y: x size (" << x.size() << ") must be equal to y size (" << y.size() << ")");
    if (x.deterministic() && x.constantData_)
        return Filter(x.size(), true);
    if (y.deterministic() && y.constantData_)
        return Filter(y.size(), true);
    if (!x.initialised() || !y.initialised())
        return Filter();
    if (!y.deterministic_)
        x.expand();
    if (x.deterministic_) {
        x.constantData_ = x.constantData_ || y.constantData_;
    } else {
        resumeCalcStats();
        for (Size i = 0; i < x.size(); ++i) {
            x.data_[i] = x.data_[i] || y[i];
        }
        stopCalcStats(x.size());
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
    if (x.deterministic_) {
        x.constantData_ = x.constantData_ == y.constantData_;
    } else {
        resumeCalcStats();
        for (Size i = 0; i < x.size(); ++i) {
            x.data_[i] = x.data_[i] == y[i];
        }
        stopCalcStats(x.size());
    }
    return x;
}

Filter operator!(Filter x) {
    if (x.deterministic_)
        x.constantData_ = !x.constantData_;
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.size(); ++i) {
            x.data_[i] = !x.data_[i];
        }
        stopCalcStats(x.size());
    }
    return x;
}

RandomVariable::~RandomVariable() { clear(); }

RandomVariable::RandomVariable()
    : n_(0), constantData_(0.0), data_(nullptr), deterministic_(false), time_(Null<Real>()) {}

RandomVariable::RandomVariable(const RandomVariable& r) {
    n_ = r.n_;
    constantData_ = r.constantData_;
    if (r.data_) {
        resumeDataStats();
        data_ = new double[n_];
        // std::memcpy(data_, r.data_, n_ * sizeof(double));
        std::copy(r.data_, r.data_ + n_, data_);
        stopDataStats(n_);
    } else {
        data_ = nullptr;
    }
    deterministic_ = r.deterministic_;
    time_ = r.time_;
}

RandomVariable::RandomVariable(RandomVariable&& r) {
    n_ = r.n_;
    constantData_ = r.constantData_;
    data_ = r.data_;
    r.data_ = nullptr;
    deterministic_ = r.deterministic_;
    time_ = r.time_;
}

RandomVariable& RandomVariable::operator=(const RandomVariable& r) {
    if (r.deterministic_) {
        deterministic_ = true;
        if (data_) {
            delete[] data_;
            data_ = nullptr;
        }
    } else {
        deterministic_ = false;
        if (r.n_ != 0) {
            resumeDataStats();
            if (n_ != r.n_) {
                if (data_)
                    delete[] data_;
                data_ = new double[r.n_];
            }
            // std::memcpy(data_, r.data_, r.n_ * sizeof(double));
            std::copy(r.data_, r.data_ + r.n_, data_);
            stopDataStats(r.n_);
        } else {
            if (data_) {
                delete[] data_;
                data_ = nullptr;
            }
        }
    }
    n_ = r.n_;
    constantData_ = r.constantData_;
    time_ = r.time_;
    return *this;
}

RandomVariable& RandomVariable::operator=(RandomVariable&& r) {
    n_ = r.n_;
    constantData_ = r.constantData_;
    if (data_) {
        delete[] data_;
    }
    data_ = r.data_;
    r.data_ = nullptr;
    deterministic_ = r.deterministic_;
    time_ = r.time_;
    return *this;
}

RandomVariable::RandomVariable(const Size n, const Real value, const Real time)
    : n_(n), constantData_(value), data_(nullptr), deterministic_(n != 0), time_(time) {}

RandomVariable::RandomVariable(const Filter& f, const Real valueTrue, const Real valueFalse, const Real time) {
    if (!f.initialised()) {
        data_ = nullptr;
        clear();
        return;
    }
    n_ = f.size();
    if (f.deterministic()) {
        data_ = nullptr;
        setAll(f.at(0) ? valueTrue : valueFalse);
    } else {
        resumeDataStats();
        constantData_ = 0.0;
        deterministic_ = false;
        data_ = new double[n_];
        for (Size i = 0; i < n_; ++i)
            set(i, f[i] ? valueTrue : valueFalse);
        stopDataStats(n_);
    }
    time_ = time;
}

RandomVariable::RandomVariable(const Size n, const Real* const data, const Real time) {
    n_ = n;
    deterministic_ = false;
    time_ = time;
    if (n_ != 0) {
        resumeDataStats();
        data_ = new double[n_];
        // std::memcpy(data_, array.begin(), n_ * sizeof(double));
        std::copy(data, data + n_, data_);
        stopDataStats(n_);
    } else {
        data_ = nullptr;
    }
    constantData_ = 0.0;
}

RandomVariable::RandomVariable(const std::vector<Real>& data, const Real time)
    : RandomVariable(data.size(), &data[0], time) {}

RandomVariable::RandomVariable(const QuantLib::Array& data, const Real time)
    : RandomVariable(data.size(), data.begin(), time) {}

void RandomVariable::copyToMatrixCol(QuantLib::Matrix& m, const Size j) const {
    if (deterministic_)
        std::fill(m.column_begin(j), std::next(m.column_end(j), n_), constantData_);
    else if (n_ != 0) {
        resumeDataStats();
        std::copy(data_, data_ + n_, m.column_begin(j));
        stopDataStats(n_);
    }
}

void RandomVariable::copyToArray(QuantLib::Array& array) const {
    if (deterministic_)
        std::fill(array.begin(), array.end(), constantData_);
    else if (n_ != 0) {
        resumeDataStats();
        // std::memcpy(array.begin(), data_, n_ * sizeof(double));
        std::copy(data_, data_ + n_, array.begin());
        stopDataStats(n_);
    }
}

void RandomVariable::clear() {
    n_ = 0;
    constantData_ = 0.0;
    if (data_) {
        delete[] data_;
        data_ = nullptr;
    }
    deterministic_ = false;
    time_ = Null<Real>();
}

void RandomVariable::updateDeterministic() {
    if (deterministic_ || !initialised())
        return;
    for (Size i = 0; i < n_; ++i) {
        resumeCalcStats();
        if (!QuantLib::close_enough(data_[i], constantData_)) {
            stopCalcStats(i);
            return;
        }
    }
    setAll(constantData_);
    stopCalcStats(n_);
}

void RandomVariable::setAll(const Real v) {
    QL_REQUIRE(n_ > 0, "RandomVariable::setAll(): dimension is zero");
    if (data_) {
        delete[] data_;
        data_ = nullptr;
    }
    constantData_ = v;
    deterministic_ = true;
}

void RandomVariable::resetSize(const Size n) {
    QL_REQUIRE(deterministic_, "RandomVariable::resetSize(): only possible for deterministic variables.");
    n_ = n;
}

void RandomVariable::expand() {
    if (!deterministic_)
        return;
    deterministic_ = false;
    resumeDataStats();
    data_ = new double[n_];
    std::fill(data_, data_ + n_, constantData_);
    stopDataStats(n_);
}

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
    if (a.deterministic_ && b.deterministic_) {
        return a.constantData_ == b.constantData_;
    } else {
        resumeCalcStats();
        for (Size j = 0; j < a.size(); ++j)
            if (a[j] != b[j]) {
                stopCalcStats(j);
                return false;
            }
    }
    return QuantLib::close_enough(a.time(), b.time());
}

bool operator!=(const RandomVariable& a, const RandomVariable b) { return !(a == b); }

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
    else if (QuantLib::close_enough(y.constantData_, 0.0))
        return *this;
    if (deterministic_)
        constantData_ += y.constantData_;
    else {
        resumeCalcStats();
        for (Size i = 0; i < n_; ++i) {
            data_[i] += y[i];
        }
        stopCalcStats(n_);
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
    else if (QuantLib::close_enough(y.constantData_, 0.0))
        return *this;
    if (deterministic_)
        constantData_ -= y.constantData_;
    else {
        resumeCalcStats();
        for (Size i = 0; i < n_; ++i) {
            data_[i] -= y[i];
        }
        stopCalcStats(n_);
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
    else if (QuantLib::close_enough(y.constantData_, 1.0))
        return *this;
    if (deterministic_)
        constantData_ *= y.constantData_;
    else {
        resumeCalcStats();
        for (Size i = 0; i < n_; ++i) {
            data_[i] *= y[i];
        }
        stopCalcStats(n_);
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
    else if (QuantLib::close_enough(y.constantData_, 1.0))
        return *this;
    if (deterministic_)
        constantData_ /= y.constantData_;
    else {
        resumeCalcStats();
        for (Size i = 0; i < n_; ++i) {
            data_[i] /= y[i];
        }
        stopCalcStats(n_);
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
    if (x.deterministic())
        x.constantData_ = std::max(x.constantData_, y.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.size(); ++i) {
            x.data_[i] = std::max(x.data_[i], y[i]);
        }
        stopCalcStats(x.size());
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
    if (x.deterministic())
        x.constantData_ = std::min(x.constantData_, y.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.size(); ++i) {
            x.data_[i] = std::min(x.data_[i], y[i]);
        }
        stopCalcStats(x.size());
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
    else if (QuantLib::close_enough(y.constantData_, 1.0))
        return x;
    if (x.deterministic())
        x.constantData_ = std::pow(x.constantData_, y.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.size(); ++i) {
            x.data_[i] = std::pow(x.data_[i], y[i]);
        }
        stopCalcStats(x.size());
    }
    return x;
}

RandomVariable operator-(RandomVariable x) {
    if (x.deterministic_)
        x.constantData_ = -x.constantData_;
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = -x.data_[i];
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable abs(RandomVariable x) {
    if (x.deterministic_)
        x.constantData_ = std::abs(x.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = std::abs(x.data_[i]);
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable exp(RandomVariable x) {
    if (x.deterministic_)
        x.constantData_ = std::exp(x.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = std::exp(x.data_[i]);
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable log(RandomVariable x) {
    if (x.deterministic_)
        x.constantData_ = std::log(x.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = std::log(x.data_[i]);
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable sqrt(RandomVariable x) {
    if (x.deterministic_)
        x.constantData_ = std::sqrt(x.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = std::sqrt(x.data_[i]);
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable sin(RandomVariable x) {
    if (x.deterministic_)
        x.constantData_ = std::sin(x.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = std::sin(x.data_[i]);
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable cos(RandomVariable x) {
    if (x.deterministic_)
        x.constantData_ = std::cos(x.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = std::cos(x.data_[i]);
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable normalCdf(RandomVariable x) {
    static const boost::math::normal_distribution<double> n;
    if (x.deterministic_)
        x.constantData_ = boost::math::cdf(n, x.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = boost::math::cdf(n, x.data_[i]);
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable normalPdf(RandomVariable x) {
    static const boost::math::normal_distribution<double> n;
    if (x.deterministic_)
        x.constantData_ = boost::math::pdf(n, x.constantData_);
    else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = boost::math::pdf(n, x.data_[i]);
        }
        stopCalcStats(x.n_);
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
        return Filter(x.size(), QuantLib::close_enough(x.constantData_, y.constantData_));
    }
    resumeCalcStats();
    Filter result(x.size(), false);
    for (Size i = 0; i < x.size(); ++i) {
        result.set(i, QuantLib::close_enough(x[i], y[i]));
    }
    stopCalcStats(x.size());
    return result;
}

bool close_enough_all(const RandomVariable& x, const RandomVariable& y) {
    QL_REQUIRE(x.size() == y.size(), "RandomVariable: close_enough_all(x,y): x size ("
                                         << x.size() << ") must be equal to y size (" << y.size() << ")");
    checkTimeConsistency(x, y);
    if (x.deterministic_ && y.deterministic_) {
        return QuantLib::close_enough(x.constantData_, y.constantData_);
    }
    resumeCalcStats();
    for (Size i = 0; i < x.size(); ++i) {
        if (!QuantLib::close_enough(x[i], y[i]))
            return false;
    }
    stopCalcStats(x.size());
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
    resumeCalcStats();
    x.expand();
    for (Size i = 0; i < f.size(); ++i) {
        if (!f[i])
            x.set(i, y[i]);
    }
    stopCalcStats(f.size());
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
    if (x.deterministic()) {
        x.constantData_ = QuantLib::close_enough(x.constantData_, y.constantData_) ? trueVal : falseVal;
    } else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = QuantLib::close_enough(x.data_[i], y[i]) ? trueVal : falseVal;
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable indicatorGt(RandomVariable x, const RandomVariable& y, const Real trueVal, const Real falseVal,
                           const Real eps) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    QL_REQUIRE(x.size() == y.size(), "RandomVariable: indicatorEq(x,y): x size ("
                                         << x.size() << ") must be equal to y size (" << y.size() << ")");
    x.checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        x.expand();
    if (eps != 0.0) {
        Real delta = getDelta(x - y, eps);
        if (!QuantLib::close_enough(delta, 0.0)) {
            // logistic function
            x.expand();
            Real delta = getDelta(x - y, eps);
            resumeCalcStats();
            for (Size i = 0; i < x.n_; ++i) {
                x.data_[i] = falseVal + (trueVal - falseVal) * 1.0 / (1.0 + std::exp(-x.data_[i] / delta));
            }
            stopCalcStats(x.n_);
            return x;
        }
    }
    // eps == 0.0 or delta == 0.0
    if (x.deterministic()) {
        x.constantData_ =
            (x.constantData_ > y.constantData_ && !QuantLib::close_enough(x.constantData_, y.constantData_)) ? trueVal
                                                                                                             : falseVal;
    } else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = (x.data_[i] > y[i] && !QuantLib::close_enough(x.data_[i], y[i])) ? trueVal : falseVal;
        }
        stopCalcStats(x.n_);
    }
    return x;
}

RandomVariable indicatorGeq(RandomVariable x, const RandomVariable& y, const Real trueVal, const Real falseVal,
                            const Real eps) {
    if (!x.initialised() || !y.initialised())
        return RandomVariable();
    QL_REQUIRE(x.size() == y.size(), "RandomVariable: indicatorEq(x,y): x size ("
                                         << x.size() << ") must be equal to y size (" << y.size() << ")");
    x.checkTimeConsistencyAndUpdate(y.time());
    if (!y.deterministic_)
        x.expand();
    if (eps != 0.0) {
        Real delta = getDelta(x - y, eps);
        if (!QuantLib::close_enough(delta, 0.0)) {
            // logistic function
            x.expand();
            Real delta = getDelta(x - y, eps);
            resumeCalcStats();
            for (Size i = 0; i < x.n_; ++i) {
                x.data_[i] = falseVal + (trueVal - falseVal) * 1.0 / (1.0 + std::exp(-x.data_[i] / delta));
            }
            stopCalcStats(x.n_);
            return x;
        }
    }
    // eps == 0.0 or delta == 0.0
    if (x.deterministic()) {
        x.constantData_ =
            (x.constantData_ > y.constantData_ || QuantLib::close_enough(x.constantData_, y.constantData_)) ? trueVal
                                                                                                            : falseVal;
    } else {
        resumeCalcStats();
        for (Size i = 0; i < x.n_; ++i) {
            x.data_[i] = (x.data_[i] > y[i] || QuantLib::close_enough(x.data_[i], y[i])) ? trueVal : falseVal;
        }
        stopCalcStats(x.n_);
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
                      x.constantData_ < y.constantData_ && !QuantLib::close_enough(x.constantData_, y.constantData_));
    }
    resumeCalcStats();
    Filter result(x.size(), false);
    for (Size i = 0; i < x.size(); ++i) {
        result.set(i, x[i] < y[i] && !QuantLib::close_enough(x[i], y[i]));
    }
    stopCalcStats(x.size());
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
                      x.constantData_ < y.constantData_ || QuantLib::close_enough(x.constantData_, y.constantData_));
    }
    resumeCalcStats();
    Filter result(x.size(), false);
    for (Size i = 0; i < x.size(); ++i) {
        result.set(i, x[i] < y[i] || QuantLib::close_enough(x[i], y[i]));
    }
    stopCalcStats(x.size());
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
                      x.constantData_ > y.constantData_ && !QuantLib::close_enough(x.constantData_, y.constantData_));
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
                      x.constantData_ > y.constantData_ || QuantLib::close_enough(x.constantData_, y.constantData_));
    }
    resumeCalcStats();
    Filter result(x.size(), false);
    for (Size i = 0; i < x.size(); ++i) {
        result.set(i, x[i] > y[i] || QuantLib::close_enough(x[i], y[i]));
    }
    stopCalcStats(x.size());
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
    if (x.deterministic_ && QuantLib::close_enough(x.constantData_, 0.0))
        return x;
    resumeCalcStats();
    for (Size i = 0; i < x.size(); ++i) {
        if (!f[i])
            x.set(i, 0.0);
    }
    stopCalcStats(x.size());
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
    if (x.deterministic_ && QuantLib::close_enough(x.constantData_, 0.0))
        return x;
    resumeCalcStats();
    for (Size i = 0; i < x.size(); ++i) {
        if (f[i])
            x.set(i, 0.0);
    }
    stopCalcStats(x.size());
    return x;
}

Matrix pcaCoordinateTransform(const std::vector<const RandomVariable*>& regressor, const Real varianceCutoff) {
    if (regressor.empty())
        return {};

    Matrix cov(regressor.size(), regressor.size());
    for (Size i = 0; i < regressor.size(); ++i) {
        cov(i, i) = variance(*regressor[i]).at(0);
        for (Size j = 0; j < i; ++j) {
            cov(i, j) = cov(j, i) = covariance(*regressor[i], *regressor[j]).at(0);
        }
    }

    QuantLib::SymmetricSchurDecomposition schur(cov);
    Real totalVariance = std::accumulate(schur.eigenvalues().begin(), schur.eigenvalues().end(), 0.0);

    Size keep = 0;
    Real explainedVariance = 0.0;
    while (keep < schur.eigenvalues().size() && explainedVariance < totalVariance * (1.0 - varianceCutoff)) {
        explainedVariance += schur.eigenvalues()[keep++];
    }

    Matrix result(keep, regressor.size());
    for (Size i = 0; i < keep; ++i) {
        std::copy(schur.eigenvectors().column_begin(i), schur.eigenvectors().column_end(i), result.row_begin(i));
    }
    return result;
}

std::vector<RandomVariable> applyCoordinateTransform(const std::vector<const RandomVariable*>& regressor,
                                                     const Matrix& transform) {
    QL_REQUIRE(transform.columns() == regressor.size(),
               "applyCoordinateTransform(): number of random variables ("
                   << regressor.size() << ") does not match number of columns in transform (" << transform.columns());
    if (regressor.empty())
        return {};
    Size n = regressor.front()->size();
    std::vector<RandomVariable> result(transform.rows(), RandomVariable(n, 0.0));
    for (Size i = 0; i < transform.rows(); ++i) {
        for (Size j = 0; j < transform.columns(); ++j) {
            result[i] += RandomVariable(n, transform(i, j)) * (*regressor[j]);
        }
    }
    return result;
}

std::vector<const RandomVariable*> vec2vecptr(const std::vector<RandomVariable>& values) {
    std::vector<const RandomVariable*> result(values.size());
    std::transform(values.begin(), values.end(), result.begin(), [](const RandomVariable& v) { return &v; });
    return result;
}

Array regressionCoefficients(
    RandomVariable r, std::vector<const RandomVariable*> regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Filter& filter, const RandomVariableRegressionMethod regressionMethod, const std::string& debugLabel) {

    for (auto const reg : regressor) {
        QL_REQUIRE(reg->size() == r.size(),
                   "regressor size (" << reg->size() << ") must match regressand size (" << r.size() << ")");
    }

    QL_REQUIRE(filter.size() == 0 || filter.size() == r.size(),
               "filter size (" << filter.size() << ") must match regressand size (" << r.size() << ")");

    QL_REQUIRE(r.size() >= basisFn.size(), "regressionCoefficients(): sample size ("
                                               << r.size() << ") must be geq basis fns size (" << basisFn.size()
                                               << ")");

    resumeCalcStats();

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

    if (!debugLabel.empty()) {
        for (Size i = 0; i < r.size(); ++i) {
            std::cout << debugLabel << "," << r[i] << ",";
            for (Size j = 0; j < regressor.size(); ++j) {
                std::cout << regressor[j]->operator[](i) << (j == regressor.size() - 1 ? "\n" : ",");
            }
        }
        std::cout << std::flush;
    }

    if (filter.size() > 0) {
        r = applyFilter(r, filter);
    }

    Array b(r.size());
    if (r.deterministic())
        std::fill(b.begin(), b.end(), r[0]);
    else
        r.copyToArray(b);

    Array res;
    if (regressionMethod == RandomVariableRegressionMethod::SVD) {
        SVD svd(A);
        const Matrix& V = svd.V();
        const Matrix& U = svd.U();
        const Array& w = svd.singularValues();
        Real threshold = r.size() * QL_EPSILON * svd.singularValues()[0];
        res = Array(basisFn.size(), 0.0);
        for (Size i = 0; i < basisFn.size(); ++i) {
            if (w[i] > threshold) {
                Real u = std::inner_product(U.column_begin(i), U.column_end(i), b.begin(), Real(0.0)) / w[i];
                for (Size j = 0; j < basisFn.size(); ++j) {
                    res[j] += u * V[j][i];
                }
            }
        }
    } else if (regressionMethod == RandomVariableRegressionMethod::QR) {
        res = qrSolve(A, b);
    } else {
        QL_FAIL("regressionCoefficients(): unknown regression method, expected SVD or QR");
    }

    // rough estimate, SVD is O(mn min(m,n))
    stopCalcStats(r.size() * basisFn.size() * std::min(r.size(), basisFn.size()));
    return res;
}

RandomVariable conditionalExpectation(
    const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Array& coefficients) {
    QL_REQUIRE(!regressor.empty(), "regressor vector is empty");
    Size n = regressor.front()->size();
    for (Size i = 1; i < regressor.size(); ++i) {
        QL_REQUIRE(regressor[i] != nullptr, "regressor #" << i << " is null.");
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
    const Filter& filter, const RandomVariableRegressionMethod regressionMethod) {
    if (r.deterministic())
        return r;
    auto coeff = regressionCoefficients(r, regressor, basisFn, filter, regressionMethod);
    return conditionalExpectation(regressor, basisFn, coeff);
}

RandomVariable expectation(const RandomVariable& r) {
    if (r.deterministic())
        return r;
    resumeCalcStats();
    boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::mean>> acc;
    for (Size i = 0; i < r.size(); ++i)
        acc(r[i]);
    return RandomVariable(r.size(), boost::accumulators::mean(acc));
}

RandomVariable variance(const RandomVariable& r) {
    if (r.deterministic())
        return RandomVariable(r.size(), 0.0);
    resumeCalcStats();
    boost::accumulators::accumulator_set<double, boost::accumulators::stats<boost::accumulators::tag::variance>> acc;
    for (Size i = 0; i < r.size(); ++i) {
        acc(r[i]);
    }
    stopCalcStats(r.size());
    return RandomVariable(r.size(), boost::accumulators::variance(acc));
}

RandomVariable covariance(const RandomVariable& r, const RandomVariable& s) {
    QL_REQUIRE(r.size() == s.size(), "covariance(RandomVariable r, RandomVariable s): inconsistent sizes r ("
                                         << r.size() << "), s(" << s.size() << ")");
    if (r.deterministic() || s.deterministic())
        return RandomVariable(r.size(), 0.0);
    resumeCalcStats();
    boost::accumulators::accumulator_set<
        double,
        boost::accumulators::stats<boost::accumulators::tag::covariance<double, boost::accumulators::tag::covariate1>>>
        acc;
    for (Size i = 0; i < r.size(); ++i) {
        acc(r[i], boost::accumulators::covariate1 = s[i]);
    }
    stopCalcStats(r.size());
    return RandomVariable(r.size(), boost::accumulators::covariance(acc));
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

    resumeCalcStats();

    Real delta = getDelta(x, eps);

    if (QuantLib::close_enough(delta, 0.0))
        return tmp;

    // compute derivative

    for (Size i = 0; i < tmp.size(); ++i) {

        // linear approximation of step
        // Real ax = std::abs(x[i]);
        // if (ax < delta) {
        //     tmp.set(i, 1.0 / (2.0 * delta));
        // }

        // logistic function
        // f(x)  = 1 / (1 + exp(-x / delta))
        // f'(x) = exp(-x/delta) / (delta * (1 + exp(-x / delta))^2), this is an even function
        tmp.set(i, std::exp(-1.0 / delta * x[i]) / (delta * std::pow(1.0 + std::exp(-1.0 / delta * x[i]), 2.0)));
    }

    stopCalcStats(x.size() * 8);

    return tmp;
}

std::function<void(RandomVariable&)> RandomVariable::deleter =
    std::function<void(RandomVariable&)>([](RandomVariable& x) { x.clear(); });

std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>
multiPathBasisSystem(Size dim, Size order, QuantLib::LsmBasisSystem::PolynomialType type, Size basisSystemSizeBound) {
    thread_local static std::map<std::pair<Size, Size>,
                                 std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>>
        cache;
    QL_REQUIRE(order > 0, "multiPathBasisSystem: order must be > 0");
    if (basisSystemSizeBound != Null<Size>()) {
        while (RandomVariableLsmBasisSystem::size(dim, order) > static_cast<Real>(basisSystemSizeBound) && order > 1) {
            --order;
        }
    }
    if (auto c = cache.find(std::make_pair(dim, order)); c != cache.end())
        return c->second;
    auto tmp = RandomVariableLsmBasisSystem::multiPathBasisSystem(dim, order, type);
    cache[std::make_pair(dim, order)] = tmp;
    return tmp;
}

} // namespace QuantExt
