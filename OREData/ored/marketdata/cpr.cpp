/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/cpr.cpp
    \brief
    \ingroup
*/

#include <ored/marketdata/cpr.hpp>
#include <ored/marketdata/marketdatum.hpp>

namespace ore {
namespace data {

CPR::CPR(const Date& asof, CPRSpec spec, const Loader& loader) {

    for (auto& md : loader.loadQuotes(asof)) {

        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::BOND &&
            md->quoteType() == MarketDatum::QuoteType::CPR) {

            boost::shared_ptr<CPRQuote> q = boost::dynamic_pointer_cast<CPRQuote>(md);
            QL_REQUIRE(q, "Failed to cast " << md->name() << " to CPRQuote");
            if (q->securityID() == spec.securityID()) {
                cpr_ = q->quote();
                return;
            }
        }
    }
    QL_FAIL("Failed to find a quote for " << spec);
}
} // namespace data
} // namespace ore
