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

/*! \file swaptionsabrcube.hpp
    \brief SABR Swaption volatility cube
    \ingroup termstructures
*/

#include <qle/termstructures/swaptionsabrcube.hpp>

namespace QuantExt {

SwaptionSabrCube::SwaptionSabrCube(const Handle<SwaptionVolatilityStructure>& atmVolStructure,
                                   const std::vector<Period>& optionTenors, const std::vector<Period>& swapTenors,
                                   const std::vector<Spread>& strikeSpreads,
                                   const std::vector<std::vector<Handle<Quote>>>& volSpreads,
                                   const boost::shared_ptr<SwapIndex>& swapIndexBase,
                                   const boost::shared_ptr<SwapIndex>& shortSwapIndexBase,
                                   QuantExt::SabrParametricVolatility::ModelVariant modelVariant,
                                   boost::optional<QuantLib::VolatilityType> outputVolatilityType)
    : SwaptionVolatilityCube(atmVolStructure, optionTenors, swapTenors, strikeSpreads, volSpreads, swapIndexBase,
                             shortSwapIndexBase, false),
      modelVariant_(modelVariant), outputVolatilityType_(outputVolatilityType) {

    registerWith(atmVolStructure);

    for (auto const& v : volSpreads)
        for (auto const& s : v)
            registerWith(s);
}

void SwaptionSabrCube::performCalculations() const {
    cache_.clear();
    std::vector<ParametricVolatility::MarketSmile> marketSmiles;
    parametricVolatility_ = boost::make_shared<SabrParametricVolatility>(
        modelVariant_, marketSmiles, ParametricVolatility::MarketModelType::Black76,
        volatilityType() == QuantLib::Normal ? ParametricVolatility::MarketQuoteType::NormalVolatility
                                             : ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        Handle<YieldTermStructure>(),
        std::map<std::pair<QuantLib::Real, QuantLib::Real>, std::vector<std::pair<Real, bool>>>());
}

boost::shared_ptr<SmileSection> SwaptionSabrCube::smileSectionImpl(Time optionTime, Time swapLength) const {
    if (auto c = cache_.find(std::make_pair(optionTime, swapLength)); c != cache_.end())
        return c->second;
    auto tmp = boost::make_shared<ParametricVolatilitySmileSection>();//optionTime, swapLength, parametricVolatility_);
    cache_[std::make_pair(optionTime, swapLength)] = tmp;
    return tmp;
}

} // namespace QuantExt
