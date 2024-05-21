/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file sabrparametricvolatility.hpp
    \brief sabr volatility structure
*/

#pragma once

#include <qle/termstructures/parametricvolatility.hpp>

#include <ql/math/interpolations/interpolation2d.hpp>

namespace QuantExt {

class SabrParametricVolatility final : public ParametricVolatility {
public:
    enum class ModelVariant {
        Hagan2002Lognormal = 0,
        Hagan2002Normal = 1,
        Hagan2002NormalZeroBeta = 2,
        Antonov2015FreeBoundaryNormal = 3,
        KienitzLawsonSwaynePde = 4 ,
        FlochKennedy = 5
    };

    /*! modelParameters are given by (tte, underlyingLen) as a vector of parameter values and
        whether the values are fixed */
    SabrParametricVolatility(
        const ModelVariant modelVariant, const std::vector<MarketSmile> marketSmiles,
        const MarketModelType marketModelType, const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters =
            {},
        const QuantLib::Size maxCalibrationAttempts = 10, const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05);

    QuantLib::Real
    evaluate(const QuantLib::Real timeToExpiry, const QuantLib::Real underlyingLength, const QuantLib::Real strike,
             const QuantLib::Real forward, const MarketQuoteType outputMarketQuoteType,
             const QuantLib::Real outputLognormalShift = QuantLib::Null<QuantLib::Real>(),
             const boost::optional<QuantLib::Option::Type> outputOptionType = boost::none) const override;

    // the calculated grid of option expiries and the underlying lenghts
    const std::vector<Real>& timeToEpiries() const;
    const std::vector<Real>& underlyingLenghts() const;
    // calibrated or interpolated model parameters (rows = underlying lenghts, cols = option expiries)
    const QuantLib::Matrix& alpha() const { return alpha_; }
    const QuantLib::Matrix& beta() const { return beta_; }
    const QuantLib::Matrix& nu() const { return nu_; }
    const QuantLib::Matrix& rho() const { return rho_; }
    const QuantLib::Matrix& lognormalShift() const { return lognormalShift_; }
    const QuantLib::Matrix& numberOfCalibrationAttempts() const { return numberOfCalibrationAttempts_; }
    // calibration error
    const QuantLib::Matrix& calibrationError() const { return calibrationError_; }
    // indicator whether smile params were interpolated (1) or calibrated (0)
    const QuantLib::Matrix& isInterpolated() const { return isInterpolated_; }

private:
    static constexpr double eps1 = .0000001;
    static constexpr double eps2 = .9999;
    static constexpr double max_nvol_equiv = 0.02;
    static constexpr double max_nu = 2.0;

    void calculate();

    std::vector<std::pair<Real, bool>> defaultModelParameters() const;
    std::vector<Real> getGuess(const std::vector<std::pair<Real, bool>>& params, const std::vector<Real>& randomSeq,
                               const Real forward, const Real lognormalShift) const;
    ParametricVolatility::MarketQuoteType preferredOutputQuoteType() const;
    std::vector<Real> direct(const std::vector<Real>& x, const Real forward, const Real lognormalShift) const;
    std::vector<Real> inverse(const std::vector<Real>& y, const Real forward, const Real lognormalShift) const;
    std::vector<Real> evaluateSabr(const std::vector<Real>& params, const Real forward, const Real timeToExpiry,
                                   const Real lognormalShift, const std::vector<Real>& strikes) const;
    std::tuple<std::vector<Real>, Real, QuantLib::Size>
    calibrateModelParameters(const MarketSmile& marketSmile, const std::vector<std::pair<Real, bool>>& params) const;

    ModelVariant modelVariant_;
    std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters_;
    QuantLib::Size maxCalibrationAttempts_;
    QuantLib::Real exitEarlyErrorThreshold_;
    QuantLib::Real maxAcceptableError_;

    mutable std::map<std::pair<Real, Real>, std::vector<Real>> calibratedSabrParams_;
    mutable std::map<std::pair<Real, Real>, Real> lognormalShifts_;
    mutable std::map<std::pair<Real, Real>, Real> calibrationErrors_;
    mutable std::map<std::pair<Real, Real>, QuantLib::Size> noOfAttempts_;

    mutable std::vector<Real> underlyingLengths_, timeToExpiries_;
    mutable std::vector<Real> underlyingLengthsForInterpolation_, timeToExpiriesForInterpolation_;
    mutable QuantLib::Matrix alpha_, beta_, nu_, rho_, lognormalShift_, calibrationError_, isInterpolated_,
        numberOfCalibrationAttempts_;
    mutable QuantLib::Interpolation2D alphaInterpolation_, betaInterpolation_, nuInterpolation_, rhoInterpolation_,
        lognormalShiftInterpolation_;
};

} // namespace QuantExt
