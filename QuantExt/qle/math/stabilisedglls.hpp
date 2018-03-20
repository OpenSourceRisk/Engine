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

/*! \file qle/math/stabilisedglls.hpp
    \brief Numerically stabilised general linear least squares
    \ingroup math
*/

#ifndef quantext_stabilised_glls_hpp
#define quantext_stabilised_glls_hpp

#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/generallinearleastsquares.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/make_shared.hpp>
#include <boost/type_traits.hpp>

#include <vector>

using namespace QuantLib;
using namespace boost::accumulators;

namespace QuantExt {

//! Numerically stabilised general linear least squares
/*! The input data is lineaerly transformed before performing the linear least squares fit.
  The linear least squares fit on the transformed data is done using the
  GeneralLinearLeastSquares class.
    \ingroup math
 */

class StabilisedGLLS {
public:
    enum Method {
        None,      // No stabilisation
        MaxAbs,    // Divide x and y values by max of abs of values (per x coordinate, y)
        MeanStdDev // Subtract mean and divide by std dev (per x coordinate, y)
    };
    template <class xContainer, class yContainer, class vContainer>
    StabilisedGLLS(const xContainer& x, const yContainer& y, const vContainer& v, const Method method = MeanStdDev);

    const Array& transformedCoefficients() const { return glls_->coefficients(); }
    const Array& transformedResiduals() const { return glls_->residuals(); }
    const Array& transformedStandardErrors() const { return glls_->standardErrors(); }
    const Array& transformedError() const { return glls_->error(); }

    //! Transformation parameters (u => (u + shift) * multiplier for u = x, y)
    const Array& xMultiplier() const { return xMultiplier_; }
    const Array& xShift() const { return xShift_; }
    const Real yMultiplier() const { return yMultiplier_; }
    const Real yShift() const { return yShift_; }

    Size size() const { return glls_->residuals().size(); }
    Size dim() const { return glls_->coefficients().size(); }

    //! evaluate regression function in terms of original x, y
    template <class xType, class vContainer>
    Real eval(xType x, vContainer& v, typename boost::enable_if<typename boost::is_arithmetic<xType>::type>::type* = 0);

    //! evaluate regression function in terms of original x, y
    template <class xType, class vContainer>
    Real eval(xType x, vContainer& v,
              typename boost::disable_if<typename boost::is_arithmetic<xType>::type>::type* = 0);

protected:
    Array a_, err_, residuals_, standardErrors_, xMultiplier_, xShift_;
    Real yMultiplier_, yShift_;
    Method method_;
    boost::shared_ptr<GeneralLinearLeastSquares> glls_;

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
    : a_(v.end() - v.begin(), 0.0), err_(v.end() - v.begin(), 0.0), residuals_(y.end() - y.begin()),
      standardErrors_(v.end() - v.begin()), method_(method) {
    calculate(x, y, v);
}

template <class xContainer, class yContainer, class vContainer>
void StabilisedGLLS::calculate(
    xContainer x, yContainer y, vContainer v,
    typename boost::enable_if<typename boost::is_arithmetic<typename xContainer::value_type>::type>::type*) {

    std::vector<Real> xData(x.end() - x.begin(), 0.0), yData(y.end() - y.begin(), 0.0);
    xMultiplier_ = Array(1, 1.0);
    xShift_ = Array(1, 0.0);
    yMultiplier_ = 1.0;
    yShift_ = 0.0;

    switch (method_) {
    case None:
        break;
    case MaxAbs: {
        Real mx = 0.0, my = 0.0;
        for (Size i = 0; i < static_cast<Size>(x.end() - x.begin()); ++i) {
            mx = std::max(std::abs(x[i]), mx);
        }
        if (!close_enough(mx, 0.0))
            xMultiplier_[0] = 1.0 / mx;
        for (Size i = 0; i < static_cast<Size>(y.end() - y.begin()); ++i) {
            my = std::max(std::abs(y[i]), my);
        }
        if (!close_enough(my, 0.0))
            yMultiplier_ = 1.0 / my;
        break;
    }
    case MeanStdDev: {
        accumulator_set<Real, stats<tag::mean, tag::variance> > acc;
        for (Size i = 0; i < static_cast<Size>(x.end() - x.begin()); ++i) {
            acc(x[i]);
        }
        xShift_[0] = -mean(acc);
        Real tmp = variance(acc);
        if (!close_enough(tmp, 0.0))
            xMultiplier_[0] = 1.0 / std::sqrt(tmp);
        accumulator_set<Real, stats<tag::mean, tag::variance> > acc2;
        for (Size i = 0; i < static_cast<Size>(y.end() - y.begin()); ++i) {
            acc2(y[i]);
        }
        yShift_ = -mean(acc2);
        Real tmp2 = variance(acc2);
        if (!close_enough(tmp2, 0.0))
            yMultiplier_ = 1.0 / std::sqrt(tmp2);
        break;
    }
    default:
        QL_FAIL("unknown stabilisation method");
    }

    for (Size i = 0; i < static_cast<Size>(x.end() - x.begin()); ++i) {
        xData[i] = (x[i] + xShift_[0]) * xMultiplier_[0];
    }
    for (Size i = 0; i < static_cast<Size>(y.end() - y.begin()); ++i) {
        yData[i] = (y[i] + yShift_) * yMultiplier_;
    }

    glls_ = boost::make_shared<GeneralLinearLeastSquares>(xData, yData, v);
}

template <class xContainer, class yContainer, class vContainer>
void StabilisedGLLS::calculate(
    xContainer x, yContainer y, vContainer v,
    typename boost::disable_if<typename boost::is_arithmetic<typename xContainer::value_type>::type>::type*) {

    QL_REQUIRE(x.end() - x.begin() > 0, "StabilisedGLLS::calculate(): x container is empty");
    QL_REQUIRE(x[0].end() - x[0].begin() > 0, "StabilisedGLLS:calculate(): x contains empty point(s)");

    std::vector<Array> xData(x.end() - x.begin(), Array(x[0].end() - x[0].begin(), 0.0));
    std::vector<Real> yData(y.end() - y.begin(), 0.0);
    xMultiplier_ = Array(x[0].end() - x[0].begin(), 1.0);
    xShift_ = Array(x[0].end() - x[0].begin(), 0.0);
    yMultiplier_ = 1.0;
    yShift_ = 0.0;

    switch (method_) {
    case None:
        break;
    case MaxAbs: {
        Array m(x[0].end() - x[0].begin(), 0.0);
        Real my = 0.0;
        for (Size i = 0; i < static_cast<Size>(x.end() - x.begin()); ++i) {
            for (Size j = 0; j < m.size(); ++j) {
                m[j] = std::max(std::abs(x[i][j]), m[j]);
            }
        }
        for (Size j = 0; j < m.size(); ++j) {
            if (!close_enough(m[j], 0.0))
                xMultiplier_[j] = 1.0 / m[j];
        }
        for (Size i = 0; i < static_cast<Size>(y.end() - y.begin()); ++i) {
            my = std::max(std::abs(y[i]), my);
        }
        if (!close_enough(my, 0.0))
            yMultiplier_ = 1.0 / my;
        break;
    }
    case MeanStdDev: {
        std::vector<accumulator_set<Real, stats<tag::mean, tag::variance> > > acc(x[0].end() - x[0].begin());
        for (Size i = 0; i < static_cast<Size>(x.end() - x.begin()); ++i) {
            for (Size j = 0; j < acc.size(); ++j) {
                acc[j](x[i][j]);
            }
        }
        for (Size j = 0; j < acc.size(); ++j) {
            xShift_[j] = -mean(acc[j]);
            Real tmp = variance(acc[j]);
            if (!close_enough(tmp, 0.0))
                xMultiplier_[j] = 1.0 / std::sqrt(tmp);
        }
        accumulator_set<Real, stats<tag::mean, tag::variance> > acc2;
        for (Size i = 0; i < static_cast<Size>(y.end() - y.begin()); ++i) {
            acc2(y[i]);
        }
        yShift_ = -mean(acc2);
        Real tmp2 = variance(acc2);
        if (!close_enough(tmp2, 0.0))
            yMultiplier_ = 1.0 / std::sqrt(tmp2);
        break;
    }
    default:
        QL_FAIL("unknown stabilisation method");
        break;
    }

    for (Size i = 0; i < static_cast<Size>(x.end() - x.begin()); ++i) {
        for (Size j = 0; j < xMultiplier_.size(); ++j) {
            xData[i][j] = (x[i][j] + xShift_[j]) * xMultiplier_[j];
        }
    }
    for (Size i = 0; i < static_cast<Size>(y.end() - y.begin()); ++i) {
        yData[i] = (y[i] + yShift_) * yMultiplier_;
    }

    glls_ = boost::make_shared<GeneralLinearLeastSquares>(xData, yData, v);
}

template <class xType, class vContainer>
Real StabilisedGLLS::eval(xType x, vContainer& v,
                          typename boost::enable_if<typename boost::is_arithmetic<xType>::type>::type*) {
    QL_REQUIRE(v.size() == glls_->dim(),
               "StabilisedGLLS::eval(): v size (" << v.size() << ") must be equal to dim (" << glls_->dim());
    Real tmp = 0.0;
    for (Size i = 0; i < v.size(); ++i) {
        tmp += glls_->coefficients()[i] * v[i]((x + xShift_[0]) * xMultiplier_[0]);
    }
    return tmp / yMultiplier_ - yShift_;
}

template <class xType, class vContainer>
Real StabilisedGLLS::eval(xType x, vContainer& v,
                          typename boost::disable_if<typename boost::is_arithmetic<xType>::type>::type*) {
    QL_REQUIRE(v.size() == glls_->dim(),
               "StabilisedGLLS::eval(): v size (" << v.size() << ") must be equal to dim (" << glls_->dim());
    Real tmp = 0.0;
    for (Size i = 0; i < v.size(); ++i) {
        xType xNew(x.end() - x.begin());
        for (Size j = 0; j < static_cast<Size>(x.end() - x.begin()); ++j) {
            xNew[j] = (x[j] + xShift_[j]) * xMultiplier_[j];
        }
        tmp += glls_->coefficients()[i] * v[i](xNew);
    }
    return tmp / yMultiplier_ - yShift_;
}

} // namespace QuantExt

#endif
