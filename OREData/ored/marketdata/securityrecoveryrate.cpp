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

/*! \file ored/marketdata/securityrecoveryrate.cpp
    \brief
    \ingroup
*/

#include <ored/marketdata/marketdatum.hpp>
#include <ored/marketdata/securityrecoveryrate.hpp>

namespace ore {
namespace data {

SecurityRecoveryRate::SecurityRecoveryRate(const Date& asof, SecurityRecoveryRateSpec spec, const Loader& loader) {

    for (auto& md : loader.loadQuotes(asof)) {

        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::RECOVERY_RATE) {

            boost::shared_ptr<RecoveryRateQuote> q = boost::dynamic_pointer_cast<RecoveryRateQuote>(md);
            QL_REQUIRE(q, "Failed to cast " << md->name() << " to RecoveryRateQuote");
            if (q->underlyingName() == spec.securityID()) {
                recoveryRate_ = q->quote();
                return;
            }
        }
    }
    QL_FAIL("Failed to find a quote for " << spec);
}
} // namespace data
} // namespace ore
