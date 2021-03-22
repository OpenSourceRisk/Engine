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

#include <qle/termstructures/futurepricehelper.hpp>

using QuantLib::AcyclicVisitor;
using QuantLib::Date;
using QuantLib::Handle;
using QuantLib::Quote;
using QuantLib::Real;
using QuantLib::Visitor;

namespace QuantExt {

FuturePriceHelper::FuturePriceHelper(const Handle<Quote>& price, const Date& expiryDate)
    : PriceHelper(price) {
    earliestDate_ = expiryDate;
    pillarDate_ = expiryDate;
}

FuturePriceHelper::FuturePriceHelper(Real price, const Date& expiryDate)
    : PriceHelper(price) {
    earliestDate_ = expiryDate;
    pillarDate_ = expiryDate;
}

Real FuturePriceHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_, "FuturePriceHelper term structure not set.");
    return termStructure_->price(pillarDate_);
}

void FuturePriceHelper::accept(AcyclicVisitor& v) {
    if (auto vis = dynamic_cast<Visitor<FuturePriceHelper>*>(&v))
        vis->visit(*this);
    else
        PriceHelper::accept(v);
}

}
