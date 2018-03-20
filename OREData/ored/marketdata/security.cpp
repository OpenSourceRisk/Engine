/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/bondspread.cpp
    \brief
    \ingroup
*/

#include <ored/marketdata/marketdatum.hpp>
#include <ored/marketdata/security.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

Security::Security(const Date& asof, SecuritySpec spec, const Loader& loader) {

    for (auto& md : loader.loadQuotes(asof)) {

        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::BOND) {

            boost::shared_ptr<SecuritySpreadQuote> q = boost::dynamic_pointer_cast<SecuritySpreadQuote>(md);
            QL_REQUIRE(q, "Failed to cast " << md->name() << " to SecuritySpreadQuote");
            if (q->securityID() == spec.securityID()) {
                spread_ = q->quote();
            }
        }

        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::RECOVERY_RATE) {

            boost::shared_ptr<RecoveryRateQuote> q = boost::dynamic_pointer_cast<RecoveryRateQuote>(md);
            QL_REQUIRE(q, "Failed to cast " << md->name() << " to RecoveryRateQuote");
            if (q->underlyingName() == spec.securityID()) {
                recoveryRate_ = q->quote();
            }
        }

        if (!spread_.empty() && !recoveryRate_.empty())
            return;
    }
    if (recoveryRate_.empty())
        WLOG("No security-specific recovery rate found for " << spec);
    if (spread_.empty())
        QL_FAIL("Failed to find a spread quote for " << spec);

    return;
}
} // namespace data
} // namespace ore
