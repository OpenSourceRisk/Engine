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
                                 const Real bondNotional, const QuantLib::ext::shared_ptr<BondIndex>& bondIndex,
                                 const Real initialPrice, const QuantLib::ext::shared_ptr<FxIndex>& fxIndex)
    : TRSCashFlow(paymentDate, fixingStartDate, fixingEndDate, bondNotional, bondIndex, initialPrice, fxIndex)  {}

const Real BondTRSCashFlow::notional(Date date) const {
    auto bondIndex = ext::dynamic_pointer_cast<BondIndex>(index_);
    QL_REQUIRE(bondIndex, "BondTRSCashFlow::notional index must be a BondIndex");
    return bondIndex->bond()->notional(fixingStartDate_); 
}

void BondTRSCashFlow::setFixingStartDate(QuantLib::Date fixingDate) { 
    QL_REQUIRE(fixingDate < fixingEndDate_, "BondTRSCashFlow fixingStartDate must be before fixingEndDate");    
    fixingStartDate_ = fixingDate; 
}

BondTRSLeg::BondTRSLeg(const std::vector<Date>& valuationDates, const std::vector<Date>& paymentDates,
                       const Real bondNotional, const QuantLib::ext::shared_ptr<BondIndex>& bondIndex,
                       const QuantLib::ext::shared_ptr<FxIndex>& fxIndex)
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
        leg.push_back(QuantLib::ext::make_shared<BondTRSCashFlow>(paymentDates_[i], valuationDates_[i], valuationDates_[i + 1],
                                                          bondNotional_, bondIndex_, initialPrice, fxIndex_));
    }
    return leg;
}

} // namespace QuantExt
