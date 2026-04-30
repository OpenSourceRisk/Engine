/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <qle/termstructures/blackvolsurfacesvi.hpp>

#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

using namespace QuantLib;

BlackVolatilitySurfaceSvi::BlackVolatilitySurfaceSvi(
    const Date& referenceDate, const std::vector<Date>& dates, const std::vector<std::vector<Real>>& strikes,
    const std::vector<std::vector<Real>>& quotes, const DayCounter& dayCounter, const Calendar& calendar,
    const Handle<Quote>& spot, const Size spotDays, const Calendar spotCalendar,
    const Handle<YieldTermStructure>& domesticTS,
    const Handle<YieldTermStructure>& foreignTS,
    SviParametricVolatility::ModelVariant modelVariant,
    ParametricVolatility::MarketQuoteType inputMarketQuoteType,
    const std::vector<std::vector<QuantLib::Option::Type>>& optionTypes,
    const std::map<std::pair<Real, Real>, std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>&
        modelParameters,
    Size maxCalibrationAttempts, Real exitEarlyErrorThreshold, Real maxAcceptableError)
    : BlackVolatilityTermStructure(referenceDate, calendar, Following, dayCounter), spot_(spot),
      spotDays_(spotDays), spotCalendar_(spotCalendar), domesticTS_(domesticTS), foreignTS_(foreignTS),
      dates_(dates) {

    QL_REQUIRE(dates.size() == strikes.size(),
               "BlackVolatilitySurfaceSvi: dates size (" << dates.size() << ") != strikes size (" << strikes.size()
                                                         << ")");
    QL_REQUIRE(dates.size() == quotes.size(),
               "BlackVolatilitySurfaceSvi: dates size (" << dates.size() << ") != quotes size (" << quotes.size() << ")");
    QL_REQUIRE(optionTypes.empty() || optionTypes.size() == dates.size(),
               "BlackVolatilitySurfaceSvi: optionTypes size (" << optionTypes.size() << ") != dates size ("
                                                               << dates.size() << ")");

    // Build market smiles for each expiry
    std::vector<ParametricVolatility::MarketSmile> marketSmiles;
    for (Size i = 0; i < dates.size(); ++i) {
        QL_REQUIRE(strikes[i].size() == quotes[i].size(),
                   "BlackVolatilitySurfaceSvi: strikes size ("
                       << strikes[i].size() << ") != quotes size (" << quotes[i].size() << ") for expiry "
                       << io::iso_date(dates[i]));
        QL_REQUIRE(strikes[i].size() >= 2,
                   "BlackVolatilitySurfaceSvi: need at least 2 strikes for expiry " << io::iso_date(dates[i]));

        Real t = dayCounter.yearFraction(referenceDate, dates[i]);
        Real fwd = forward(t);

        ParametricVolatility::MarketSmile smile;
        smile.timeToExpiry = t;
        smile.underlyingLength = Null<Real>();
        smile.forward = fwd;
        smile.lognormalShift = 0.0;
        smile.strikes = strikes[i];
        smile.marketQuotes = quotes[i];
        if (!optionTypes.empty()) {
            QL_REQUIRE(optionTypes[i].size() == strikes[i].size(),
                       "BlackVolatilitySurfaceSvi: optionTypes size ("
                           << optionTypes[i].size() << ") != strikes size (" << strikes[i].size() << ") for expiry "
                           << io::iso_date(dates[i]));
            smile.optionTypes = optionTypes[i];
        }
        marketSmiles.push_back(smile);
    }

    // Use domestic curve as discount curve
    Handle<YieldTermStructure> discountCurve(domesticTS_);

    if (modelVariant == SviParametricVolatility::ModelVariant::Gatheral2004SviRaw ||
        modelVariant == SviParametricVolatility::ModelVariant::Gatheral2004SviNatural ||
        modelVariant == SviParametricVolatility::ModelVariant::Gatheral2004SviJw) {
        svi_ = QuantLib::ext::make_shared<SviParametricVolatility>(
            modelVariant, marketSmiles, ParametricVolatility::MarketModelType::Black76,
            inputMarketQuoteType, discountCurve, modelParameters,
            std::map<Real, Real>(), maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
    } else if (modelVariant == SviParametricVolatility::ModelVariant::Gatheral2012SsviHeston ||
               modelVariant == SviParametricVolatility::ModelVariant::Gatheral2012SsviPowerLaw) {
        svi_ = QuantLib::ext::make_shared<SsviParametricVolatility>(
            modelVariant, marketSmiles, ParametricVolatility::MarketModelType::Black76,
            inputMarketQuoteType, discountCurve, modelParameters,
            std::map<Real, Real>(), maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError,
                true, false);
    } else if (modelVariant == SviParametricVolatility::ModelVariant::CorbettaEtAl2019Essvi) {
        svi_ = QuantLib::ext::make_shared<SsviParametricVolatilityRobust>(
            modelVariant, marketSmiles, ParametricVolatility::MarketModelType::Black76,
            inputMarketQuoteType, discountCurve, modelParameters,
            std::map<Real, Real>(), maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError,
                true, false);
    } else if (modelVariant == SviParametricVolatility::ModelVariant::Mingone2022EssviGJ ||
               modelVariant == SviParametricVolatility::ModelVariant::Mingone2022EssviMM) {
        svi_ = QuantLib::ext::make_shared<SsviParametricVolatilityGlobal>(
            modelVariant, marketSmiles, ParametricVolatility::MarketModelType::Black76,
            inputMarketQuoteType, discountCurve, modelParameters,
            std::map<Real, Real>(), maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError,
            true, false);
    } else {
        QL_FAIL("BlackVolatilitySurfaceSvi: model variant " << static_cast<int>(modelVariant) << " is not supported");
    }

    registerWith(spot_);
    registerWith(domesticTS_);
    registerWith(foreignTS_);
    enableExtrapolation();
}

Volatility BlackVolatilitySurfaceSvi::blackVolImpl(Time t, Real strike) const {
    // minimum supported time is 1D
    t = std::max(t, 1.0 / 365.0);
    Real fwd = forward(t);
    return svi_->evaluate(t, Null<Real>(), strike, fwd,
                          ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility);
}

Real BlackVolatilitySurfaceSvi::forward(Time t) const {
    return spot_->value() * foreignTS_->discount(t) / domesticTS_->discount(t);
}

} // namespace QuantExt
