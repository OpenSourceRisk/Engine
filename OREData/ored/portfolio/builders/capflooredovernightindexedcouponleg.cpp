/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/capflooredovernightindexedcouponleg.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/blackovernightindexedcouponpricer.hpp>
#include <qle/termstructures/proxyoptionletvolatility.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<FloatingRateCouponPricer>
CapFlooredOvernightIndexedCouponLegEngineBuilder::engineImpl(const std::string& index,
                                                             const QuantLib::Period& rateComputationPeriod) {
    std::string ccyCode = parseIborIndex(index)->currency().code();
    Handle<YieldTermStructure> yts = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));
    QL_REQUIRE(!yts.empty(), "engineFactory error: yield term structure not found for currency " << ccyCode);
    Handle<OptionletVolatilityStructure> ovs = market_->capFloorVol(index, configuration(MarketContext::pricing));

    /* if we are pricing an overnight index coupon with rate computation period different from the one on which the
       market vol surface is based, we apply a moneyness adjustment accounting for this difference */
    auto [volIndex, volRateComputationPeriod] =
        market_->capFloorVolIndexBase(index, configuration(MarketContext::pricing));
    if (volIndex == index && volRateComputationPeriod != rateComputationPeriod &&
        volRateComputationPeriod != 0 * Days && rateComputationPeriod != 0 * Days) {
        ovs = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<QuantExt::ProxyOptionletVolatility>(
            ovs, *market_->iborIndex(volIndex, configuration(MarketContext::pricing)),
            *market_->iborIndex(index, configuration(MarketContext::pricing)), volRateComputationPeriod,
            rateComputationPeriod));
    }

    return QuantLib::ext::make_shared<QuantExt::BlackOvernightIndexedCouponPricer>(ovs);
}

string CapFlooredOvernightIndexedCouponLegEngineBuilder::keyImpl(const string& index,
                                                                 const QuantLib::Period& rateComputationPeriod) {
    return index + "_" + ore::data::to_string(rateComputationPeriod);
}

} // namespace data
} // namespace ore
