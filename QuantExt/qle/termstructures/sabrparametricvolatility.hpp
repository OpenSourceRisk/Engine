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

#include <ql/math/interpolation.hpp>
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

    /*! Value for a single parameter, i.e. \f$\alpha\f$, \f$\beta\f$, \f$\nu\f$ or \f$\rho\f$, and whether it is 
        fixed, calibrated or implied.
    */
    using SingleParamInfo = std::pair<Real, ParameterCalibration>;
    /*! Calibration information for a single smile i.e. all four parameters in the order \f$\alpha\f$, \f$\beta\f$, 
        \f$\nu\f$, \f$\rho\f$.
    */
    using SliceParamInfo = std::vector<SingleParamInfo>;
    /*! Each smile is identified by a key consisting of time-to-expiry and underlying tenor.
        The underlying tenor is used for swaptions and set to `Null<Real>()` for cap floor surfaces and other surfaces
        where the underlying tenor is not relevant.
    */
    using TteUndKey = std::pair<QuantLib::Real, QuantLib::Real>;
    //! The parameter information for all smiles for the volatility surface or cube.
    using ParamInfo = std::map<TteUndKey, SliceParamInfo>;

    /*! \param modelVariant
            SABR model variant to use.

        \param marketSmiles
            Input market smiles.

        \param marketModelType
            Market model type represented by the input smiles.

        \param inputMarketQuoteType
            Quote type of the input market smiles.

        \param discountCurve
            Discount curve used where required by the model.

        \param modelParameters
            Optional SABR model parameters, indexed by (time-to-expiry, underlying length).
            Each entry contains the parameter values together with their calibration status.

        \param modelShift
            Optional lognormal shifts indexed by underlying length. If not provided, the
            shift from the corresponding input market smile is used.

        \param maxCalibrationAttempts
            Maximum number of calibration attempts per smile.

        \param exitEarlyErrorThreshold
            Calibration error threshold below which no further calibration attempts are made.

        \param maxAcceptableError
            Maximum acceptable calibration error.

        \param residualCorrection
            Optional residual correction method. If not provided, no residual correction is applied.
    */
    SabrParametricVolatility(const ModelVariant modelVariant,
        const std::vector<MarketSmile>& marketSmiles,
        const MarketModelType marketModelType,
        const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const ParamInfo& modelParameters = {},
        const std::map<QuantLib::Real, QuantLib::Real>& modelShift = {},
        const QuantLib::Size maxCalibrationAttempts = 10,
        const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05,
        QuantLib::ext::optional<ResidualCorrection> residualCorrection = QuantLib::ext::nullopt);

    QuantLib::Real evaluate(const QuantLib::Real timeToExpiry,
        const QuantLib::Real underlyingLength,
        const QuantLib::Real strike,
        const QuantLib::Real forward,
        const MarketQuoteType outputMarketQuoteType,
        const QuantLib::Real outputLognormalShift = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType = QuantLib::ext::nullopt) const override;

    // the calculated grid of option expiries and the underlying lengths
    const std::vector<Real>& timeToExpiries() const { return timeToExpiries_; }
    const std::vector<Real>& underlyingLengths() const { return underlyingLengths_; }
    // calibrated or interpolated model parameters (rows = underlying lengths, cols = option expiries)
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
    const ParamInfo& modelParameters() const { return modelParameters_; }

    struct CalibrationResult {
        QuantLib::Real timeToExpiry;
        QuantLib::Real underlyingLength;
        QuantLib::Real forward;
        std::vector<Real> strikes;           // strikes
        std::vector<Real> marketInput;       // the market input data
        std::vector<Real> calibrationTarget; // the converted data against which the model is calibrated
        std::vector<Real> calibrationResult; // the best model fit
        QuantLib::Real error;                // the calibration error
        bool accepted;                       // true if isInterpolated = false
    };

    const std::vector<CalibrationResult>& calibrationResults() const { return calibrationResults_; }

    QuantLib::ext::shared_ptr<SabrParametricVolatility> clone(
        const std::vector<ParametricVolatility::MarketSmile>& marketSmiles,
        const std::vector<ParameterCalibration>& calibrationTypes) const;

private:
    static constexpr double eps1 = .0000001;
    static constexpr double eps2 = .9999;
    static constexpr double max_nvol_equiv = 0.02;
    static constexpr double max_nu = 2.0;

    void calculate();

    SliceParamInfo defaultModelParameters() const;
    std::vector<Real> getGuess(const SliceParamInfo& params, const std::vector<Real>& randomSeq,
        const Real forward, const Real lognormalShift) const;
    ParametricVolatility::MarketQuoteType preferredOutputQuoteType() const;
    std::vector<Real> direct(const std::vector<Real>& x, const Real forward, const Real lognormalShift) const;
    std::vector<Real> inverse(const std::vector<Real>& y, const Real forward, const Real lognormalShift) const;
    std::vector<Real> evaluateSabr(const std::vector<Real>& params, const Real forward, const Real timeToExpiry,
                                   const Real lognormalShift, const std::vector<Real>& strikes) const;
    std::tuple<std::vector<Real>, Real, Real, QuantLib::Size> calibrateModelParameters(const MarketSmile& marketSmile,
        const SliceParamInfo& params, const std::vector<Real>& convertedMarketQuotes) const;
    std::vector<Real> implyAlpha(const std::vector<Real>& params, const Real forward, const Real tte, const Real shift,
                                 const Real atmVol) const;
    QuantLib::Real getLognormalShift(const ParametricVolatility::MarketSmile& marketSmile) const;
    void populateConvertedMarketQuotes() const;

    // Helpers for residual correction.
    void buildResidualSmiles() const;
    void updateResidualSmiles() const;
    QuantLib::Real residualCorrection(QuantLib::Real timeToExpiry, QuantLib::Real underlyingLength,
        QuantLib::Real strike, QuantLib::Real forward) const;
    std::vector<QuantLib::Real> calculateResiduals(const MarketSmile& marketSmile, const TteUndKey& tteUndKey,
        const std::string& keyStr) const;

    ModelVariant modelVariant_;
    ParamInfo modelParameters_;
    std::map<QuantLib::Real, QuantLib::Real> modelShifts_;
    QuantLib::Size maxCalibrationAttempts_;
    QuantLib::Real exitEarlyErrorThreshold_;
    QuantLib::Real maxAcceptableError_;
    mutable std::map<TteUndKey, std::vector<Real>> calibratedSabrParams_;
    mutable std::map<TteUndKey, Real> lognormalShifts_;
    mutable std::map<TteUndKey, Real> calibrationErrors_;
    mutable std::map<TteUndKey, QuantLib::Size> noOfAttempts_;
    mutable std::vector<Real> underlyingLengths_;
    mutable std::vector<Real> timeToExpiries_;
    mutable std::vector<Real> underlyingLengthsForInterpolation_;
    mutable std::vector<Real> timeToExpiriesForInterpolation_;
    mutable QuantLib::Matrix alpha_;
    mutable QuantLib::Matrix beta_;
    mutable QuantLib::Matrix nu_;
    mutable QuantLib::Matrix rho_;
    mutable QuantLib::Matrix lognormalShift_;
    mutable QuantLib::Matrix calibrationError_;
    mutable QuantLib::Matrix isInterpolated_;
    mutable QuantLib::Matrix numberOfCalibrationAttempts_;
    mutable QuantLib::Interpolation2D alphaInterpolation_;
    mutable QuantLib::Interpolation2D betaInterpolation_;
    mutable QuantLib::Interpolation2D nuInterpolation_;
    mutable QuantLib::Interpolation2D rhoInterpolation_;
    mutable QuantLib::Interpolation2D lognormalShiftInterpolation_;
    mutable std::vector<CalibrationResult> calibrationResults_;
    mutable std::map<TteUndKey, std::vector<Real>> convertedMarketQuotes_;

    // Residuals for each smile and associated interpolation.
    // Implementation is private for now but may be made public in the future if needed elsewhere.
    struct ResidualSmile {
        std::vector<QuantLib::Real> strikeCoordinates_;
        std::vector<QuantLib::Real> residuals_;
        QuantLib::Interpolation interpolation_;

        ResidualSmile(std::vector<QuantLib::Real> strikeCoordinates, std::vector<QuantLib::Real> residuals);

        // Prevent copying or moving.
        ResidualSmile(const ResidualSmile&) = delete;
        ResidualSmile& operator=(const ResidualSmile&) = delete;
        ResidualSmile(ResidualSmile&&) = delete;
        ResidualSmile& operator=(ResidualSmile&&) = delete;

        void build();
        void updateResiduals(const std::vector<QuantLib::Real>& newResiduals);
    };
    mutable std::map<TteUndKey, std::unique_ptr<ResidualSmile>> residualSmiles_;
    QuantLib::Real evaluateResidual(const ResidualSmile& residualSmile, QuantLib::Real strike,
        QuantLib::Real forward) const;
};

} // namespace QuantExt
