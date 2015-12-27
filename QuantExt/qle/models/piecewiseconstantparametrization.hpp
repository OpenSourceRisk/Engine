/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file piecewiseconstantparametrization.hpp
    \brief generic piecewise constant parametrization providing some
           useful integrals in closed form
*/

#ifndef quantext_piecewiseconstant_parametrization_hpp
#define quantext_piecewiseconstant_parametrization_hpp

#include <qle/models/parametrization.hpp>
#include <qle/math/piecewisefunction.hpp>

namespace QuantExt {

class PiecewiseConstantParametrization : Parametrization {
  protected:
    PiecewiseConstantParametrization(const Currency &currency, const Array &t1,
                                     const Array &y1, const Array &t2,
                                     const Array &y2);
    void update() const;
    Real y1(const Time t) const;
    Real y2(const Time t) const;
    //! int_0^t y1^2(s) ds
    Real int_y1_sqr(const Time t) const;
    //! int_0^t -y2(s) ds
    Real exp_m_int_y2(const Time t) const;
    //! int_0^t int_0^s -y2(u) du ds
    Real int_exp_m_int_y2(const Time t) const;

  private:
    const Real zeroCutoff_;
    const Array &t1_, &y1_, &t2_, &y2_;
    mutable std::vector<Real> b1_, b2_, c2_;
    bool compute1_, compute2_;
};

// inline

inline void PiecewiseConstantParametrization::update() const {

    if (compute1_) {
        Real sum = 0.0;
        b1_.resize(t1_.size());
        for (Size i = 0; i < t1_.size(); ++i) {
            sum += y1_[i] * y1_[i] * (t1_[i] - (i == 0 ? 0.0 : t1_[i - 1]));
            b1_[i] = sum;
        }
    }

    if (compute2_) {
        Real sum = 0.0, sum2 = 0.0;
        b2_.resize(t2_.size());
        c2_.resize(t2_.size());
        for (Size i = 0; i < t2_.size(); ++i) {
            Real t0 = (i == 0 ? 0.0 : t2_[i - 1]);
            sum += y2_[i] * (t2_[i] - t0);
            b2_[i] = sum;
            Real b2Tmp = (i == 0 ? 0.0 : b2_[i - 1]);
            if (std::fabs(y2_[i]) < zeroCutoff_) {
                sum2 += (t2_[i] - t0) * std::exp(-b2Tmp);
            } else {
                sum2 += (std::exp(-b2Tmp - y2_[i] * (t2_[i] - t0)) -
                         std::exp(-b2Tmp)) /
                        y2_[i];
            }
            c2_[i] = sum2;
        }
    }
}

inline Real PiecewiseConstantParametrization::y1(const Time t) const {
    return QL_PIECEWISE_FUNCTION(t1_, y1_, t);
}

inline Real PiecewiseConstantParametrization::y2(const Time t) const {
    return QL_PIECEWISE_FUNCTION(t2_, y2_, t);
}

inline Real PiecewiseConstantParametrization::int_y1_sqr(const Time t) const {
    if (t < 0.0)
        return 0.0;
    Size i = std::upper_bound(t1_.begin(), t1_.end(), t) - t1_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += b1_[std::min(i - 1, b1_.size() - 1)];
    Real a = y1_[std::min(i, t1_.size() - 1)];
    res += a * a * (t - (i == 0 ? 0.0 : t1_[i - 1]));
    return res;
}

inline Real PiecewiseConstantParametrization::exp_m_int_y2(const Time t) const {
    if (t < 0.0)
        return 0.0;
    Size i = std::upper_bound(t2_.begin(), t2_.end(), t) - t2_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += b2_[std::min(i - 1, b2_.size() - 1)];
    Real a = y2_[std::min(i, y2_.size() - 1)];
    res += a * (t - (i == 0 ? 0.0 : t2_[i - 1]));
    return std::exp(-res);
}

inline Real
PiecewiseConstantParametrization::int_exp_m_int_y2(const Time t) const {
    if (t < 0.0)
        return 0.0;
    Size i = std::upper_bound(t2_.begin(), t2_.end(), t) - t2_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += c2_[std::min(i - 1, c2_.size() - 1)];
    Real a = y2_[std::min(i, y2_.size() - 1)];
    Real t0 = (i == 0 ? 0.0 : t2_[i - 1]);
    Real b2Tmp = (i == 0 ? 0.0 : b2_[i - 1]);
    if (std::fabs(a) < zeroCutoff_) {
        res += std::exp(-b2Tmp) * (t - t0);
    } else {
        res += (std::exp(-b2Tmp - a * (t - t0)) - std::exp(-b2Tmp)) / a;
    }
    return res;
}

} // namespace QuantExt

#endif
