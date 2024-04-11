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

/*! \file normalsabrinterpolation.hpp
    \brief normal SABR interpolation interpolation between discrete points
*/

#pragma once

#include <qle/models/normalsabr.hpp>

#include <ql/math/interpolations/xabrinterpolation.hpp>
#include <ql/termstructures/volatility/sabr.hpp>

#include <boost/assign/list_of.hpp>
#include <boost/make_shared.hpp>

namespace QuantExt {
using namespace QuantLib;

namespace detail {

class NormalSABRWrapper {
public:
    NormalSABRWrapper(const Time t, const Real& forward, const std::vector<Real>& params,
                      const std::vector<Real>& addParams)
        : t_(t), forward_(forward), params_(params) {
        // validateSabrParameters(params[0], 0.0, params[1], params[2]);
    }
    const std::vector<Real>& params() const { return params_; }
    Real volatility(const Real x) { return normalSabrVolatility(x, forward_, t_, params_[0], params_[1], params_[2]); }

private:
    const Real t_, &forward_;
    const std::vector<Real> params_;
};

struct NormalSABRSpecs {
    Size dimension() { return 3; }
    void defaultValues(std::vector<Real>& params, std::vector<bool>&, const Real& forward, const Real expiryTime,
                       const std::vector<Real>& addParams) {
        if (params[0] == Null<Real>())
            params[0] = 0.0040;
        if (params[1] == Null<Real>())
            params[1] = std::sqrt(0.4);
        if (params[2] == Null<Real>())
            params[2] = 0.0;
    }
    void guess(Array& values, const std::vector<bool>& paramIsFixed, const Real& forward, const Real expiryTime,
               const std::vector<Real>& r, const std::vector<Real>& addParams) {
        Size j = 0;
        if (!paramIsFixed[0]) {
            values[0] = (0.01 - 2E-6) * r[j++] + 1E-6; // normal vol guess
        }
        if (!paramIsFixed[1])
            values[1] = 1.5 * r[j++] + 1E-6;
        if (!paramIsFixed[2])
            values[2] = (2.0 * r[j++] - 1.0) * (1.0 - 1E-6);
    }
    Real eps1() { return .0000001; }
    Real eps2() { return .9999; }
    Real dilationFactor() { return 0.001; }
    Array inverse(const Array& y, const std::vector<bool>&, const std::vector<Real>&, const Real) {
        Array x(3);
        x[0] = std::tan(y[0] * M_PI / 0.02 - M_PI_2);
        x[1] = std::tan(y[1] * M_PI / 5.00 - M_PI_2);
        x[2] = std::tan((y[2] + 1.0) * M_PI / 2.0 - M_PI_2);
        return x;
    }
    Array direct(const Array& x, const std::vector<bool>&, const std::vector<Real>&, const Real) {
        Array y(3);
        y[0] = 0.02 * (std::atan(x[0]) + M_PI_2) / M_PI;
        y[1] = 5.00 * (std::atan(x[1]) + M_PI_2) / M_PI;
        y[2] = 2.0 * (std::atan(x[2]) + M_PI_2) / M_PI - 1.0;
        return y;
    }
    Real weight(const Real strike, const Real forward, const Real stdDev, const std::vector<Real>& addParams) {
        return bachelierBlackFormulaStdDevDerivative(strike, forward, stdDev, 1.0);
    }
    typedef NormalSABRWrapper type;
    QuantLib::ext::shared_ptr<type> instance(const Time t, const Real& forward, const std::vector<Real>& params,
                                     const std::vector<Real>& addParams) {
        std::vector<Real> updatedParams(params);
        if (!addParams.empty()) {
            updatedParams[0] = normalSabrAlphaFromAtmVol(forward, t, addParams.front(), params[1], params[2]);
        }
        return QuantLib::ext::make_shared<type>(t, forward, updatedParams, addParams);
    }
};
} // namespace detail

//! %SABR smile interpolation between discrete volatility points.
/*! \ingroup interpolations */
class NormalSABRInterpolation : public Interpolation {
public:
    template <class I1, class I2>
    NormalSABRInterpolation(
        const I1& xBegin, // x = strikes
        const I1& xEnd,
        const I2& yBegin, // y = volatilities
        Time t,           // option expiry
        const Real& forward, Real alpha, Real nu, Real rho, bool alphaIsFixed, bool nuIsFixed, bool rhoIsFixed,
        bool vegaWeighted = true, const Size atmStrikeIndex = Null<Size>(), const bool implyAlphaFromAtmVol = false,
        const QuantLib::ext::shared_ptr<EndCriteria>& endCriteria = QuantLib::ext::shared_ptr<EndCriteria>(),
        const QuantLib::ext::shared_ptr<OptimizationMethod>& optMethod = QuantLib::ext::shared_ptr<OptimizationMethod>(),
        const Real errorAccept = 0.0002, const bool useMaxError = false, const Size maxGuesses = 50) {

        QL_REQUIRE(
            !implyAlphaFromAtmVol || atmStrikeIndex != Null<Real>(),
            "NormalSABRInterpolation: imply alpha from atm vol implies a) that the atm strike index must be given");

        std::vector<Real> addParams;
        if (implyAlphaFromAtmVol) {
            addParams.push_back(*std::next(yBegin, atmStrikeIndex));
        }

        impl_ = QuantLib::ext::shared_ptr<Interpolation::Impl>(
            new QuantLib::detail::XABRInterpolationImpl<I1, I2, detail::NormalSABRSpecs>(
                xBegin, xEnd, yBegin, t, forward, boost::assign::list_of(alpha)(nu)(rho),
                boost::assign::list_of(alphaIsFixed)(nuIsFixed)(rhoIsFixed), vegaWeighted, endCriteria, optMethod,
                errorAccept, useMaxError, maxGuesses, addParams));
        coeffs_ = QuantLib::ext::dynamic_pointer_cast<QuantLib::detail::XABRCoeffHolder<detail::NormalSABRSpecs>>(impl_);
    }
    Real expiry() const { return coeffs_->t_; }
    Real forward() const { return coeffs_->forward_; }
    Real alpha() const { return coeffs_->modelInstance_->params()[0]; }
    Real nu() const { return coeffs_->modelInstance_->params()[1]; }
    Real rho() const { return coeffs_->modelInstance_->params()[2]; }
    Real rmsError() const { return coeffs_->error_; }
    Real maxError() const { return coeffs_->maxError_; }
    const std::vector<Real>& interpolationWeights() const { return coeffs_->weights_; }
    EndCriteria::Type endCriteria() { return coeffs_->XABREndCriteria_; }

private:
    QuantLib::ext::shared_ptr<QuantLib::detail::XABRCoeffHolder<detail::NormalSABRSpecs>> coeffs_;
};

//! %SABR interpolation factory and traits
/*! \ingroup interpolations */
class NormalSABR {
public:
    NormalSABR(Time t, Real forward, Real alpha, Real nu, Real rho, bool alphaIsFixed, bool nuIsFixed, bool rhoIsFixed,
               bool vegaWeighted = false,
               const QuantLib::ext::shared_ptr<EndCriteria> endCriteria = QuantLib::ext::shared_ptr<EndCriteria>(),
               const QuantLib::ext::shared_ptr<OptimizationMethod> optMethod = QuantLib::ext::shared_ptr<OptimizationMethod>(),
               const Real errorAccept = 0.0002, const bool useMaxError = false, const Size maxGuesses = 50)
        : t_(t), forward_(forward), alpha_(alpha), nu_(nu), rho_(rho), alphaIsFixed_(alphaIsFixed),
          nuIsFixed_(nuIsFixed), rhoIsFixed_(rhoIsFixed), vegaWeighted_(vegaWeighted), endCriteria_(endCriteria),
          optMethod_(optMethod), errorAccept_(errorAccept), useMaxError_(useMaxError), maxGuesses_(maxGuesses) {}
    template <class I1, class I2> Interpolation interpolate(const I1& xBegin, const I1& xEnd, const I2& yBegin) const {
        return SABRInterpolation(xBegin, xEnd, yBegin, t_, forward_, alpha_, nu_, rho_, alphaIsFixed_, nuIsFixed_,
                                 rhoIsFixed_, vegaWeighted_, endCriteria_, optMethod_, errorAccept_, useMaxError_,
                                 maxGuesses_);
    }
    static const bool global = true;

private:
    Time t_;
    Real forward_;
    Real alpha_, nu_, rho_;
    bool alphaIsFixed_, nuIsFixed_, rhoIsFixed_;
    bool vegaWeighted_;
    const QuantLib::ext::shared_ptr<EndCriteria> endCriteria_;
    const QuantLib::ext::shared_ptr<OptimizationMethod> optMethod_;
    const Real errorAccept_;
    const bool useMaxError_;
    const Size maxGuesses_;
};
} // namespace QuantExt
