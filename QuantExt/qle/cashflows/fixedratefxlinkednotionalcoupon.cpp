/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>

using namespace QuantLib;

namespace QuantExt {

FixedRateFXLinkedNotionalCoupon::FixedRateFXLinkedNotionalCoupon(
    const QuantLib::Date& fxFixingDate, QuantLib::Real foreignAmount,
    QuantLib::ext::shared_ptr<FxIndex> fxIndex,
    const QuantLib::ext::shared_ptr<FixedRateCoupon>& underlying)
    : FixedRateCoupon(underlying->date(), foreignAmount, underlying->rate(),
        underlying->dayCounter(), underlying->accrualStartDate(),
        underlying->accrualEndDate(), underlying->referencePeriodStart(),
        underlying->referencePeriodEnd()),
    FXLinked(fxFixingDate, foreignAmount, fxIndex), underlying_(underlying) {
    registerWith(FXLinked::fxIndex());
    registerWith(underlying_);
}

Rate FixedRateFXLinkedNotionalCoupon::nominal() const { 
    return foreignAmount() * fxRate(); 
}

Rate FixedRateFXLinkedNotionalCoupon::rate() const { 
    return underlying_->rate();
}

QuantLib::ext::shared_ptr<FixedRateCoupon> FixedRateFXLinkedNotionalCoupon::underlying() const { 
    return underlying_; 
}

void FixedRateFXLinkedNotionalCoupon::update() { 
    notifyObservers(); 
}

void FixedRateFXLinkedNotionalCoupon::accept(AcyclicVisitor& v) {
    Visitor<FixedRateFXLinkedNotionalCoupon>* v1 = dynamic_cast<Visitor<FixedRateFXLinkedNotionalCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        FixedRateCoupon::accept(v);
}

QuantLib::ext::shared_ptr<FXLinked> FixedRateFXLinkedNotionalCoupon::clone(QuantLib::ext::shared_ptr<FxIndex> fxIndex) {
    return QuantLib::ext::make_shared<FixedRateFXLinkedNotionalCoupon>(fxFixingDate(), foreignAmount(), fxIndex,
        underlying());
}

} //QuantExt
