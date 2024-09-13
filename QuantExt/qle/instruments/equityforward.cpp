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
                             const Real& quantity, const Date& maturityDate, const Real& strike, const Date& payDate,
                             const Currency payCcy,
                             const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex, const Date& fixingDate)
    : name_(name), currency_(currency), longShort_(longShort), quantity_(quantity), maturityDate_(maturityDate),
      strike_(strike), payDate_(payDate), payCcy_(payCcy), fxIndex_(fxIndex), fixingDate_(fixingDate) {
    if (payDate_ == Date()) {
        payDate_ = maturityDate_;
    }
    if (payCcy_ == Currency()) {
        payCcy_ = currency_;
    }
    if(payCcy_ != currency_){
        QL_REQUIRE(fxIndex_ != nullptr, "Payment Currency doesnt match underlying currency, please provide a FxIndex");
        registerWith(fxIndex_);
    }
}

bool EquityForward::isExpired() const {
    //return detail::simple_event(payDate_).hasOccurred();
    ext::optional<bool> includeToday = Settings::instance().includeTodaysCashFlows();
    Date refDate = Settings::instance().evaluationDate();
    return detail::simple_event(payDate_).hasOccurred(refDate, includeToday);
}
  
void EquityForward::setupArguments(PricingEngine::arguments* args) const {
    EquityForward::arguments* arguments = dynamic_cast<EquityForward::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type in equityforward");
    arguments->name = name_;
    arguments->currency = currency_;
    arguments->longShort = longShort_;
    arguments->quantity = quantity_;
    arguments->maturityDate = maturityDate_;
    arguments->strike = strike_;
    arguments->payCurrency = payCcy_;
    arguments->payDate = payDate_;
    arguments->fxIndex = fxIndex_;
    arguments->fixingDate = fixingDate_;
}

void EquityForward::arguments::validate() const {
    QL_REQUIRE(quantity > 0, "quantity should be positive: " << quantity);
    QL_REQUIRE(strike >= 0, "strike should be positive: " << strike);
}
} // namespace QuantExt
