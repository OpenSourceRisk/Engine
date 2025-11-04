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
                                 const Real bondNotional, const QuantLib::ext::shared_ptr<Index>& index,
                                 const Real initialPrice, const QuantLib::ext::shared_ptr<FxIndex>& fxIndex,
                                 const bool applyFXIndexFixingDays)
    : TRSCashFlow(paymentDate, fixingStartDate, fixingEndDate, bondNotional, index, initialPrice, fxIndex,
                  applyFXIndexFixingDays) {}

const Real BondTRSCashFlow::notional(Date date) const {
    if (auto bondIndex = ext::dynamic_pointer_cast<BondIndex>(index_)) {
        return bondIndex->bond()->notional(fixingStartDate_);
    } else if (auto bondFuturesIndex = ext::dynamic_pointer_cast<BondFuturesIndex>(index_)) {
        return bondFuturesIndex->ctd()->notional(fixingStartDate_);
    } else {
        QL_FAIL("BondTRSCashFlow:: notional index must be a BondIndex or BondFuturesIndex");
    }
}

void BondTRSCashFlow::setFixingStartDate(QuantLib::Date fixingDate) { 
    QL_REQUIRE(fixingDate < fixingEndDate_, "BondTRSCashFlow fixingStartDate must be before fixingEndDate");    
    fixingStartDate_ = fixingDate; 
}

void BondTRSCashFlow::accept(AcyclicVisitor& v) {
    Visitor<BondTRSCashFlow>* v1 = dynamic_cast<Visitor<BondTRSCashFlow>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        TRSCashFlow::accept(v);
}


BondTRSLeg::BondTRSLeg(const std::vector<Date>& valuationDates, const std::vector<Date>& paymentDates,
                       const Real bondNotional, const QuantLib::ext::shared_ptr<Index>& index,
                       const QuantLib::ext::shared_ptr<FxIndex>& fxIndex)
    : valuationDates_(valuationDates), paymentDates_(paymentDates), bondNotional_(bondNotional), index_(index),
      fxIndex_(fxIndex) {}

BondTRSLeg& BondTRSLeg::withInitialPrice(Real initialPrice) {
    initialPrice_ = initialPrice;
    return *this;
}

BondTRSLeg& BondTRSLeg::withApplyFXIndexFixingDays(bool applyFXIndexFixingDays) {
    applyFXIndexFixingDays_ = applyFXIndexFixingDays;
    return *this;
}

BondTRSLeg::operator Leg() const {
    Leg leg;
    Real initialPrice;
   for (Size i = 0; i < valuationDates_.size() - 1; ++i) {
        initialPrice = i == 0 ? initialPrice_ : Null<Real>();
        leg.push_back(QuantLib::ext::make_shared<BondTRSCashFlow>(paymentDates_[i], valuationDates_[i],
                                                                  valuationDates_[i + 1], bondNotional_, index_,
                                                                  initialPrice, fxIndex_, applyFXIndexFixingDays_));
    }
    return leg;
}

} // namespace QuantExt
