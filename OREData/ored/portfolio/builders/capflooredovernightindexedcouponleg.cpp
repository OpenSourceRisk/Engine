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

#include <qle/cashflows/blackovernightindexedcouponpricer.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

boost::shared_ptr<FloatingRateCouponPricer>
CapFlooredOvernightIndexedCouponLegEngineBuilder::engineImpl(const std::string& index) {
    std::string ccyCode = parseIborIndex(index)->currency().code();
    Handle<YieldTermStructure> yts = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));
    QL_REQUIRE(!yts.empty(), "engineFactory error: yield term structure not found for currency " << ccyCode);
    Handle<OptionletVolatilityStructure> ovs = market_->capFloorVol(ccyCode, configuration(MarketContext::pricing));
    return boost::make_shared<QuantExt::BlackOvernightIndexedCouponPricer>(ovs);
}
} // namespace data
} // namespace ore
