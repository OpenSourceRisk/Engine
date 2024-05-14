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

#include <qle/instruments/commodityforward.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

CommodityForward::CommodityForward(const QuantLib::ext::shared_ptr<CommodityIndex>& index, const QuantLib::Currency& currency,
                                   QuantLib::Position::Type position, QuantLib::Real quantity,
                                   const QuantLib::Date& maturityDate, QuantLib::Real strike, bool physicallySettled,
                                   const Date& paymentDate, const QuantLib::Currency& payCcy, const Date& fixingDate,
                                   const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex)
    : index_(index), currency_(currency), position_(position), quantity_(quantity), maturityDate_(maturityDate),
      strike_(strike), physicallySettled_(physicallySettled), paymentDate_(paymentDate), payCcy_(payCcy),
      fxIndex_(fxIndex), fixingDate_(fixingDate) {

    QL_REQUIRE(quantity_ > 0, "Commodity forward quantity should be positive: " << quantity);
    QL_REQUIRE(strike_ > 0 || close_enough(strike_, 0.0), "Commodity forward strike should be greater than or equal to 0: " << strike);

    if (physicallySettled_) {
        QL_REQUIRE(paymentDate_ == Date(), "CommodityForward: payment date (" << io::iso_date(paymentDate_) <<
                                                                              ") should not be provided for physically settled commodity forwards.");
    }

    if (!physicallySettled_ && paymentDate_ != Date()) {
        QL_REQUIRE(paymentDate_ >= maturityDate_, "CommodityForward: payment date (" << io::iso_date(paymentDate_) <<
                                                                                     ") for a cash settled commodity forward should be on or after the maturity date (" <<
                                                                                     io::iso_date(maturityDate_) << ").");
    }

    if (!physicallySettled_ && fixingDate_ != Date()) {
        QL_REQUIRE(paymentDate_ >= fixingDate_,
                   "CommodityNonDeliverableForward: payment date ("
                       << io::iso_date(paymentDate_) << ") for a commodity NDF should be on or after the fixing date ("
                       << io::iso_date(fixingDate_) << ").");
    }

    registerWith(index_);
}

bool CommodityForward::isExpired() const {
    if (physicallySettled_ || paymentDate_ == Date()) {
        return detail::simple_event(maturityDate_).hasOccurred();
    } else {
        return detail::simple_event(paymentDate_).hasOccurred();
    }
}

void CommodityForward::setupArguments(PricingEngine::arguments* args) const {
    auto* arguments = dynamic_cast<CommodityForward::arguments*>(args);
    QL_REQUIRE(arguments != nullptr, "wrong argument type in CommodityForward");

    arguments->index = index_;
    arguments->currency = currency_;
    arguments->position = position_;
    arguments->quantity = quantity_;
    arguments->maturityDate = maturityDate_;
    arguments->strike = strike_;
    arguments->physicallySettled = physicallySettled_;
    arguments->paymentDate = paymentDate_;
    arguments->payCcy = payCcy_;
    arguments->fixingDate = fixingDate_;
    arguments->fxIndex = fxIndex_;
}

void CommodityForward::arguments::validate() const {
    QL_REQUIRE(quantity > 0, "quantity should be positive: " << quantity);
    QL_REQUIRE(strike > 0 || close_enough(strike, 0.0), "strike should be greater than or equal to 0: " << strike);
}

} // namespace QuantExt
