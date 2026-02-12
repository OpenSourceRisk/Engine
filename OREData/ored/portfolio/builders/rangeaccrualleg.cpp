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

#include <ored/portfolio/builders/rangeaccrualleg.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/indexparser.hpp>

#include <ql/cashflows/rangeaccrual.hpp>
#include <ql/termstructures/volatility/flatsmilesection.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<FloatingRateCouponPricer> RangeAccrualLegEngineBuilder::engineImpl(const std::string& index) {

    std::string ccyCode = parseIborIndex(index)->currency().code();
    auto configuration = this->configuration(MarketContext::pricing);

    Real correlation = parseReal(engineParameter("Correlation", {}, false, "1.0"));
    bool withSmile = parseBool(engineParameter("WithSmile", {}, false, "false"));
    bool byCallSpread = parseBool(engineParameter("ByCallSpread", {}, false, "true"));

    Handle<OptionletVolatilityStructure> ovs;
    bool zeroVolatility = parseBool(engineParameter("ZeroVolatility", {}, false, "false"));
    if (zeroVolatility) {
        ovs = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
            0, NullCalendar(), Unadjusted, 0.0, Actual365Fixed(), Normal));
    } else {
        ovs = market_->capFloorVol(index, configuration);
    }

    // We use FlatSmileSections based on the ATM vol from the optionlet vol surface.
    // The RangeAccrualPricerByBgm requires per-coupon smile sections (at expiry and payment dates),
    // but the CachingCouponPricerBuilder pattern provides a single pricer per index.
    // We create smile sections at a representative date (reference date) from the cap/floor vol surface.
    // The actual per-coupon smile sections are set when setCouponPricer dispatches via the visitor pattern.
    Date refDate = ovs->referenceDate();
    Volatility atmVol = ovs->volatility(1.0, 0.03); // 1Y, 3% as representative point
    DayCounter dc = ovs->dayCounter();

    auto smileOnExpiry = QuantLib::ext::make_shared<FlatSmileSection>(refDate, atmVol, dc);
    auto smileOnPayment = QuantLib::ext::make_shared<FlatSmileSection>(refDate, atmVol, dc);

    return QuantLib::ext::make_shared<RangeAccrualPricerByBgm>(correlation, smileOnExpiry, smileOnPayment,
                                                                withSmile, byCallSpread);
}

} // namespace data
} // namespace ore
