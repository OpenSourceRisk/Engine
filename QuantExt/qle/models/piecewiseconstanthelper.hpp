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

/*! \file piecewiseconstanthelper.hpp
    \brief helper classes for piecewise constant parametrizations
    \ingroup models
*/

#ifndef quantext_piecewiseconstant_helper_hpp
#define quantext_piecewiseconstant_helper_hpp

#include <qle/models/pseudoparameter.hpp>

#include <ql/experimental/math/piecewisefunction.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/time/date.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Piecewise Constant Helper 1
/*! \ingroup models
 */
class PiecewiseConstantHelper1 {
public:
    PiecewiseConstantHelper1(const Array& t);
    PiecewiseConstantHelper1(const std::vector<Date>& dates, const Handle<YieldTermStructure>& yts);
    const Array& t() const;
    const boost::shared_ptr<Parameter> p() const;
    void update() const;
    /*! this returns the transformed value */
    Real y(const Time t) const;
    //! int_0^t y^2(s) ds
    Real int_y_sqr(const Time t) const;

    Real direct(const Real x) const;
    Real inverse(const Real y) const;

protected:
    const Array t_;
    /*! y are the raw values in the sense of parameter transformation */
    const boost::shared_ptr<PseudoParameter> y_;

private:
    mutable std::vector<Real> b_;
};

//! Piecewise Constant Helper 11
/*! this is PiecewiseConstantHelper1 with two sets of (t,y)
    \ingroup models
*/
class PiecewiseConstantHelper11 {
public:
    /*! y are the raw values in the sense of parameter transformation */
    PiecewiseConstantHelper11(const Array& t1, const Array& t2);
    PiecewiseConstantHelper11(const std::vector<Date>& dates1, const std::vector<Date>& dates2,
                              const Handle<YieldTermStructure>& yts);
    const PiecewiseConstantHelper1& helper1() const;
    const PiecewiseConstantHelper1& helper2() const;

private:
    const PiecewiseConstantHelper1 h1_, h2_;
};

//! Piecewise Constant Helper2
/*! \ingroup models
 */
class PiecewiseConstantHelper2 {
public:
    PiecewiseConstantHelper2(const Array& t);
    PiecewiseConstantHelper2(const std::vector<Date>& dates, const Handle<YieldTermStructure>& yts);
    const Array& t() const;
    const boost::shared_ptr<Parameter> p() const;
    void update() const;
    /*! this returns the transformed value */
    Real y(const Time t) const;
    //! exp(int_0^t -y(s)) ds
    Real exp_m_int_y(const Time t) const;
    //! int_0^t exp(int_0^s -y(u) du) ds
    Real int_exp_m_int_y(const Time t) const;

    Real direct(const Real x) const;
    Real inverse(const Real y) const;

private:
    const Real zeroCutoff_;

protected:
    const Array t_;
    /*! y are the raw values in the sense of parameter transformation */
    const boost::shared_ptr<PseudoParameter> y_;

private:
    mutable std::vector<Real> b_, c_;
};

//! Piecewise Constant Helper 3
/*! \ingroup models
 */
class PiecewiseConstantHelper3 {
public:
    PiecewiseConstantHelper3(const Array& t1, const Array& t2);
    PiecewiseConstantHelper3(const std::vector<Date>& dates1, const std::vector<Date>& dates2,
                             const Handle<YieldTermStructure>& yts);
    const Array& t1() const;
    const Array& t2() const;
    const Array& tUnion() const;
    const boost::shared_ptr<Parameter> p1() const;
    const boost::shared_ptr<Parameter> p2() const;
    /* update is required after construction for helper 3 ! */
    void update() const;
    /*! this returns the transformed value */
    Real y1(const Time t) const;
    /*! this returns the transformed value */
    Real y2(const Time t) const;
    //! int_0^t y1^2(s) exp(2*int_0^s y2(u) du) ds
    Real int_y1_sqr_exp_2_int_y2(const Time t) const;

    Real direct1(const Real x) const;
    Real inverse1(const Real y) const;
    Real direct2(const Real x) const;
    Real inverse2(const Real y) const;

private:
    const Real zeroCutoff_;

protected:
    const Array t1_, t2_;
    mutable Array tUnion_;
    /*! y1, y2 are the raw values in the sense of parameter transformation */
    const boost::shared_ptr<PseudoParameter> y1_, y2_;
    mutable Array y1Union_, y2Union_;

private:
    mutable std::vector<Real> b_, c_;
};

// inline

inline const Array& PiecewiseConstantHelper1::t() const { return t_; }

inline const boost::shared_ptr<Parameter> PiecewiseConstantHelper1::p() const { return y_; }

inline Real PiecewiseConstantHelper1::direct(const Real x) const { return x * x; }

inline Real PiecewiseConstantHelper1::inverse(const Real y) const { return std::sqrt(y); }

inline void PiecewiseConstantHelper1::update() const {
    Real sum = 0.0;
    b_.resize(t_.size());
    for (Size i = 0; i < t_.size(); ++i) {
        sum += direct(y_->params()[i]) * direct(y_->params()[i]) * (t_[i] - (i == 0 ? 0.0 : t_[i - 1]));
        b_[i] = sum;
    }
}

inline const PiecewiseConstantHelper1& PiecewiseConstantHelper11::helper1() const { return h1_; }

inline const PiecewiseConstantHelper1& PiecewiseConstantHelper11::helper2() const { return h2_; }

inline const Array& PiecewiseConstantHelper2::t() const { return t_; }

inline const boost::shared_ptr<Parameter> PiecewiseConstantHelper2::p() const { return y_; }

inline Real PiecewiseConstantHelper2::direct(const Real x) const { return x; }

inline Real PiecewiseConstantHelper2::inverse(const Real y) const { return y; }

inline void PiecewiseConstantHelper2::update() const {
    Real sum = 0.0, sum2 = 0.0;
    b_.resize(t_.size());
    c_.resize(t_.size());
    for (Size i = 0; i < t_.size(); ++i) {
        Real t0 = (i == 0 ? 0.0 : t_[i - 1]);
        sum += direct(y_->params()[i]) * (t_[i] - t0);
        b_[i] = sum;
        Real b2Tmp = (i == 0 ? 0.0 : b_[i - 1]);
        if (std::fabs(direct(y_->params()[i])) < zeroCutoff_) {
            sum2 += (t_[i] - t0) * std::exp(-b2Tmp);
        } else {
            sum2 += (std::exp(-b2Tmp) - std::exp(-b2Tmp - direct(y_->params()[i]) * (t_[i] - t0))) /
                    direct(y_->params()[i]);
        }
        c_[i] = sum2;
    }
}

inline const Array& PiecewiseConstantHelper3::t1() const { return t1_; }
inline const Array& PiecewiseConstantHelper3::t2() const { return t2_; }
inline const Array& PiecewiseConstantHelper3::tUnion() const { return tUnion_; }

inline const boost::shared_ptr<Parameter> PiecewiseConstantHelper3::p1() const { return y1_; }

inline const boost::shared_ptr<Parameter> PiecewiseConstantHelper3::p2() const { return y2_; }

inline Real PiecewiseConstantHelper3::direct1(const Real x) const { return x * x; }

inline Real PiecewiseConstantHelper3::inverse1(const Real y) const { return std::sqrt(y); }

inline Real PiecewiseConstantHelper3::direct2(const Real x) const { return x; }

inline Real PiecewiseConstantHelper3::inverse2(const Real y) const { return y; }

inline void PiecewiseConstantHelper3::update() const {
    std::vector<Real> tTmp(t1_.begin(), t1_.end());
    tTmp.insert(tTmp.end(), t2_.begin(), t2_.end());
    std::sort(tTmp.begin(), tTmp.end());
    std::vector<Real>::const_iterator end = std::unique(tTmp.begin(), tTmp.end(), std::ptr_fun(close_enough));
    tTmp.resize(end - tTmp.begin());
    tUnion_ = Array(tTmp.begin(), tTmp.end());
    y1Union_ = Array(tUnion_.size() + 1);
    y2Union_ = Array(tUnion_.size() + 1);
    for (Size i = 0; i < tUnion_.size() + 1; ++i) {
        // choose a safe t for y1 and y2 evaluation
        Real t = (i == tUnion_.size() ? (tUnion_.size() == 0 ? 1.0 : tUnion_.back() + 1.0)
                                      : (0.5 * (tUnion_[i] + (i > 0 ? tUnion_[i - 1] : 0.0))));
        y1Union_[i] = QL_PIECEWISE_FUNCTION(t1_, y1_->params(), t);
        y2Union_[i] = QL_PIECEWISE_FUNCTION(t2_, y2_->params(), t);
    }
    Real sum = 0.0, sum2 = 0.0;
    b_.resize(tUnion_.size());
    c_.resize(tUnion_.size());
    for (Size i = 0; i < tUnion_.size(); ++i) {
        Real t0 = (i == 0 ? 0.0 : tUnion_[i - 1]);
        sum += direct2(y2Union_[i]) * (tUnion_[i] - t0);
        b_[i] = sum;
        Real b2Tmp = (i == 0 ? 0.0 : b_[i - 1]);
        if (std::fabs(direct2(y2Union_[i])) < zeroCutoff_) {
            sum2 += direct1(y1Union_[i]) * direct1(y1Union_[i]) * (tUnion_[i] - t0) * std::exp(2.0 * b2Tmp);
        } else {
            sum2 += direct1(y1Union_[i]) * direct1(y1Union_[i]) *
                    (std::exp(2.0 * b2Tmp + 2.0 * direct2(y2Union_[i]) * (tUnion_[i] - t0)) - std::exp(2.0 * b2Tmp)) /
                    (2.0 * direct2(y2Union_[i]));
        }
        c_[i] = sum2;
    }
}

inline Real PiecewiseConstantHelper1::y(const Time t) const {
    return direct(QL_PIECEWISE_FUNCTION(t_, y_->params(), t));
}

inline Real PiecewiseConstantHelper2::y(const Time t) const {
    return direct(QL_PIECEWISE_FUNCTION(t_, y_->params(), t));
}

inline Real PiecewiseConstantHelper3::y1(const Time t) const {
    return direct1(QL_PIECEWISE_FUNCTION(t1_, y1_->params(), t));
}

inline Real PiecewiseConstantHelper3::y2(const Time t) const {
    return direct2(QL_PIECEWISE_FUNCTION(t2_, y2_->params(), t));
}

inline Real PiecewiseConstantHelper1::int_y_sqr(const Time t) const {
    if (t < 0.0)
        return 0.0;
    Size i = std::upper_bound(t_.begin(), t_.end(), t) - t_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += b_[std::min(i - 1, b_.size() - 1)];
    Real a = direct(y_->params()[std::min(i, y_->size() - 1)]);
    res += a * a * (t - (i == 0 ? 0.0 : t_[i - 1]));
    return res;
}

inline Real PiecewiseConstantHelper2::exp_m_int_y(const Time t) const {
    if (t < 0.0)
        return 1.0;
    Size i = std::upper_bound(t_.begin(), t_.end(), t) - t_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += b_[std::min(i - 1, b_.size() - 1)];
    Real a = y_->params()[std::min(i, y_->size() - 1)];
    res += a * (t - (i == 0 ? 0.0 : t_[i - 1]));
    return std::exp(-res);
}

inline Real PiecewiseConstantHelper2::int_exp_m_int_y(const Time t) const {
    if (t < 0.0)
        return 0.0;
    Size i = std::upper_bound(t_.begin(), t_.end(), t) - t_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += c_[std::min(i - 1, c_.size() - 1)];
    Real a = direct(y_->params()[std::min(i, y_->size() - 1)]);
    Real t0 = (i == 0 ? 0.0 : t_[i - 1]);
    Real b2Tmp = (i == 0 ? 0.0 : b_[i - 1]);
    if (std::fabs(a) < zeroCutoff_) {
        res += std::exp(-b2Tmp) * (t - t0);
    } else {
        res += (std::exp(-b2Tmp) - std::exp(-b2Tmp - a * (t - t0))) / a;
    }
    return res;
}

inline Real PiecewiseConstantHelper3::int_y1_sqr_exp_2_int_y2(const Time t) const {
    if (t < 0.0)
        return 0.0;
    Size i = std::upper_bound(tUnion_.begin(), tUnion_.end(), t) - tUnion_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += c_[std::min(i - 1, c_.size() - 1)];
    Real a = direct2(y2Union_[std::min(i, y2Union_.size() - 1)]);
    Real b = direct1(y1Union_[std::min(i, y1Union_.size() - 1)]);
    Real t0 = (i == 0 ? 0.0 : tUnion_[i - 1]);
    Real b2Tmp = (i == 0 ? 0.0 : b_[i - 1]);
    if (std::fabs(a) < zeroCutoff_) {
        res += b * b * std::exp(2.0 * b2Tmp) * (t - t0);
    } else {
        res += b * b * (std::exp(2.0 * b2Tmp + 2.0 * a * (t - t0)) - std::exp(2.0 * b2Tmp)) / (2.0 * a);
    }
    return res;
}

} // namespace QuantExt

#endif
