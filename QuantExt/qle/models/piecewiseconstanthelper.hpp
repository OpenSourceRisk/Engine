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

/*! \file piecewiseconstanthelper.hpp
    \brief helper classes for piecewise constant parametrizations
*/

#ifndef quantext_piecewiseconstant_helper_hpp
#define quantext_piecewiseconstant_helper_hpp

#include <qle/math/piecewisefunction.hpp>
#include <ql/math/array.hpp>

using namespace QuantLib;

namespace QuantExt {

class PiecewiseConstantHelper1 {
  public:
    /*! y are the raw values in the sense of parameter transformation */
    PiecewiseConstantHelper1(const Array &t, const Array &y);
    const Array &t() const;
    Array &y() const;
    void update() const;
    Real y(const Time t) const;
    //! int_0^t y^2(s) ds
    Real int_y_sqr(const Time t) const;

  protected:
    Real direct(const Real x) const;
    Real inverse(const Real y) const;

    const Array t_;
    mutable Array y_;

  private:
    mutable std::vector<Real> b_;
};

class PiecewiseConstantHelper2 {
  public:
    /*! y are the raw values in the sense of parameter transformation */
    PiecewiseConstantHelper2(const Array &t, const Array &y);
    const Array &t() const;
    Array &y() const;
    void update() const;
    Real y(const Time t) const;
    //! exp(int_0^t -y(s)) ds
    Real exp_m_int_y(const Time t) const;
    //! int_0^t exp(int_0^s -y(u) du) ds
    Real int_exp_m_int_y(const Time t) const;

  private:
    const Real zeroCutoff_;

  protected:
    Real direct(const Real x) const;
    Real inverse(const Real y) const;
    const Array t_;
    mutable Array y_;

  private:
    mutable std::vector<Real> b_, c_;
};

class PiecewiseConstantHelper3 {
  public:
    /*! y1, y2 are the raw values in the sense of parameter transformation */
    PiecewiseConstantHelper3(const Array &t, const Array &y1, const Array &y2);
    const Array &t() const;
    Array &y1() const;
    Array &y2() const;
    void update() const;
    Real y1(const Time t) const;
    Real y2(const Time t) const;
    //! int_0^t y1^2(s) exp(2*int_0^s y2(u) du) ds
    Real int_y1_sqr_exp_2_int_y2(const Time t) const;

  private:
    const Real zeroCutoff_;

  protected:
    Real direct1(const Real x) const;
    Real inverse1(const Real y) const;
    Real direct2(const Real x) const;
    Real inverse2(const Real y) const;
    const Array t_;
    mutable Array y1_, y2_;

  private:
    mutable std::vector<Real> b_, c_;
};

// inline

inline const Array &PiecewiseConstantHelper1::t() const { return t_; }

inline Array &PiecewiseConstantHelper1::y() const { return y_; }

inline Real PiecewiseConstantHelper1::direct(const Real x) const {
    return x * x;
}

inline Real PiecewiseConstantHelper1::inverse(const Real y) const {
    return std::sqrt(y);
}

inline void PiecewiseConstantHelper1::update() const {
    Real sum = 0.0;
    b_.resize(t_.size());
    for (Size i = 0; i < t_.size(); ++i) {
        sum += direct(y_[i]) * direct(y_[i]) *
               (t_[i] - (i == 0 ? 0.0 : t_[i - 1]));
        b_[i] = sum;
    }
}

inline const Array &PiecewiseConstantHelper2::t() const { return t_; }

inline Array &PiecewiseConstantHelper2::y() const { return y_; }

inline Real PiecewiseConstantHelper2::direct(const Real x) const { return x; }

inline Real PiecewiseConstantHelper2::inverse(const Real y) const { return y; }

inline void PiecewiseConstantHelper2::update() const {
    Real sum = 0.0, sum2 = 0.0;
    b_.resize(t_.size());
    c_.resize(t_.size());
    for (Size i = 0; i < t_.size(); ++i) {
        Real t0 = (i == 0 ? 0.0 : t_[i - 1]);
        sum += direct(y_[i]) * (t_[i] - t0);
        b_[i] = sum;
        Real b2Tmp = (i == 0 ? 0.0 : b_[i - 1]);
        if (std::fabs(direct(y_[i])) < zeroCutoff_) {
            sum2 += (t_[i] - t0) * std::exp(-b2Tmp);
        } else {
            sum2 += (std::exp(-b2Tmp) -
                     std::exp(-b2Tmp - direct(y_[i]) * (t_[i] - t0))) /
                    direct(y_[i]);
        }
        c_[i] = sum2;
    }
};

inline const Array &PiecewiseConstantHelper3::t() const { return t_; }

inline Array &PiecewiseConstantHelper3::y1() const { return y1_; }

inline Array &PiecewiseConstantHelper3::y2() const { return y2_; }

inline Real PiecewiseConstantHelper3::direct1(const Real x) const {
    return x * x;
}

inline Real PiecewiseConstantHelper3::inverse1(const Real y) const {
    return std::sqrt(y);
}

inline Real PiecewiseConstantHelper3::direct2(const Real x) const {
    return x;
}

inline Real PiecewiseConstantHelper3::inverse2(const Real y) const {
    return y;
}

inline void PiecewiseConstantHelper3::update() const {
    Real sum = 0.0, sum2 = 0.0;
    b_.resize(t_.size());
    c_.resize(t_.size());
    for (Size i = 0; i < t_.size(); ++i) {
        Real t0 = (i == 0 ? 0.0 : t_[i - 1]);
        sum += direct2(y2_[i]) * (t_[i] - t0);
        b_[i] = sum;
        Real b2Tmp = (i == 0 ? 0.0 : b_[i - 1]);
        if (std::fabs(direct2(y2_[i])) < zeroCutoff_) {
            sum2 += direct1(y1_[i]) * direct1(y1_[i]) * (t_[i] - t0) *
                    std::exp(2.0 * b2Tmp);
        } else {
            sum2 +=
                direct1(y1_[i]) * direct1(y1_[i]) *
                (std::exp(2.0 * b2Tmp + 2.0 * direct2(y2_[i]) * (t_[i] - t0)) -
                 std::exp(2.0 * b2Tmp)) /
                (2.0 * direct2(y2_[i]));
        }
        c_[i] = sum2;
    }
};

inline Real PiecewiseConstantHelper1::y(const Time t) const {
    return direct(QL_PIECEWISE_FUNCTION(t_, y_, t));
}

inline Real PiecewiseConstantHelper2::y(const Time t) const {
    return direct(QL_PIECEWISE_FUNCTION(t_, y_, t));
}

inline Real PiecewiseConstantHelper3::y1(const Time t) const {
    return direct1(QL_PIECEWISE_FUNCTION(t_, y1_, t));
}

inline Real PiecewiseConstantHelper3::y2(const Time t) const {
    return direct2(QL_PIECEWISE_FUNCTION(t_, y2_, t));
}

inline Real PiecewiseConstantHelper1::int_y_sqr(const Time t) const {
    if (t < 0.0)
        return 0.0;
    Size i = std::upper_bound(t_.begin(), t_.end(), t) - t_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += b_[std::min(i - 1, b_.size() - 1)];
    Real a = direct(y_[std::min(i, y_.size() - 1)]);
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
    Real a = y_[std::min(i, y_.size() - 1)];
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
    Real a = direct(y_[std::min(i, y_.size() - 1)]);
    Real t0 = (i == 0 ? 0.0 : t_[i - 1]);
    Real b2Tmp = (i == 0 ? 0.0 : b_[i - 1]);
    if (std::fabs(a) < zeroCutoff_) {
        res += std::exp(-b2Tmp) * (t - t0);
    } else {
        res += (std::exp(-b2Tmp) - std::exp(-b2Tmp - a * (t - t0))) / a;
    }
    return res;
}

inline Real
PiecewiseConstantHelper3::int_y1_sqr_exp_2_int_y2(const Time t) const {
    if (t < 0.0)
        return 0.0;
    Size i = std::upper_bound(t_.begin(), t_.end(), t) - t_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += c_[std::min(i - 1, c_.size() - 1)];
    Real a = direct2(y2_[std::min(i, y2_.size() - 1)]);
    Real b = direct1(y1_[std::min(i, y1_.size() - 1)]);
    Real t0 = (i == 0 ? 0.0 : t_[i - 1]);
    Real b2Tmp = (i == 0 ? 0.0 : b_[i - 1]);
    if (std::fabs(a) < zeroCutoff_) {
        res += b * b * std::exp(2.0 * b2Tmp) * (t - t0);
    } else {
        res += b * b * (std::exp(2.0 * b2Tmp + 2.0 * a * (t - t0)) -
                        std::exp(2.0 * b2Tmp)) /
               (2.0 * a);
    }
    return res;
}

} // namespace QuantExt

#endif
