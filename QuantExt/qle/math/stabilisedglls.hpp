/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file stabilisedglls.hpp
    \brief Numerically stabilised general linear least squares
    \ingroup termstructures
*/

#ifndef quantext_stabilised_glls_hpp
#define quantext_stabilised_glls_hpp

#include <ql/math/array.hpp>
#include <ql/math/generallinearleastsquares.hpp>

#include <boost/make_unique.hpp>
#include <boost/type_traits.hpp>

#include <vector>

using namespace QuantLib;

namespace QuantExt {

//! Numerically stabilised general linear least squares
class StabilisedGLLS {
public:
    enum Method {
        None,      // No stabilisation
        MaxAbs,    // Scale by max of abs of values
        MeanStdDev // Scale by mean and stddev of values
    };
    template <class xContainer, class yContainer, class vContainer>
    StabilisedGLLS(const xContainer& x, const yContainer& y, const vContainer& v, const Method method = MeanStdDev);

    const Array& transformedCoefficients() const { return glls_->coefficients(); }
    const Array& transformedResiduals() const { return glls_->residuals(); }

    //! standard parameter errors as given by Excel, R etc.
    const Array& transformedStandardErrors() const { return glls_->standardErrors(); }
    //! modeling uncertainty as definied in Numerical Recipes
    const Array& transformedError() const { return glls_->error(); }

    Size size() const { return glls_->residuals().size(); }

    Size dim() const { return glls_->coefficients().size(); }

    // two versions (arithmetic, vector), store v
    // template <class xContainer> Real operator()(xContainer x) {
    //     Real tmp = 0.0;
    //     for (Size i = 0; i < v.size(); ++i) {
    //         tmp += glls_->coefficients[i] * v[i](x);
    //     }
    //     return tmp;
    // }

protected:
    Array a_, err_, residuals_, standardErrors_;
    Method method_;
    std::unique_ptr<GeneralLinearLeastSquares> glls_;

    template <class xContainer, class yContainer, class vContainer>
    void calculate(
        xContainer x, yContainer y, vContainer v,
        typename boost::enable_if<typename boost::is_arithmetic<typename xContainer::value_type>::type>::type* = 0);

    template <class xContainer, class yContainer, class vContainer>
    void calculate(
        xContainer x, yContainer y, vContainer v,
        typename boost::disable_if<typename boost::is_arithmetic<typename xContainer::value_type>::type>::type* = 0);
};

template <class xContainer, class yContainer, class vContainer>
inline StabilisedGLLS::StabilisedGLLS(const xContainer& x, const yContainer& y, const vContainer& v,
                                      const Method method)
    : a_(v.size(), 0.0), err_(v.size(), 0.0), residuals_(y.size()), standardErrors_(v.size()), method_(method) {
    calculate(x, y, v);
}

template <class xContainer, class yContainer, class vContainer>
void StabilisedGLLS::calculate(
    xContainer x, yContainer y, vContainer v,
    typename boost::enable_if<typename boost::is_arithmetic<typename xContainer::value_type>::type>::type*) {

    std::vector<Real> xData(x.size(), 0.0);
    switch (method_) {
    case None:
        break;
    case MaxAbs: {
        Real m = 0.0;
        for (Size i = 0; i < x.size(); ++i) {
            m = std::max(std::abs(x[i]), m);
        }
        for (Size i = 0; i < x.size(); ++i) {
            xData[i] = x[i] / m;
        }
        break;
    }
    case MeanStdDev:
        QL_FAIL("std dev to do ...");
        break;
    default:
        QL_FAIL("unknown stabilization method");
    }

    glls_ = boost::make_unique<GeneralLinearLeastSquares>(xData, y, v);
}

template <class xContainer, class yContainer, class vContainer>
void StabilisedGLLS::calculate(
    xContainer x, yContainer y, vContainer v,
    typename boost::disable_if<typename boost::is_arithmetic<typename xContainer::value_type>::type>::type*) {

    std::vector<Array> xData(x.size(), 0.0);
    switch (method_) {
    case None:
        break;
    case MaxAbs: {
        QL_REQUIRE(x.size() > 0, "empty x container");
        Array m(x[0].size(), 0.0);
        for (Size i = 0; i < x.size(); ++i) {
            for (Size j = 0; j < m.size(); ++j) {
                m[j] = std::max(std::abs(x[i][j]), m[i]);
            }
        }
        for (Size i = 0; i < x.size(); ++i) {
            for (Size j = 0; j < m.size(); ++j) {
                xData[i][j] /= m[j];
            }
        }
        break;
    }
    case MeanStdDev:
        QL_FAIL("std dev to do ...");
        break;
    default:
        QL_FAIL("unknown stabilization method");
        break;
    }

    glls_ = boost::make_unique<GeneralLinearLeastSquares>(xData, y, v);
}

} // namespace QuantExt

#endif
