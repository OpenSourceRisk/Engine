/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2013 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <qle/instruments/equityforward.hpp>
#include <ql/event.hpp>

using namespace QuantLib;

namespace QuantExt {

EquityForward::EquityForward(const std::string& name,
                                const Currency& currency,
                                const Position::Type& longShort,
                                const Real& quantity,
                                const Date& maturityDate,
                                const Real& strike)
    : name_(name),
        currency_(currency),
        longShort_(longShort),
        quantity_(quantity),
        maturityDate_(maturityDate),
        strike_(strike)
{}    

bool EquityForward::isExpired() const {
    return detail::simple_event(maturityDate_).hasOccurred();
}

void EquityForward::setupArguments(PricingEngine::arguments* args) const {
    EquityForward::arguments* arguments
        = dynamic_cast<EquityForward::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type in fxforward");
    arguments->name = name_;
    arguments->currency = currency_;
    arguments->longShort = longShort_;
    arguments->quantity = quantity_;
    arguments->maturityDate = maturityDate_;
    arguments->strike = strike_;
}

void EquityForward::arguments::validate() const {
    QL_REQUIRE(quantity > 0, "quantity should be positive: "<<quantity);
    QL_REQUIRE(strike > 0, "strike should be positive: "<<strike);
}
}
