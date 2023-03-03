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

#include <qle/instruments/bondoption.hpp>

namespace QuantExt {

bool BondOption::isExpired() const { return putCallSchedule_.back()->hasOccurred(); }

void BondOption::setupArguments(PricingEngine::arguments* args) const {
    BondOption::arguments* arguments = dynamic_cast<BondOption::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type");
    arguments->underlying = underlying_;
    arguments->putCallSchedule = putCallSchedule_;
    arguments->knocksOutOnDefault = knocksOutOnDefault_;
}

void BondOption::arguments::validate() const { QL_REQUIRE(underlying, "null underlying"); }

} // namespace QuantExt
