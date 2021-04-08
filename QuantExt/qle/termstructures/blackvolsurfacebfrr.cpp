/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/termstructures/blackvolsurfacebfrr.hpp>

namespace QuantExt {

BlackVolatilitySurfaceBFRR::BlackVolatilitySurfaceBFRR(
    Date referenceDate, const std::vector<Date>& dates, const std::vector<Real>& deltas,
    const std::vector<std::vector<Real>>& bfQuotes, const std::vector<std::vector<Real>>& rrQuotes,
    const std::vector<Real>& atmQuotes, const DayCounter& dayCounter, const Calendar& calendar,
    const Handle<Quote>& spot, const Handle<YieldTermStructure>& domesticTS,
    const Handle<YieldTermStructure>& foreignTS, const DeltaVolQuote::DeltaType dt, const DeltaVolQuote::AtmType at,
    const Period& switchTenor, const DeltaVolQuote::DeltaType ltdt, const DeltaVolQuote::AtmType ltat,
    const Option::Type riskReversalInFavorOf, const bool butterflyIsBrokerStyle,
    const SmileInterpolation smileInterpolation)
    : BlackVolatilityTermStructure(referenceDate, calendar, Following, dayCounter), dates_(dates), deltas_(deltas),
      bfQuotes_(bfQuotes), rrQuotes_(rrQuotes), atmQuotes_(atmQuotes), spot_(spot), domesticTS_(domesticTS),
      foreignTS_(foreignTS), dt_(dt), at_(at), switchTenor_(switchTenor), ltdt_(ltdt), ltat_(ltat),
      riskReversalInFavorOf_(riskReversalInFavorOf), butterflyIsBrokerStyle_(butterflyIsBrokerStyle),
      smileInterpolation_(smileInterpolation) {
    registerWith(spot_);
    registerWith(domesticTS_);
    registerWith(foreignTS_);
}

Volatility BlackVolatilitySurfaceBFRR::blackVolImpl(Time t, Real strike) const { return 0.0; }

} // namespace QuantExt
