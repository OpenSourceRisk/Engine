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

#include <qle/cashflows/bondtrscashflow.hpp>

namespace QuantExt {

BondTRSCashFlow::BondTRSCashFlow(const Date& paymentDate, const Date& fixingStartDate, const Date& fixingEndDate,
                                 const Real bondNotional, const boost::shared_ptr<BondIndex>& bondIndex,
                                 const Real initialPrice, const boost::shared_ptr<FxIndex>& fxIndex)
    : paymentDate_(paymentDate), fixingStartDate_(fixingStartDate), fixingEndDate_(fixingEndDate),
      bondNotional_(bondNotional), bondIndex_(bondIndex), initialPrice_(initialPrice), fxIndex_(fxIndex) {
    QL_REQUIRE(!bondIndex->relative(), "BondTRSCashFlow: bond index should not use relative prices");
    registerWith(fxIndex_);
}

Real BondTRSCashFlow::amount() const { return bondNotional_ * (bondEnd() * fxEnd() - bondStart() * fxStart()); }

Date BondTRSCashFlow::date() const { return paymentDate_; }

void BondTRSCashFlow::accept(AcyclicVisitor& v) {
    Visitor<BondTRSCashFlow>* v1 = dynamic_cast<Visitor<BondTRSCashFlow>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

Real BondTRSCashFlow::fxStart() const {
    return fxIndex_ ? fxIndex_->fixing(fxIndex_->fixingCalendar().adjust(fixingStartDate_, Preceding)) : 1.0;
}

Real BondTRSCashFlow::fxEnd() const {
    return fxIndex_ ? fxIndex_->fixing(fxIndex_->fixingCalendar().adjust(fixingEndDate_, Preceding)) : 1.0;
}

Real BondTRSCashFlow::bondStart() const {
    return initialPrice_ != Null<Real>() ? initialPrice_ * bondIndex_->bond()->notional(fixingStartDate_)
                                         : bondIndex_->fixing(fixingStartDate_);
}

Real BondTRSCashFlow::bondEnd() const { return bondIndex_->fixing(fixingEndDate_); }

BondTRSLeg::BondTRSLeg(const std::vector<Date>& valuationDates, const std::vector<Date>& paymentDates,
                       const Real bondNotional, const boost::shared_ptr<BondIndex>& bondIndex,
                       const boost::shared_ptr<FxIndex>& fxIndex)
    : valuationDates_(valuationDates), paymentDates_(paymentDates), bondNotional_(bondNotional), bondIndex_(bondIndex),
      fxIndex_(fxIndex) {}

BondTRSLeg& BondTRSLeg::withInitialPrice(Real initialPrice) {
    initialPrice_ = initialPrice;
    return *this;
}

BondTRSLeg::operator Leg() const {
    Leg leg;
    Real initialPrice;

    for (Size i = 0; i < valuationDates_.size() - 1; ++i) {
        initialPrice = i == 0 ? initialPrice_ : Null<Real>();
        leg.push_back(boost::make_shared<BondTRSCashFlow>(paymentDates_[i], valuationDates_[i], valuationDates_[i + 1],
                                                          bondNotional_, bondIndex_, initialPrice, fxIndex_));
    }
    return leg;
}

} // namespace QuantExt
