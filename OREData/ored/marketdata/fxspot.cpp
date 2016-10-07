/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/marketdata/fxspot.cpp
    \brief
    \ingroup
*/

#include <ored/marketdata/fxspot.hpp>
#include <ored/marketdata/marketdatum.hpp>

namespace openriskengine {
namespace data {

FXSpot::FXSpot(const Date& asof, FXSpotSpec spec, const Loader& loader) {

    for (auto& md : loader.loadQuotes(asof)) {

        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::FX_SPOT) {

            boost::shared_ptr<FXSpotQuote> q = boost::dynamic_pointer_cast<FXSpotQuote>(md);
            QL_REQUIRE(q, "Failed to cast " << md->name() << " to FXSpotQuote");
            if (q->unitCcy() == spec.unitCcy() && q->ccy() == spec.ccy()) {
                spot_ = q->quote();
                return;
            }
        }
    }
    QL_FAIL("Failed to find a quote for " << spec);
}
}
}
