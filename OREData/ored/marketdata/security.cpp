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

Security::Security(const Date& asof, SecuritySpec spec, const Loader& loader, const CurveConfigurations& curveConfigs) {

    try {
        const QuantLib::ext::shared_ptr<SecurityConfig>& config = curveConfigs.securityConfig(spec.securityID());

        string spreadQuote = config->spreadQuote();
        if (spreadQuote != "" && loader.has(spreadQuote, asof)) {
            QuantLib::ext::shared_ptr<SecuritySpreadQuote> q =
                QuantLib::ext::dynamic_pointer_cast<SecuritySpreadQuote>(loader.get(spreadQuote, asof));
            QL_REQUIRE(q, "Failed to cast " << spreadQuote << " to SecuritySpreadQuote");
            spread_ = q->quote();
        }

        // get recovery quote
        string recoveryQuote = config->recoveryRatesQuote();
        if (recoveryQuote != "" && loader.has(recoveryQuote, asof)) {
            QuantLib::ext::shared_ptr<RecoveryRateQuote> q =
                QuantLib::ext::dynamic_pointer_cast<RecoveryRateQuote>(loader.get(recoveryQuote, asof));
            QL_REQUIRE(q, "Failed to cast " << recoveryQuote << " to RecoveryRateQuote");
            recoveryRate_ = q->quote();
        }

        // get cpr quote
        string cprQuote = config->cprQuote();
        if (cprQuote != "" && loader.has(cprQuote,asof)) {
            QuantLib::ext::shared_ptr<CPRQuote> q = QuantLib::ext::dynamic_pointer_cast<CPRQuote>(loader.get(cprQuote, asof));
            QL_REQUIRE(q, "Failed to cast " << cprQuote << " to CPRQuote");
            cpr_ = q->quote();
        }

        // get price quote
        string priceQuote = config->priceQuote();
        if (priceQuote != "" && loader.has(priceQuote,asof)) {
            QuantLib::ext::shared_ptr<BondPriceQuote> q =
                QuantLib::ext::dynamic_pointer_cast<BondPriceQuote>(loader.get(priceQuote, asof));
            QL_REQUIRE(q, "Failed to cast " << priceQuote << " to BondPriceQuote");
            price_ = q->quote();
        }

    } catch (std::exception& e) {
        QL_FAIL("Security building failed for curve " << spec.curveConfigID() << " on date " << io::iso_date(asof)
                                                      << ": " << e.what());
    } catch (...) {
        QL_FAIL("Security building failed: unknown error");
    }

    return;
}
} // namespace data
} // namespace ore
