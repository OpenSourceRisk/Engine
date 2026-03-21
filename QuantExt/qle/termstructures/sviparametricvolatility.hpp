/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file sviparametricvolatility.hpp
    \brief svi volatility structure
*/

#pragma once

#include <qle/termstructures/parametricvolatility.hpp>

#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/math/optimization/constraint.hpp>

namespace QuantExt {

class SviParametricVolatility : public ParametricVolatility {
public:
    enum class ModelVariant {
        Gatheral2004SviRaw = 0, // SVI with Raw parameterization
        Gatheral2004SviNatural = 1, // SVI with Natural parameterization
        Gatheral2004SviJw = 2, // SVI with Jump-Wings parameterization
        Gatheral2012SsviHeston = 3, // Surface SVI with Heston-like parameterization, arbitrage-free
        Gatheral2012SsviPowerLaw = 4, // Surface SVI with power-law parameterization, arbitrage-free
        HendriksMartini2017EssviFirstPowerLaw = 5, // Extended Surface SVI with first power-law parameterization, arbitrage-free
        HendriksMartini2017EssviSecondPowerLaw = 6, // Extended Surface SVI with second power-law parameterization, arbitrage-free
        CorbettaEtAl2019Essvi = 7, // Extended Surface SVI using robust calibration, arbitrage-free,
        Mingone2022EssviGJ = 8, // Extended Surface SVI with global calibration and Gatheral Jim condition, arbitrage-free
        Mingone2022EssviMM = 9, // Extended Surface SVI with global calibration and Martini Mingone condition, arbitrage-free
    };

    /*! - modelParameters are keyed by (timeToExpiry, underlyingLength) where underlyingLength
          is an optional second dimension (e.g. swap tenor for swaptions, Null<Real>() for equities)
        - each value is a vector of parameter values and whether each is fixed or calibrated
        - modelShift is optional and defines the lognormal shift used within the model (if applicable), if not given, it
          is set to the input market smiles shift
    */
    SviParametricVolatility(
        const ModelVariant modelVariant, const std::vector<MarketSmile>& marketSmiles,
        const MarketModelType marketModelType, const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>&
            modelParameters = {},
        const std::map<QuantLib::Real, QuantLib::Real>& modelShift = {},
        const QuantLib::Size maxCalibrationAttempts = 10, const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05, bool enforceNoArbitrage = false,
        const MarketQuoteType calibrationQuoteType = MarketQuoteType::ShiftedLognormalVolatility);

    virtual QuantLib::Real evaluate(
        const QuantLib::Real timeToExpiry, const QuantLib::Real underlyingLength, const QuantLib::Real strike,
        const QuantLib::Real forward, const MarketQuoteType outputMarketQuoteType,
        const QuantLib::Real outputLognormalShift = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType = QuantLib::ext::nullopt) const override;

    // The calculated grid of option expiries and underlying lengths.
    const std::vector<Real>& timeToExpiries() const { return timeToExpiries_; }
    const std::vector<Real>& underlyingLengths() const { return underlyingLengths_; }

    // calibrated or interpolated model parameters (rows = underlying lengths, cols = option expiries)
    const std::vector<QuantLib::Matrix>& sviParametersMatrices() const { return sviParametersMatrices_; }
    const QuantLib::Matrix& lognormalShift() const { return lognormalShift_; }
    const QuantLib::Matrix& numberOfCalibrationAttempts() const { return numberOfCalibrationAttempts_; }
    // calibration error
    const QuantLib::Matrix& calibrationError() const { return calibrationError_; }
    //indicator whether smile params were interpolated (1) or calibrated (0)
    const QuantLib::Matrix& isInterpolated() const { return isInterpolated_; }
    // Vol RMSE metrics — identical definition for all model variants, comparable across Corbetta/PowerLaw/Mingone etc.
    // All three are normalised by the per-slice maximum of the *market* value in that quote type.
    // Shape: rows = underlyingLengths, cols = timeToExpiries. Null<Real>() for uncalibrated slices.

    // |model_vol - market_vol| / max(market_vol_per_slice), where vol = shifted-lognormal vol
    const QuantLib::Matrix& volRmseShiftedLognormal() const { return volRmseShiftedLognormal_; }
    QuantLib::Real globalVolRmseShiftedLognormal() const { return globalVolRmseShiftedLognormal_; }

    // |model_price - market_price| / max(market_price_per_slice)
    const QuantLib::Matrix& volRmsePrice() const { return volRmsePrice_; }
    QuantLib::Real globalVolRmsePrice() const { return globalVolRmsePrice_; }

    // |model_totalVar - market_totalVar| / max(market_totalVar_per_slice),  totalVar = vol^2 * T
    const QuantLib::Matrix& volRmseTotalVariance() const { return volRmseTotalVariance_; }
    QuantLib::Real globalVolRmseTotalVariance() const { return globalVolRmseTotalVariance_; }

    struct CalibrationResult {
        QuantLib::Real timeToExpiry     = QuantLib::Null<QuantLib::Real>();
        QuantLib::Real underlyingLength = QuantLib::Null<QuantLib::Real>();
        QuantLib::Real forward          = QuantLib::Null<QuantLib::Real>();
        std::vector<Real> strikes;            // strikes
        std::vector<Real> marketInput;        // the market input data
        std::vector<Real> calibrationTarget;  // the converted data against which the model is calibrated
        std::vector<Real> calibrationResult;  // the best model fit
        QuantLib::Real error = QuantLib::Null<QuantLib::Real>();  // the calibration error
        bool accepted        = false;         // true if isInterpolated = false
    };

    const std::vector<CalibrationResult>& calibrationResults() const { return calibrationResults_; }

protected:
    SviParametricVolatility(
        const ModelVariant modelVariant, const std::vector<MarketSmile>& marketSmiles,
        const MarketModelType marketModelType, const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>&
            modelParameters,
        const std::map<QuantLib::Real, QuantLib::Real>& modelShift,
        QuantLib::Size maxCalibrationAttempts, QuantLib::Real exitEarlyErrorThreshold,
        QuantLib::Real maxAcceptableError, bool enforceNoArbitrage, bool,
        MarketQuoteType calibrationQuoteType = MarketQuoteType::ShiftedLognormalVolatility);

    void init() { calculate(); }

    ModelVariant modelVariant_;

    virtual QuantLib::Constraint getCalibrationConstraint(const std::vector<std::pair<Real, ParameterCalibration>>& params,
                                                          bool arbitrageFree) const;
    std::vector<Real> getGuess(const std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>& params,
                               const std::vector<Real>& randomSeq, const Real forward, const Real lognormalShift) const;

    QuantLib::Size expectedModelParametersSize() const;
    static QuantLib::Size expectedModelParametersSize(ModelVariant modelVariant);
    ParametricVolatility::MarketQuoteType preferredOutputQuoteType() const;
    std::tuple<Real, Real, Real, Real, Real> convertToRawSvi(const Real timeToExpiry, const Real underlyingLength) const;
    static std::tuple<Real, Real, Real, Real, Real> convertToRawSvi(const Real timeToExpiry,
                                                                    const std::vector<Real>& params,
                                                                    ModelVariant modelVariant);
    static std::vector<Real> convertFromRawSvi(const Real timeToExpiry, const std::vector<Real>& params,
                                               ModelVariant modelVariant);
    static std::tuple<Real, Real, Real> convertToNaturalSvi(const Real timeToExpiry,
                                                            const std::vector<Real>& params,
                                                            ModelVariant modelVariant);
    Real getAtmQuote(const MarketSmile& marketSmile, Real modelLognormalShift,
                     QuantLib::ext::optional<MarketQuoteType> outputMarketQuoteType = QuantLib::ext::nullopt) const;
    virtual std::tuple<std::vector<Real>, Real, Real, QuantLib::Size>
    calibrateModelParameters(const MarketSmile& marketSmile,
                             const std::vector<std::pair<Real, ParameterCalibration>>& params) const;
    std::vector<std::pair<Real, ParameterCalibration>> defaultModelParameters() const;
    void sanitiseSviParams(std::vector<QuantLib::Matrix>& m);

    mutable std::map<std::pair<Real, Real>, std::vector<Real>> calibratedModelParams_;
    mutable std::map<std::pair<Real, Real>, std::vector<Real>> calibratedSviParams_;
    mutable std::map<std::pair<Real, Real>, Real> lognormalShifts_;
    mutable std::map<std::pair<Real, Real>, Real> calibrationErrors_;
    mutable std::map<std::pair<Real, Real>, QuantLib::Size> noOfAttempts_;

    // Key of previous slice for calendar spread constraint (Corbetta);
    // overridden in SsviParametricVolatilityGlobal to return prevSliceKey_
    virtual QuantLib::ext::optional<std::pair<Real, Real>> prevSliceKey() const { return QuantLib::ext::nullopt; }

    mutable std::vector<Real> underlyingLengths_, timeToExpiries_;
    mutable std::vector<Real> underlyingLengthsForInterpolation_, timeToExpiriesForInterpolation_;
    mutable QuantLib::Matrix lognormalShift_, calibrationError_, isInterpolated_, numberOfCalibrationAttempts_;
    mutable QuantLib::Matrix volRmseShiftedLognormal_, volRmsePrice_, volRmseTotalVariance_;
    mutable QuantLib::Real globalVolRmseShiftedLognormal_ = QuantLib::Null<QuantLib::Real>();
    mutable QuantLib::Real globalVolRmsePrice_            = QuantLib::Null<QuantLib::Real>();
    mutable QuantLib::Real globalVolRmseTotalVariance_    = QuantLib::Null<QuantLib::Real>();
    mutable QuantLib::Interpolation2D lognormalShiftInterpolation_;
    mutable std::vector<QuantLib::Matrix> sviParametersMatrices_;
    mutable std::vector<QuantLib::Interpolation2D> sviParametersInterpolations_;
    mutable std::vector<CalibrationResult> calibrationResults_;

    std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>
        modelParameters_;
    std::map<QuantLib::Real, QuantLib::Real> modelShifts_;
    QuantLib::Size maxCalibrationAttempts_;
    QuantLib::Real exitEarlyErrorThreshold_;
    QuantLib::Real maxAcceptableError_;
    bool enforceNoArbitrage_;
    MarketQuoteType calibrationTargetType_;

    void calculate();
    void computeVolRmseMetrics();
    void validateEvaluateDimensions(Real timeToExpiry, Real underlyingLength) const;

    virtual void setDefaultParameters();
    virtual void calibrate();

    virtual std::vector<Real> evaluateSvi(const std::vector<Real>& params, const Real forward,
                                          const Real timeToExpiry, const Real lognormalShift,
                                          const std::vector<Real>& strikes,
                                          const MarketQuoteType outputMarketQuoteType,
                                          const std::vector<QuantLib::Option::Type>& outputOptionTypes,
                                          const Real outputLognormalShift) const;
};

class SsviParametricVolatility : public SviParametricVolatility {
public:
    SsviParametricVolatility(
        const ModelVariant modelVariant, const std::vector<MarketSmile>& marketSmiles,
        const MarketModelType marketModelType, const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>&
            modelParameters = {},
        const std::map<QuantLib::Real, QuantLib::Real>& modelShift = {},
        const QuantLib::Size maxCalibrationAttempts = 10, const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05, bool enforceNoArbitrage = true,
        bool useInverseVegaWeight = false,
        const MarketQuoteType calibrationQuoteType = MarketQuoteType::ShiftedLognormalVolatility);

    QuantLib::Real evaluate(
        const QuantLib::Real timeToExpiry, const QuantLib::Real underlyingLength, const QuantLib::Real strike,
        const QuantLib::Real forward, const MarketQuoteType outputMarketQuoteType,
        const QuantLib::Real outputLognormalShift = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType = QuantLib::ext::nullopt) const override;

    // calibrate model parameters for a single market smile
    std::tuple<std::vector<Real>, Real, Real, QuantLib::Size>
    calibrateModelParameters(const MarketSmile& marketSmile,
                             const std::vector<std::pair<Real, ParameterCalibration>>& params) const override;

protected:
    bool useInverseVegaWeight_ = false;

    SsviParametricVolatility(
        const ModelVariant modelVariant, const std::vector<MarketSmile>& marketSmiles,
        const MarketModelType marketModelType, const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>&
            modelParameters,
        const std::map<QuantLib::Real, QuantLib::Real>& modelShift,
        const QuantLib::Size maxCalibrationAttempts, const QuantLib::Real exitEarlyErrorThreshold,
        const QuantLib::Real maxAcceptableError, bool enforceNoArbitrage, bool, bool useInverseVegaWeight,
        MarketQuoteType calibrationQuoteType = MarketQuoteType::ShiftedLognormalVolatility);

    std::vector<Real> evaluateSvi(const std::vector<Real>& params, const Real forward,
                                  const Real timeToExpiry, const Real lognormalShift,
                                  const std::vector<Real>& strikes,
                                  const MarketQuoteType outputMarketQuoteType,
                                  const std::vector<QuantLib::Option::Type>& outputOptionTypes,
                                  const Real outputLognormalShift) const override;

    virtual std::tuple<std::vector<Real>, Real, std::vector<Real>, QuantLib::Size>
    calibrateModelParametersGlobal(const std::vector<MarketSmile>& marketSmiles,
                                   const std::vector<std::pair<Real, ParameterCalibration>>& params) const;
    void calibrate() override;

    void initialiseSsviAtmGuesses(
        std::map<std::pair<QuantLib::Real, QuantLib::Real>,
                 std::vector<std::pair<Real, ParameterCalibration>>>& modelParams,
        const std::vector<Real>& modelLognormalShifts) const;
    void storeSsviCalibrationResult(const std::pair<Real, Real>& key, const std::vector<Real>& params,
                                    QuantLib::Real error, QuantLib::Real shift,
                                    QuantLib::Size noOfAttempts);

    Real resolveUnderlyingLength(Real underlyingLength) const;
    Real evaluateSsviSlice(Real timeToExpiry, Real underlyingLength, Real strike, Real forward,
                           Real rho, Real theta, Real phi, MarketQuoteType outputMarketQuoteType,
                           Real outputLognormalShift,
                           const QuantLib::ext::optional<QuantLib::Option::Type>& outputOptionType) const;
};

class SsviParametricVolatilityRobust : public SsviParametricVolatility {
public:
    SsviParametricVolatilityRobust(
        const ModelVariant modelVariant, const std::vector<MarketSmile>& marketSmiles,
        const MarketModelType marketModelType, const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>&
            modelParameters = {},
        const std::map<QuantLib::Real, QuantLib::Real>& modelShift = {},
        const QuantLib::Size maxCalibrationAttempts = 10, const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05, bool enforceNoArbitrage = true,
        bool useInverseVegaWeight = false,
        const MarketQuoteType calibrationQuoteType = MarketQuoteType::ShiftedLognormalVolatility
    );

    QuantLib::Real evaluate(
        const QuantLib::Real timeToExpiry, const QuantLib::Real underlyingLength, const QuantLib::Real strike,
        const QuantLib::Real forward, const MarketQuoteType outputMarketQuoteType,
        const QuantLib::Real outputLognormalShift = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType = QuantLib::ext::nullopt) const override;

protected:
    mutable std::map<std::pair<Real, Real>, std::vector<Real>> corbettaCalibratedModelParams_;
    QuantLib::Constraint getCalibrationConstraint(const std::vector<std::pair<Real, ParameterCalibration>>& params,
                                                  bool arbitrageFree) const override;

    // Per-underlyingLength keys of the previous successful slice for the Corbetta
    // calendar-spread constraint. Using a map (keyed by underlyingLength) ensures
    // the constraint never propagates across different CDS tenor rows.
    mutable std::map<QuantLib::Real, QuantLib::ext::optional<std::pair<Real, Real>>> prevSliceKeys_;
    // Underlying length of the slice currently being calibrated, used by prevSliceKey().
    mutable QuantLib::Real currentSmileUnderlyingLength_ = QuantLib::Null<QuantLib::Real>();
    QuantLib::ext::optional<std::pair<Real, Real>> prevSliceKey() const override {
        auto it = prevSliceKeys_.find(currentSmileUnderlyingLength_);
        return it != prevSliceKeys_.end() ? it->second : QuantLib::ext::nullopt;
    }

    void calibrate() override;

private:
    void storeCorbettaCalibrationResult(const std::pair<Real, Real>& key, const std::vector<Real>& params,
                                        QuantLib::Real error, QuantLib::Real shift,
                                        QuantLib::Size noOfAttempts);
    void calibrateCorbettaFixedRhoSlice(
        const MarketSmile& marketSmile, std::vector<std::pair<Real, ParameterCalibration>>& params,
        const std::pair<Real, Real>& key);
    void calibrateCorbettaRhoSlice(
        const MarketSmile& marketSmile, std::vector<std::pair<Real, ParameterCalibration>>& params,
        const std::pair<Real, Real>& key);
    void initialiseCorbettaAtmGuesses(
        std::map<std::pair<QuantLib::Real, QuantLib::Real>,
                 std::vector<std::pair<Real, ParameterCalibration>>>& modelParams,
        const std::vector<Real>& modelLognormalShifts) const;
    void calibrateCorbettaSlices(
        std::map<std::pair<QuantLib::Real, QuantLib::Real>,
                 std::vector<std::pair<Real, ParameterCalibration>>>& modelParams);

};

class SsviParametricVolatilityGlobal : public SsviParametricVolatility {
public:
    SsviParametricVolatilityGlobal(
        const ModelVariant modelVariant, const std::vector<MarketSmile>& marketSmiles,
        const MarketModelType marketModelType, const MarketQuoteType inputMarketQuoteType,
        const QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, ParameterCalibration>>>&
            modelParameters = {},
        const std::map<QuantLib::Real, QuantLib::Real>& modelShift = {},
        const QuantLib::Size maxCalibrationAttempts = 10, const QuantLib::Real exitEarlyErrorThreshold = 0.005,
        const QuantLib::Real maxAcceptableError = 0.05, bool enforceNoArbitrage = true,
        bool useInverseVegaWeight = false,
        const MarketQuoteType calibrationQuoteType = MarketQuoteType::ShiftedLognormalVolatility
    );

    QuantLib::Real evaluate(
        const QuantLib::Real timeToExpiry, const QuantLib::Real underlyingLength, const QuantLib::Real strike,
        const QuantLib::Real forward, const MarketQuoteType outputMarketQuoteType,
        const QuantLib::Real outputLognormalShift = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::ext::optional<QuantLib::Option::Type> outputOptionType = QuantLib::ext::nullopt) const override;

protected:
    std::tuple<std::vector<Real>, Real, std::vector<Real>, QuantLib::Size>
    calibrateModelParametersGlobal(const std::vector<MarketSmile>& marketSmiles,
                                   const std::vector<std::pair<Real, ParameterCalibration>>& params) const override;
    static std::tuple<std::vector<Real>, std::vector<Real>, std::vector<Real>> convertToNaturalSviGlobal(
        const std::vector<Real>& params, ModelVariant modelVariant);

    // Key of previous slice for calendar spread constraint (Corbetta)
    mutable QuantLib::ext::optional<std::pair<Real, Real>> prevSliceKey_;
    QuantLib::ext::optional<std::pair<Real, Real>> prevSliceKey() const override { return prevSliceKey_; }

    void calibrate() override;
    void setDefaultParameters() override;

private:
    void storeGlobalCalibrationResults(const std::vector<Real>& params, const std::vector<Real>& shifts,
                                       QuantLib::Real error, QuantLib::Size noOfAttempts);

};

} // namespace QuantExt
