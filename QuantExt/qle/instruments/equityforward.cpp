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

#include <ql/event.hpp>
#include <qle/instruments/equityforward.hpp>

using namespace QuantLib;

namespace QuantExt {

EquityForward::EquityForward(const std::string& name, const Currency& currency, const Position::Type& longShort,
                             const Real& quantity, const Date& maturityDate, const Real& strike)
    : name_(name), currency_(currency), longShort_(longShort), quantity_(quantity), maturityDate_(maturityDate),
      strike_(strike) {}

bool EquityForward::isExpired() const { return detail::simple_event(maturityDate_).hasOccurred(); }

void EquityForward::setupArguments(PricingEngine::arguments* args) const {
    EquityForward::arguments* arguments = dynamic_cast<EquityForward::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type in equityforward");
    arguments->name = name_;
    arguments->currency = currency_;
    arguments->longShort = longShort_;
    arguments->quantity = quantity_;
    arguments->maturityDate = maturityDate_;
    arguments->strike = strike_;
}

void EquityForward::arguments::validate() const {
    QL_REQUIRE(quantity > 0, "quantity should be positive: " << quantity);
    QL_REQUIRE(strike > 0, "strike should be positive: " << strike);
}
} // namespace QuantExt
