/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/instruments/bondfuture.hpp>
#include <qle/pricingengines/discountingforwardbondengine.hpp>

#include <ql/event.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

BondFuture::BondFuture(const QuantLib::ext::shared_ptr<QuantExt::BondFuturesIndex>& index, const Real contractNotional,
                       const bool isLong, const QuantLib::Date& futureSettlement, const bool physicalSettlement)
    : index_(index), contractNotional_(contractNotional), isLong_(isLong), futureSettlement_(futureSettlement),
      physicalSettlement_(physicalSettlement) {
    registerWith(index_);
}

bool BondFuture::isExpired() const {
    ext::optional<bool> includeToday = Settings::instance().includeTodaysCashFlows();
    Date refDate = Settings::instance().evaluationDate();
    return detail::simple_event(futureSettlement_).hasOccurred(refDate, includeToday);
}

void BondFuture::setupArguments(PricingEngine::arguments* args) const {
    BondFuture::arguments* arguments = dynamic_cast<BondFuture::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type in BondFuture");
    arguments->index = index_;
    arguments->contractNotional = contractNotional_;
    arguments->isLong = isLong_;
    arguments->futureSettlement = futureSettlement_;
    arguments->physicalSettlement = physicalSettlement_;
}

void BondFuture::fetchResults(const PricingEngine::results* r) const { Instrument::fetchResults(r); }

void BondFuture::arguments::validate() const {
    QL_REQUIRE(index, "BondFuturesIndex is null");
    QL_REQUIRE(futureSettlement != Date(), "futureSettlement is null");
}

} // namespace QuantExt
