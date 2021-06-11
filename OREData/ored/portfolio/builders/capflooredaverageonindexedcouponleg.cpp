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

#include <ored/portfolio/builders/capflooredaverageonindexedcouponleg.hpp>
#include <ored/utilities/log.hpp>

#include <qle/cashflows/blackovernightindexedcouponpricer.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

boost::shared_ptr<FloatingRateCouponPricer>
CapFlooredAverageONIndexedCouponLegEngineBuilder::engineImpl(const Currency& ccy) {

    // Check if we already have a pricer for this ccy
    const string& ccyCode = ccy.code();
    if (engines_.find(ccyCode) == engines_.end()) {
        Handle<YieldTermStructure> yts = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));
        QL_REQUIRE(!yts.empty(), "engineFactory error: yield term structure not found for currency " << ccyCode);
        Handle<OptionletVolatilityStructure> ovs = market_->capFloorVol(ccyCode, configuration(MarketContext::pricing));
        boost::shared_ptr<FloatingRateCouponPricer> pricer =
            boost::make_shared<QuantExt::BlackAverageONIndexedCouponPricer>(ovs);
        engines_[ccyCode] = pricer;
    }

    // Return the cached pricer
    return engines_[ccyCode];
}
} // namespace data
} // namespace ore
