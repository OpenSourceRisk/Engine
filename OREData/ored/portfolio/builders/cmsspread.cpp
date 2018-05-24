/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/cmsspread.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

boost::shared_ptr<FloatingRateCouponPricer>
CmsSpreadCouponPricerBuilder::engineImpl(const Currency& ccy, const boost::shared_ptr<CmsCouponPricer>& cmsPricer) {

    const string& ccyCode = ccy.code();
    Real correlation;
    if (modelParameters_.find("Correlation_" + ccyCode) != modelParameters_.end()) {
        correlation = parseReal(modelParameters_.at("Correlation_" + ccyCode));
    } else if (modelParameters_.find("Correlation") != modelParameters_.end()) {
        correlation = parseReal(modelParameters_.at("Correlation"));
    } else {
        QL_FAIL("CmsSpreadCouponPricerBuilder(" << ccy << "): correlation parameter required");
    }

    return boost::make_shared<LognormalCmsSpreadPricer>(
        cmsPricer, Handle<Quote>(boost::make_shared<SimpleQuote>(correlation)),
        market_->discountCurve(ccyCode, configuration(MarketContext::pricing)),
        parseReal(engineParameters_.at("IntegrationPoints")));
}

} // namespace data
} // namespace ore
