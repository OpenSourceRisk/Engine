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

#ifndef quantext_nadaraya_watson_regression_hpp
#define quantext_nadaraya_watson_regression_hpp

#include <ql/math/comparison.hpp>

#include <boost/make_shared.hpp>

/*! \file qle/math/nadarayawatson.hpp
    \brief Nadaraya-Watson regression
    \ingroup math
*/

using QuantLib::Real;
using QuantLib::Size;

namespace QuantExt {
namespace detail {

//! Regression impl
/*! \ingroup math
 */
class RegressionImpl {
public:
    virtual ~RegressionImpl() {}
    virtual void update() = 0;
    virtual Real value(Real x) const = 0;
    virtual Real standardDeviation(Real x) const = 0;
};

//! Nadaraya Watson impl
/*! \ingroup math
 */
template <class I1, class I2, class Kernel> class NadarayaWatsonImpl : public RegressionImpl {
public:
    /*! \pre the \f$ x \f$ values must be sorted.
        \pre kernel needs a Real operator()(Real x) implementation
    */
    NadarayaWatsonImpl(const I1& xBegin, const I1& xEnd, const I2& yBegin, const Kernel& kernel)
        : xBegin_(xBegin), xEnd_(xEnd), yBegin_(yBegin), kernel_(kernel) {}

    void update() {}

    Real value(Real x) const {

        Real tmp1 = 0.0, tmp2 = 0.0;

        for (Size i = 0; i < static_cast<Size>(xEnd_ - xBegin_); ++i) {
            Real tmp = kernel_(x - xBegin_[i]);
            tmp1 += yBegin_[i] * tmp;
            tmp2 += tmp;
        }

        return QuantLib::close_enough(tmp2, 0.0) ? 0.0 : tmp1 / tmp2;
    }

    Real standardDeviation(Real x) const {

        Real tmp1 = 0.0, tmp1b = 0.0, tmp2 = 0.0;

        for (Size i = 0; i < static_cast<Size>(xEnd_ - xBegin_); ++i) {
            Real tmp = kernel_(x - xBegin_[i]);
            tmp1 += yBegin_[i] * tmp;
            tmp1b += yBegin_[i] * yBegin_[i] * tmp;
            tmp2 += tmp;
        }

        return QuantLib::close_enough(tmp2, 0.0) ? 0.0 : std::sqrt(tmp1b / tmp2 - (tmp1 * tmp1) / (tmp2 * tmp2));
    }

private:
    I1 xBegin_, xEnd_;
    I2 yBegin_;
    Kernel kernel_;
};
} // namespace detail

//! Nadaraya Watson regression
/*! This implements the estimator

    \f[
    m(x) = \frac{\sum_i y_i K(x-x_i)}{\sum_i K(x-x_i)}
    \f]

    \ingroup math
*/
class NadarayaWatson {
public:
    /*! \pre the \f$ x \f$ values must be sorted.
        \pre kernel needs a Real operator()(Real x) implementation
    */
    template <class I1, class I2, class Kernel>
    NadarayaWatson(const I1& xBegin, const I1& xEnd, const I2& yBegin, const Kernel& kernel) {
        impl_ = boost::make_shared<detail::NadarayaWatsonImpl<I1, I2, Kernel> >(xBegin, xEnd, yBegin, kernel);
    }

    Real operator()(Real x) const { return impl_->value(x); }

    Real standardDeviation(Real x) const { return impl_->standardDeviation(x); }

private:
    boost::shared_ptr<detail::RegressionImpl> impl_;
};

} // namespace QuantExt

#endif
