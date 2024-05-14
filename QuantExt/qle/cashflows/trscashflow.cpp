/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/cashflows/trscashflow.hpp>

namespace QuantExt {

TRSCashFlow::TRSCashFlow(const Date& paymentDate, const Date& fixingStartDate, const Date& fixingEndDate,
                                 const Real notional, const QuantLib::ext::shared_ptr<Index>& index,
                                 const Real initialPrice, const QuantLib::ext::shared_ptr<FxIndex>& fxIndex)
    : paymentDate_(paymentDate), fixingStartDate_(fixingStartDate), fixingEndDate_(fixingEndDate),
      notional_(notional), index_(index), initialPrice_(initialPrice), fxIndex_(fxIndex) {
    //QL_REQUIRE(!index->relative(), "TRSCashFlow: index should not use relative prices");
    registerWith(fxIndex_);
}

Real TRSCashFlow::amount() const { return notional_ * (assetEnd() * fxEnd() - assetStart() * fxStart()); }

Date TRSCashFlow::date() const { return paymentDate_; }

void TRSCashFlow::accept(AcyclicVisitor& v) {
    Visitor<TRSCashFlow>* v1 = dynamic_cast<Visitor<TRSCashFlow>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

Real TRSCashFlow::fxStart() const {
    return fxIndex_ ? fxIndex_->fixing(fxIndex_->fixingCalendar().adjust(fixingStartDate_, Preceding)) : 1.0;
}

Real TRSCashFlow::fxEnd() const {
    return fxIndex_ ? fxIndex_->fixing(fxIndex_->fixingCalendar().adjust(fixingEndDate_, Preceding)) : 1.0;
}

Real TRSCashFlow::assetStart() const {
    return initialPrice_ != Null<Real>() ? initialPrice_ * notional(fixingStartDate_)
                                         : index_->fixing(fixingStartDate_);
}

Real TRSCashFlow::assetEnd() const { return index_->fixing(fixingEndDate_); }

TRSLeg::TRSLeg(const std::vector<Date>& valuationDates, const std::vector<Date>& paymentDates,
                       const Real notional, const QuantLib::ext::shared_ptr<Index>& index,
                       const QuantLib::ext::shared_ptr<FxIndex>& fxIndex)
    : valuationDates_(valuationDates), paymentDates_(paymentDates), notional_(notional), index_(index),
      fxIndex_(fxIndex) {}

TRSLeg& TRSLeg::withInitialPrice(Real initialPrice) {
    initialPrice_ = initialPrice;
    return *this;
}

TRSLeg::operator Leg() const {
    Leg leg;
    Real initialPrice;

    for (Size i = 0; i < valuationDates_.size() - 1; ++i) {
        initialPrice = i == 0 ? initialPrice_ : Null<Real>();
        leg.push_back(QuantLib::ext::make_shared<TRSCashFlow>(paymentDates_[i], valuationDates_[i], valuationDates_[i + 1],
                                                          notional_, index_, initialPrice, fxIndex_));
    }
    return leg;
}

} // namespace QuantExt
