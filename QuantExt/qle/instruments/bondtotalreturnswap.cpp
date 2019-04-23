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

#include <ql/event.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/instruments/bondtotalreturnswap.hpp>
#include <qle/pricingengines/discountingbondtrsengine.hpp>

namespace QuantExt {

BondTRS::BondTRS(const boost::shared_ptr<QuantLib::Bond>& underlying, const QuantLib::Leg& fundingLeg,
                 const QuantLib::Schedule& compensationPaymentsSchedule, const Date& bondTRSStartDate,
                 const Date& bondTRSMaturityDate, const bool& longInBond)
    : underlying_(underlying), fundingLeg_(fundingLeg), compensationPaymentsSchedule_(compensationPaymentsSchedule),
      bondTRSStartDate_(bondTRSStartDate), bondTRSMaturityDate_(bondTRSMaturityDate), longInBond_(longInBond) {}

bool BondTRS::isExpired() const { return detail::simple_event(bondTRSMaturityDate_).hasOccurred(); }

void BondTRS::setupArguments(PricingEngine::arguments* args) const {
    BondTRS::arguments* arguments = dynamic_cast<BondTRS::arguments*>(args);
    QL_REQUIRE(arguments != 0, "BondTRS instrument: wrong argument type in bond total return swap");
    arguments->underlying = underlying_;
    arguments->fundingLeg = fundingLeg_;
    arguments->compensationPaymentsSchedule = compensationPaymentsSchedule_;
    arguments->bondTRSMaturityDate = bondTRSMaturityDate_;
    arguments->bondTRSStartDate = bondTRSStartDate_;
    arguments->longInBond = longInBond_;
}

void BondTRS::results::reset() {
    Instrument::results::reset();
    underlyingSpotValue = Null<Real>();
    fundingLegSpotValue = Null<Real>();
    compensationPaymentsSpotValue = Null<Real>();
}

void BondTRS::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
    const BondTRS::results* results = dynamic_cast<const BondTRS::results*>(r);
    QL_REQUIRE(results, "wrong result type");
    underlyingSpotValue_ = results->underlyingSpotValue;
    fundingLegSpotValue_ = results->fundingLegSpotValue;
    compensationPaymentsSpotValue_ = results->compensationPaymentsSpotValue;
}

void BondTRS::arguments::validate() const { QL_REQUIRE(underlying, "bond pointer is null"); }

} // namespace QuantExt
