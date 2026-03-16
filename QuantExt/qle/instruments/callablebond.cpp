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

#include <qle/instruments/callablebond.hpp>

namespace QuantExt {

using namespace QuantLib;

CallableBond::CallableBond(Size settlementDays, const Calendar& calendar, const Date& issueDate, const Leg& coupons,
                           const std::vector<CallabilityData>& callData, const std::vector<CallabilityData>& putData,
                           const bool perpetual)
    : Bond(settlementDays, calendar, issueDate, coupons), callData_(callData), putData_(putData),
      perpetual_(perpetual) {}

void CallableBond::setupArguments(PricingEngine::arguments* args) const {
    Bond::setupArguments(args);
    CallableBond::arguments* arguments = dynamic_cast<CallableBond::arguments*>(args);
    QL_REQUIRE(arguments != nullptr, "CallableBond::setupArguments(): wrong argument type");
    arguments->startDate = startDate();
    arguments->notionals = notionals();
    arguments->callData = callData_;
    arguments->putData = putData_;
    arguments->perpetual = perpetual_;
}

void CallableBond::fetchResults(const PricingEngine::results* r) const { Bond::fetchResults(r); }

void CallableBond::arguments::validate() const { Bond::arguments::validate(); }
void CallableBond::results::reset() { Bond::results::reset(); }

} // namespace QuantExt
