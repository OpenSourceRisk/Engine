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

namespace QuantExt {

SwaptionSabrCube::SwaptionSabrCube(
    const Handle<SwaptionVolatilityStructure>& atmVolStructure, const std::vector<Period>& optionTenors,
    const std::vector<Period>& swapTenors, const std::vector<Spread>& strikeSpreads,
    const std::vector<std::vector<Handle<Quote>>>& volSpreads, const boost::shared_ptr<SwapIndex>& swapIndexBase,
    const boost::shared_ptr<SwapIndex>& shortSwapIndexBase,
    const QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
    const boost::optional<QuantLib::VolatilityType> outputVolatilityType,
    const std::vector<std::pair<Real, bool>>& initialModelParameters, const QuantLib::Size maxCalibrationAttempts,
    const QuantLib::Real exitEarlyErrorThreshold, const QuantLib::Real maxAcceptableError)
    : SwaptionVolatilityCube(atmVolStructure, optionTenors, swapTenors, strikeSpreads, volSpreads, swapIndexBase,
                             shortSwapIndexBase, false),
      modelVariant_(modelVariant), outputVolatilityType_(outputVolatilityType),
      initialModelParameters_(initialModelParameters), maxCalibrationAttempts_(maxCalibrationAttempts),
      exitEarlyErrorThreshold_(exitEarlyErrorThreshold), maxAcceptableError_(maxAcceptableError) {

    registerWith(atmVolStructure);

    for (auto const& v : volSpreads)
        for (auto const& s : v)
            registerWith(s);
}

void SwaptionSabrCube::performCalculations() const {

    SwaptionVolatilityCube::performCalculations();

    cache_.clear();

    std::vector<ParametricVolatility::MarketSmile> marketSmiles;
    std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>> modelParameters;
    for (Size i = 0; i < optionTenors_.size(); ++i) {
        for (Size j = 0; j < swapTenors_.size(); ++j) {
            Real forward = atmStrike(optionTenors_[i], swapTenors_[j]);
            Real sigma = atmVol()->volatility(optionTenors_[i], swapTenors_[j], forward);
            std::vector<Real> strikes;
            std::vector<Real> vols;
            for (Size k = 0; k < strikeSpreads_.size(); ++k) {
                strikes.push_back(forward + strikeSpreads_[k]);
                vols.push_back(sigma + volSpreads_[i * swapTenors_.size() + j][k]->value());
            }
            marketSmiles.push_back(ParametricVolatility::MarketSmile{
                optionTimes_[i], swapLengths_[j], forward, shift(optionTenors_[i], swapTenors_[j]), {}, strikes, vols});
            if (!initialModelParameters_.empty())
                modelParameters[std::make_pair(optionTimes_[i], swapLengths_[j])] = initialModelParameters_;
        }
    }

    // TODO we want to interpolate the market smiles on the finer atm grid, at least as an option

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
