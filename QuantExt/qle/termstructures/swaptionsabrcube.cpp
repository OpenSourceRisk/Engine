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

#include <qle/termstructures/swaptionsabrcube.hpp>

#include <ql/experimental/math/laplaceinterpolation.hpp>

namespace QuantExt {

SwaptionSabrCube::SwaptionSabrCube(
    const Handle<SwaptionVolatilityStructure>& atmVolStructure, const std::vector<Period>& optionTenors,
    const std::vector<Period>& swapTenors, const std::vector<Period>& atmOptionTenors,
    const std::vector<Period>& atmSwapTenors, const std::vector<Spread>& strikeSpreads,
    const std::vector<std::vector<Handle<Quote>>>& volSpreads, const boost::shared_ptr<SwapIndex>& swapIndexBase,
    const boost::shared_ptr<SwapIndex>& shortSwapIndexBase,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
    const boost::optional<QuantLib::VolatilityType> outputVolatilityType,
    const std::map<std::pair<Period, Period>, std::vector<std::pair<Real, bool>>>& initialModelParameters,
    const QuantLib::Size maxCalibrationAttempts, const QuantLib::Real exitEarlyErrorThreshold,
    const QuantLib::Real maxAcceptableError)
    : SwaptionVolatilityCube(atmVolStructure, optionTenors, swapTenors, strikeSpreads, volSpreads, swapIndexBase,
                             shortSwapIndexBase, false),
      atmOptionTenors_(atmOptionTenors), atmSwapTenors_(atmSwapTenors), modelVariant_(modelVariant),
      outputVolatilityType_(outputVolatilityType), initialModelParameters_(initialModelParameters),
      maxCalibrationAttempts_(maxCalibrationAttempts), exitEarlyErrorThreshold_(exitEarlyErrorThreshold),
      maxAcceptableError_(maxAcceptableError) {

    registerWith(atmVolStructure);

    for (auto const& v : volSpreads)
        for (auto const& s : v)
            registerWith(s);
}

namespace {
void laplaceInterpolationWithErrorHandling(Matrix& m, const std::vector<Real>& x, const std::vector<Real>& y) {
    try {
        laplaceInterpolation(m, x, y, 1e-6, 100);
    } catch (const std::exception& e) {
        QL_FAIL("Error during laplaceInterpolation() in SwaptionSabrCube: "
                << e.what() << ", this might be related to the numerical parameters relTol, maxIterMult. Contact dev.");
    }
}
} // namespace

void SwaptionSabrCube::performCalculations() const {

    SwaptionVolatilityCube::performCalculations();

    cache_.clear();

    // build matrices of vol spreads on either given atm options / swap tenors or the smile option / swap tenors

    std::vector<Period> allOptionTenors = atmOptionTenors_.empty() ? optionTenors_ : atmOptionTenors_;
    std::vector<Period> allSwapTenors = atmSwapTenors_.empty() ? swapTenors_ : atmSwapTenors_;

    std::vector<Real> allOptionTimes, allSwapLengths;
    for (auto const& p : allOptionTenors)
        allOptionTimes.push_back(timeFromReference(optionDateFromTenor(p)));
    for (auto const& p : allSwapTenors)
        allSwapLengths.push_back(swapLength(p));

    std::vector<Matrix> interpolatedVolSpreads(strikeSpreads_.size(),
                                               Matrix(allSwapLengths.size(), allOptionTimes.size(), Null<Real>()));

    for (Size k = 0; k < strikeSpreads_.size(); ++k) {
        for (Size i = 0; i < allOptionTenors.size(); ++i) {
            for (Size j = 0; j < allSwapTenors.size(); ++j) {
                Size i0 = std::distance(optionTenors_.begin(),
                                        std::find(optionTenors_.begin(), optionTenors_.end(), allOptionTenors[i]));
                Size j0 = std::distance(swapTenors_.begin(),
                                        std::find(swapTenors_.begin(), swapTenors_.end(), allSwapTenors[j]));
                if (i0 < optionTenors_.size() && j0 < swapTenors_.size()) {
                    interpolatedVolSpreads[k](j, i) = volSpreads_[i0 * swapTenors_.size() + j0][k]->value();
                }
            }
        }
    }

    for (auto& v : interpolatedVolSpreads) {
        laplaceInterpolationWithErrorHandling(v, allOptionTimes, allSwapLengths);
    }

    // build market smiles on the grid

    std::vector<ParametricVolatility::MarketSmile> marketSmiles;
    std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters;
    for (Size i = 0; i < allOptionTenors.size(); ++i) {
        for (Size j = 0; j < allSwapTenors.size(); ++j) {
            Real forward = atmStrike(allOptionTenors[i], allSwapTenors[j]);
            Real sigma = atmVol()->volatility(allOptionTenors[i], allSwapTenors[j], forward);
            std::vector<Real> strikes;
            std::vector<Real> vols;
            for (Size k = 0; k < strikeSpreads_.size(); ++k) {
                strikes.push_back(forward + strikeSpreads_[k]);
                vols.push_back(sigma + interpolatedVolSpreads[k](j, i));
            }
            marketSmiles.push_back(ParametricVolatility::MarketSmile{allOptionTimes[i],
                                                                     allSwapLengths[j],
                                                                     forward,
                                                                     shift(allOptionTenors[i], allSwapTenors[j]),
                                                                     {},
                                                                     strikes,
                                                                     vols});
            if (auto m = initialModelParameters_.find(std::make_pair(allOptionTenors[i], allSwapTenors[j]));
                m != initialModelParameters_.end()) {
                modelParameters[std::make_pair(allOptionTimes[i], allSwapLengths[j])] = m->second;
            }
        }
    }

    parametricVolatility_ = boost::make_shared<SabrParametricVolatility>(
        modelVariant_, marketSmiles, ParametricVolatility::MarketModelType::Black76,
        volatilityType() == QuantLib::Normal ? ParametricVolatility::MarketQuoteType::NormalVolatility
                                             : ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        Handle<YieldTermStructure>(), modelParameters, maxCalibrationAttempts_, exitEarlyErrorThreshold_,
        maxAcceptableError_);
}

boost::shared_ptr<SmileSection> SwaptionSabrCube::smileSectionImpl(Time optionTime, Time swapLength) const {
    calculate();
    if (auto c = cache_.find(std::make_pair(optionTime, swapLength)); c != cache_.end()) {
        return c->second;
    }
    Real forward =
        atmStrike(optionDateFromTime(optionTime), std::max<int>(1, static_cast<int>(swapLength * 12.0 + 0.5)) * Months);
    QuantLib::VolatilityType outVolType = outputVolatilityType_ ? *outputVolatilityType_ : volatilityType();
    auto tmp = boost::make_shared<ParametricVolatilitySmileSection>(
        optionTime, swapLength, forward, parametricVolatility_,
        outVolType == QuantLib::Normal ? ParametricVolatility::MarketQuoteType::NormalVolatility
                                       : ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility);
    cache_[std::make_pair(optionTime, swapLength)] = tmp;
    return tmp;
}

} // namespace QuantExt
