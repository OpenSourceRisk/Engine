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
#include <ql/math/comparison.hpp>
#include <ql/math/generallinearleastsquares.hpp>

#include <boost/make_unique.hpp>
#include <boost/type_traits.hpp>

#include <vector>

using namespace QuantLib;

namespace QuantExt {

//! Numerically stabilised general linear least squares
/*! The input data is lineaerly transformed before performing the linear least squares fit.
  See GeneralLinearLeastSquares for additional information on the least squares
  method itself. */
class StabilisedGLLS {
public:
    enum Method {
        None,      // No stabilisation
        MaxAbs,    // Divide x and y values by max of abs of values (per x coordinate and y vector)
        MeanStdDev // Subtract mean and divide by std dev (per x coordinate and y vector)
    };
    template <class xContainer, class yContainer, class vContainer>
    StabilisedGLLS(const xContainer& x, const yContainer& y, const vContainer& v, const Method method = MeanStdDev);

    const Array& transformedCoefficients() const { return glls_->coefficients(); }
    const Array& transformedResiduals() const { return glls_->residuals(); }
    const Array& transformedStandardErrors() const { return glls_->standardErrors(); }
    const Array& transformedError() const { return glls_->error(); }

    //! Transformation parameters
    const Array& xMultiplier() const { return xMultiplier_; }
    const Array& xShift() const { return xShift_; }
    const Real yMultiplier() const { return yMultiplier_; }
    const Real yShift() const { return yShift_; }

    Size size() const { return glls_->residuals().size(); }
    Size dim() const { return glls_->coefficients().size(); }

    template <class xType, class vContainer>
    Real eval(xType x, vContainer& v, typename boost::enable_if<typename boost::is_arithmetic<xType>::type>::type* = 0);

    template <class xType, class vContainer>
    Real eval(xType x, vContainer& v,
              typename boost::disable_if<typename boost::is_arithmetic<xType>::type>::type* = 0);

protected:
    Array a_, err_, residuals_, standardErrors_, xMultiplier_, xShift_;
    Real yMultiplier_, yShift_;
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

    std::vector<Real> xData(x.size(), 0.0), yData(y.size(), 0.0);
    xMultiplier_ = Array(1, 1.0);
    xShift_ = Array(1, 0.0);
    yMultiplier_ = 1.0;
    yShift_ = 0.0;

    switch (method_) {
    case None:
        break;
    case MaxAbs: {
        Real mx = 0.0, my = 0.0;
        for (Size i = 0; i < x.size(); ++i) {
            mx = std::max(std::abs(x[i]), mx);
        }
        if (!close_enough(mx, 0.0))
            xMultiplier_[0] = 1.0 / mx;
        for (Size i = 0; i < y.size(); ++i) {
            my = std::max(std::abs(y[i]), my);
        }
        if (!close_enough(my, 0.0))
            yMultiplier_ = 1.0 / my;
        break;
    }
    case MeanStdDev:
        QL_FAIL("std dev to do ...");
        break;
    default:
        QL_FAIL("unknown stabilization method");
    }

    for (Size i = 0; i < x.size(); ++i) {
        xData[i] = x[i] * xMultiplier_[0] + xShift_[0];
    }
    for (Size i = 0; i < y.size(); ++i) {
        yData[i] = y[i] * yMultiplier_ + yShift_;
    }

    glls_ = boost::make_unique<GeneralLinearLeastSquares>(xData, yData, v);
}

template <class xContainer, class yContainer, class vContainer>
void StabilisedGLLS::calculate(
    xContainer x, yContainer y, vContainer v,
    typename boost::disable_if<typename boost::is_arithmetic<typename xContainer::value_type>::type>::type*) {

    QL_REQUIRE(x.size() > 0, "StabilisedGLLS::calculate(): x container is empty");
    QL_REQUIRE(x[0].size() > 0, "StabilisedGLLS:calculate(): x contains empty point(s)");

    std::vector<Array> xData(x.size(), Array(x[0].size(), 0.0));
    std::vector<Real> yData(y.size(), 0.0);
    xMultiplier_ = Array(x.size(), 1.0);
    xShift_ = Array(x.size(), 0.0);
    yMultiplier_ = 1.0;
    yShift_ = 0.0;

    switch (method_) {
    case None:
        break;
    case MaxAbs: {
        Array m(x[0].size(), 0.0);
        Real my = 0.0;
        for (Size i = 0; i < x.size(); ++i) {
            for (Size j = 0; j < m.size(); ++j) {
                m[j] = std::max(std::abs(x[i][j]), m[j]);
            }
        }
        for (Size j = 0; j < m.size(); ++j) {
            if (!close_enough(m[j], 0.0))
                xMultiplier_[j] = 1.0 / m[j];
        }
        for (Size i = 0; i < y.size(); ++i) {
            my = std::max(std::abs(y[i]), my);
        }
        if (!close_enough(my, 0.0))
            yMultiplier_ = 1.0 / my;
        break;
    }
    case MeanStdDev:
        QL_FAIL("std dev to do ...");
        break;
    default:
        QL_FAIL("unknown stabilization method");
        break;
    }

    for (Size i = 0; i < x.size(); ++i) {
        for (Size j = 0; j < xMultiplier_.size(); ++j) {
            xData[i][j] = x[i][j] * xMultiplier_[j] + xShift_[j];
        }
    }
    for (Size i = 0; i < y.size(); ++i) {
        yData[i] = y[i] * yMultiplier_ + yShift_;
    }

    glls_ = boost::make_unique<GeneralLinearLeastSquares>(xData, yData, v);
}

template <class xType, class vContainer>
Real StabilisedGLLS::eval(xType x, vContainer& v,
                          typename boost::enable_if<typename boost::is_arithmetic<xType>::type>::type*) {
    QL_REQUIRE(v.size() == glls_->dim(), "StabilisedGLLS::eval(): v size (" << v.size() << ") must be equal to dim ("
                                                                            << glls_->dim());
    Real tmp = 0.0;
    for (Size i = 0; i < v.size(); ++i) {
        tmp += glls_->coefficients()[i] * v[i](x * xMultiplier_[0] + xShift_[0]);
    }
    return (tmp - yShift_) / yMultiplier_;
}

template <class xType, class vContainer>
Real StabilisedGLLS::eval(xType x, vContainer& v,
                          typename boost::disable_if<typename boost::is_arithmetic<xType>::type>::type*) {
    QL_REQUIRE(v.size() == glls_->dim(), "StabilisedGLLS::eval(): v size (" << v.size() << ") must be equal to dim ("
                                                                            << glls_->dim());
    Real tmp = 0.0;
    for (Size i = 0; i < v.size(); ++i) {
        xType xNew(x.size());
        for (Size j = 0; j < x.size(); ++j) {
            xNew[j] = x[j] * xMultiplier_[j] + xShift_[j];
        }
        tmp += glls_->coefficients()[i] * v[i](xNew);
    }
    return (tmp - yShift_) / yMultiplier_;
}

} // namespace QuantExt

#endif
