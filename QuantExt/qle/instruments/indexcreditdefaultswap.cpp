/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/instruments/indexcreditdefaultswap.hpp>
#include <qle/pricingengines/midpointindexcdsengine.hpp>

namespace QuantExt {

void IndexCreditDefaultSwap::setupArguments(PricingEngine::arguments* args) const {
    CreditDefaultSwap::setupArguments(args);
    IndexCreditDefaultSwap::arguments* arguments = dynamic_cast<IndexCreditDefaultSwap::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type");
    arguments->underlyingNotionals = underlyingNotionals_;
}

QuantLib::ext::shared_ptr<PricingEngine>
IndexCreditDefaultSwap::buildPricingEngine(const Handle<DefaultProbabilityTermStructure>& p, Real r,
                                           const Handle<YieldTermStructure>& d, PricingModel model) const {
    return QuantLib::ext::make_shared<MidPointIndexCdsEngine>(p, r, d, true);
}

} // namespace QuantExt
